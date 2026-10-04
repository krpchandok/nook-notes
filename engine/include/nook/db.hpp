#pragma once
#include <vector>
#include <string>
#include "nook/lookup.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

namespace nook {
struct Options {
    size_t memtable_bytes = 4 * 1024 * 1024;
};

class DB {
    void flush();
    void maybe_flush();

    Options opts;
    MemTable memtable;
    std::vector<SSTable> sstables;   // oldest at the front, newest at the back
public:
    explicit DB(Options opts = {});

    void put(const std::string& key, const std::string& value);
    void del(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;

    void compact();
    size_t num_sstables() const;
};
}