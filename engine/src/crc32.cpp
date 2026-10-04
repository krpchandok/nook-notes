#include <array>
#include "nook/crc32.hpp"

namespace nook {

static std::array<uint32_t, 256> make_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            if (c & 1) c = 0xEDB88320u ^ (c >> 1);
            else       c = c >> 1;
        }
        table[i] = c;
    }
    return table;
}

uint32_t crc32(const uint8_t* data, size_t len) {
    static const std::array<uint32_t, 256> table = make_table();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t crc32(std::string_view data) {
    return crc32(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

}