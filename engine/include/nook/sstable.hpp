#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "nook/entry.hpp"
#include "nook/lookup.hpp"
#include "nook/memtable.hpp"

namespace nook {

class SSTable {
public:
    static void write(const std::string& path, const std::vector<Entry>& entries);
    static void write(const std::string& path, const MemTable& mt);
    static SSTable open(const std::string& path);

    ~SSTable();
    SSTable(const SSTable&) = delete;
    SSTable& operator=(const SSTable&) = delete;
    SSTable(SSTable&& other) noexcept;
    SSTable& operator=(SSTable&& other) noexcept;

    Lookup lookup(const std::string& key) const;
    std::vector<Entry> read_all() const;
    std::vector<Entry> scan(const std::string& start, const std::string& end) const;
    size_t size() const { return static_cast<size_t>(entry_count_); }
    const std::string& path() const { return path_; }

private:
    struct IndexEntry {
        std::string key;
        uint64_t offset;
    };

    SSTable() = default;

    std::string path_;
    int fd_ = -1;
    uint64_t data_end_ = 0;
    uint64_t entry_count_ = 0;
    std::vector<IndexEntry> index_;
};

}