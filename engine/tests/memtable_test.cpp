#include "nook/memtable.hpp"
#include "nook/lookup.hpp"
#include <gtest/gtest.h>

using nook::MemTable;
using nook::Lookup;
using nook::State;

TEST(MemTable, PutThenLookupFindsValue) {
    MemTable mt;
    mt.put("cat", "meow");

    Lookup result = mt.lookup("cat");
    EXPECT_EQ(result.state, State::Found);
    EXPECT_EQ(result.value, "meow");
}