#pragma once
#include <string>
#include <vector>
#include "nook/wal_record.hpp"

namespace nook {

class Wal {
public:
    struct ReplayResult {
        std::vector<WalRecord> records;
        size_t valid_bytes = 0;
    };

    static ReplayResult replay(const std::string& path);

    explicit Wal(const std::string& path);
    ~Wal();

    Wal(const Wal&) = delete;
    Wal& operator=(const Wal&) = delete;
    Wal(Wal&& other) noexcept;
    Wal& operator=(Wal&& other) noexcept;

    void append(const WalRecord& r);
    void reset();

private:
    int fd_ = -1;
};

}