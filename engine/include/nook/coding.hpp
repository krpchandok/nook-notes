#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace nook {

inline void put_u32(std::string& out, uint32_t x) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((x >> (8 * i)) & 0xFF));
}

inline void put_u64(std::string& out, uint64_t x) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((x >> (8 * i)) & 0xFF));
}

inline uint32_t get_u32(std::string_view s, size_t pos) {
    uint32_t x = 0;
    for (int i = 0; i < 4; ++i) x |= static_cast<uint32_t>(static_cast<uint8_t>(s[pos + i])) << (8 * i);
    return x;
}

inline uint64_t get_u64(std::string_view s, size_t pos) {
    uint64_t x = 0;
    for (int i = 0; i < 8; ++i) x |= static_cast<uint64_t>(static_cast<uint8_t>(s[pos + i])) << (8 * i);
    return x;
}

}