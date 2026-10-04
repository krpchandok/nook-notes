#pragma once
#include <string>
#include <optional>   
#include <vector>
#include <map>
#include "lookup.hpp"
#include "entry.hpp"

namespace nook {
class MemTable {
    std::map<std::string, std::optional<std::string>> entries;
    size_t approx_bytes = 0;
public:
    void put(const std::string& key, const std::string& value);
    void del(const std::string& key);
    Lookup lookup(const std::string& key) const;

    size_t get_approx_bytes() const;
    bool empty() const;
    void clear();

    // sorted iteration, used later to build an SSTable
    auto begin() const { return entries.cbegin(); }
    auto end() const { return entries.cend(); }

    static size_t entry_size(const std::string& key, const std::optional<std::string>& value);
    std::vector<Entry> scan(const std::string& start, const std::string& end) const;
};
}