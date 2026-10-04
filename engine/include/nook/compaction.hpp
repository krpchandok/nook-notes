#pragma once
#include <vector>
#include "nook/entry.hpp"
#include "nook/sstable.hpp"

namespace nook {

std::vector<Entry> merge_tables(const std::vector<SSTable>& tables, bool drop_tombstones);

}