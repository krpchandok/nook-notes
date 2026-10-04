#include "nook/wal_record.hpp"
#include "nook/coding.hpp"
#include "nook/crc32.hpp"

namespace nook {

std::string encode_record(const WalRecord& r) {
    std::string body;
    body.push_back(static_cast<char>(r.type));
    put_u32(body, static_cast<uint32_t>(r.key.size()));
    put_u32(body, static_cast<uint32_t>(r.value.size()));
    body += r.key;
    body += r.value;

    std::string out;
    put_u32(out, crc32(body));
    out += body;
    return out;
}

std::optional<WalRecord> decode_record(std::string_view data, size_t& offset) {
    if (offset > data.size() || data.size() - offset < kRecordHeaderSize) {
        return std::nullopt;
    }

    uint32_t stored_crc = get_u32(data, offset);
    uint8_t type_byte   = static_cast<uint8_t>(data[offset + 4]);
    uint32_t key_len    = get_u32(data, offset + 5);
    uint32_t val_len    = get_u32(data, offset + 9);

    uint64_t payload = static_cast<uint64_t>(key_len) + val_len;
    if (data.size() - offset - kRecordHeaderSize < payload) {
        return std::nullopt;
    }

    size_t body_start = offset + 4;
    size_t body_len   = (kRecordHeaderSize - 4) + static_cast<size_t>(payload);
    if (crc32(data.substr(body_start, body_len)) != stored_crc) {
        return std::nullopt;
    }

    if (type_byte > static_cast<uint8_t>(RecordType::Delete)) {
        return std::nullopt;
    }

    size_t key_start = offset + kRecordHeaderSize;
    WalRecord r{
        static_cast<RecordType>(type_byte),
        std::string(data.substr(key_start, key_len)),
        std::string(data.substr(key_start + key_len, val_len)),
    };

    offset += kRecordHeaderSize + static_cast<size_t>(payload);
    return r;
}

}