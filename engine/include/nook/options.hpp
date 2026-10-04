#pragma once
#include <cstddef>

namespace nook {

struct Options {
    size_t memtable_bytes = 4 * 1024 * 1024;
};

}