#pragma once
#include <string>
#include <optional>
#include <vector>
#include "nook/lookup.hpp"
#include "nook/memtable.hpp"
#include "nook/entry.hpp"

namespace nook {

class SSTable {
    std::vector<Entry> entries;
public:
    explicit SSTable(std::vector<Entry> entries);

    static SSTable from_memtable(const MemTable& mt);

    Lookup lookup(const std::string& key) const;
    size_t size() const;

    auto begin() const { return entries.begin(); }
    auto end() const { return entries.end(); }
};
}