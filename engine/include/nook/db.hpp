#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include "nook/manifest.hpp"
#include "nook/memtable.hpp"
#include "nook/options.hpp"
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

    using KeyValue = std::pair<std::string, std::string>;
    std::vector<KeyValue> scan(const std::string& start, const std::string& end) const;
    std::vector<KeyValue> scan_prefix(const std::string& prefix) const;

private:
    void apply(const WalRecord& r);
    void maybe_flush();
    void flush();
    void load_tables();
    void remove_orphans();
    std::string table_path(uint64_t number) const;
    std::string manifest_path() const;

    std::string dir;
    Options opts;
    MemTable memtable;
    std::vector<SSTable> sstables;
    std::optional<Wal> wal;
    Manifest manifest;
};

}