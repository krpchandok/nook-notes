#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nook {

uint32_t crc32(const uint8_t* data, size_t len);
uint32_t crc32(std::string_view data);

}