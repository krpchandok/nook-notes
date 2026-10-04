#include <string>
#include <vector>

namespace nook::core {

struct Note {
    std::string id;
    std::string created_at;   // ISO 8601 UTC, e.g. "2026-10-04T17:27:03Z"
    std::string text;
    std::string role;         // e.g. "bmo-2026"; empty if none
    std::vector<std::string> tags;
};

std::string to_json(const Note& n);
Note note_from_json(const std::string& s);

std::string now_iso_utc();
std::string random_id();   // 6 lowercase hex characters

}