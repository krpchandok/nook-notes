#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nook {

enum class RecordType : uint8_t { Put = 0, Delete = 1 };

struct WalRecord {
    RecordType type;
    std::string key;
    std::string value;
};

constexpr size_t kRecordHeaderSize = 13;

std::string encode_record(const WalRecord& r);
std::optional<WalRecord> decode_record(std::string_view data, size_t& offset);

}