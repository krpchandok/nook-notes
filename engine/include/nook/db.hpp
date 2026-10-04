#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "nook/options.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"
#include "nook/wal.hpp"

namespace nook {

class DB {
public:
    explicit DB(const std::string& dir, Options opts = {});

    void put(const std::string& key, const std::string& value);
    void del(const std::string& key);
    std::optional<std::string> get(const std::string& key) const;

    void compact();
    size_t num_sstables() const;

private:
    void apply(const WalRecord& r);
    void maybe_flush();
    void flush();
    void load_tables();
    std::string table_path(uint64_t number) const;

    std::string dir;
    Options opts;
    MemTable memtable;
    std::vector<SSTable> sstables;
    std::optional<Wal> wal;
    uint64_t next_file_number = 1;
};

}