#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "nook/core/note_store.hpp"
#include "nook/core/note.hpp"
#include "nook/db.hpp"

namespace nook::core {

static std::string note_key(const std::string& created_at, const std::string& id) {
    return "note/" + created_at + "/" + id;
}

static std::string tag_key(const std::string& tag, const std::string& created_at, const std::string& id) {
    return "idx/tag/" + tag + "/" + created_at + "/" + id;
}

NoteStore::NoteStore(DB& db) : db(db) {}

Note NoteStore::add(const std::string& text, const std::string& role,
                    const std::vector<std::string>& tags) {
    return add(text, role, tags, now_iso_utc());
}

Note NoteStore::add(const std::string& text, const std::string& role,
                    const std::vector<std::string>& tags, const std::string& created_at) {
    for (const std::string& tag : tags) {
        if (tag.empty() || tag.find('/') != std::string::npos) {
            throw std::invalid_argument("tags can't be empty or contain '/': " + tag);
        }
    }

    Note n;
    n.id = random_id();
    n.created_at = created_at;
    n.text = text;
    n.role = role;
    n.tags = tags;

    db.put(note_key(n.created_at, n.id), to_json(n));
    for (const std::string& tag : n.tags) {
        db.put(tag_key(tag, n.created_at, n.id), "");
    }
    return n;
}

std::optional<Note> NoteStore::get(const std::string& created_at, const std::string& id) const {
    std::optional<std::string> value = db.get(note_key(created_at, id));
    if (!value) return std::nullopt;
    return note_from_json(*value);
}

std::vector<Note> NoteStore::between(const std::string& from, const std::string& to) const {
    std::vector<Note> out;
    for (const auto& [key, value] : db.scan("note/" + from, "note/" + to)) {
        out.push_back(note_from_json(value));
    }
    return out;
}

std::vector<Note> NoteStore::with_tag(const std::string& tag) const {
    std::vector<Note> out;
    std::string prefix = "idx/tag/" + tag + "/";

    for (const auto& [key, value] : db.scan_prefix(prefix)) {
        std::string rest = key.substr(prefix.size());
        std::optional<std::string> note = db.get("note/" + rest);
        if (note) out.push_back(note_from_json(*note));
    }
    return out;
}

}