#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nook {

struct Manifest {
    uint64_t next_file_number = 1;
    std::vector<uint64_t> tables;

    static std::optional<Manifest> load(const std::string& path);
    void save(const std::string& path) const;
};

}