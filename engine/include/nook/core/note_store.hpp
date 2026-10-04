#pragma once
#include <optional>
#include <string>
#include <vector>
#include "nook/core/note.hpp"
#include "nook/db.hpp"

namespace nook::core {

class NoteStore {
public:
    explicit NoteStore(DB& db);

    Note add(const std::string& text, const std::string& role,
             const std::vector<std::string>& tags);

    Note add(const std::string& text, const std::string& role,
             const std::vector<std::string>& tags, const std::string& created_at);

    std::optional<Note> get(const std::string& created_at, const std::string& id) const;
    std::vector<Note> between(const std::string& from, const std::string& to) const;
    std::vector<Note> with_tag(const std::string& tag) const;

private:
    DB& db;
};

}