#include <sstream>
#include <stdexcept>
#include "nook/manifest.hpp"
#include "nook/file_util.hpp"

namespace nook {

std::optional<Manifest> Manifest::load(const std::string& path) {
    std::optional<std::string> data = read_file(path);
    if (!data) return std::nullopt;

    std::istringstream in(*data);
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "nook-manifest" || version != 1) {
        throw std::runtime_error("manifest: bad header in " + path);
    }

    Manifest m;
    bool saw_next = false;
    std::string word;
    while (in >> word) {
        uint64_t n = 0;
        if (!(in >> n)) throw std::runtime_error("manifest: missing number after '" + word + "'");

        if (word == "next") {
            m.next_file_number = n;
            saw_next = true;
        } else if (word == "table") {
            m.tables.push_back(n);
        } else {
            throw std::runtime_error("manifest: unknown entry '" + word + "'");
        }
    }

    if (!saw_next) throw std::runtime_error("manifest: missing 'next' in " + path);
    return m;
}

void Manifest::save(const std::string& path) const {
    std::string out = "nook-manifest 1\n";
    out += "next " + std::to_string(next_file_number) + "\n";
    for (uint64_t n : tables) {
        out += "table " + std::to_string(n) + "\n";
    }
    write_file_atomically(path, out);
}

}