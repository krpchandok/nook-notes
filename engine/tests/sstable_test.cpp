#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

using nook::Lookup;
using nook::MemTable;
using nook::SSTable;
using nook::State;

namespace {

MemTable sample_memtable() {
    MemTable mt;
    mt.put("banana", "yellow");
    mt.put("apple", "red");
    mt.put("cherry", "dark red");
    mt.del("banana");
    mt.del("durian");
    return mt;
}

}

TEST(SSTable, MatchesMemTableLookups) {
    MemTable mt = sample_memtable();
    SSTable t = SSTable::from_memtable(mt);

    for (const std::string key : {"apple", "banana", "cherry", "durian", "zzz"}) {
        Lookup a = mt.lookup(key);
        Lookup b = t.lookup(key);
        EXPECT_EQ(a.state, b.state) << "key: " << key;
        EXPECT_EQ(a.value, b.value) << "key: " << key;
    }
}

TEST(SSTable, TombstonesSurviveFlush) {
    SSTable t = SSTable::from_memtable(sample_memtable());

    EXPECT_EQ(t.lookup("banana").state, State::Deleted);
    EXPECT_EQ(t.lookup("durian").state, State::Deleted);
}

TEST(SSTable, NeverWrittenKeyIsAbsent) {
    SSTable t = SSTable::from_memtable(sample_memtable());

    EXPECT_EQ(t.lookup("mango").state, State::Absent);
}

TEST(SSTable, IteratesInSortedOrder) {
    SSTable t = SSTable::from_memtable(sample_memtable());

    std::vector<std::string> keys;
    for (const auto& entry : t) {
        keys.push_back(entry.key);
    }

    std::vector<std::string> expected = {"apple", "banana", "cherry", "durian"};
    EXPECT_EQ(keys, expected);
}

TEST(SSTable, SizeCountsTombstones) {
    SSTable t = SSTable::from_memtable(sample_memtable());

    EXPECT_EQ(t.size(), 4u);
}

TEST(SSTable, EdgeLookups) {
    SSTable t = SSTable::from_memtable(sample_memtable());

    Lookup first = t.lookup("apple");
    EXPECT_EQ(first.state, State::Found);
    EXPECT_EQ(first.value, "red");

    EXPECT_EQ(t.lookup("durian").state, State::Deleted);

    EXPECT_EQ(t.lookup("aaa").state, State::Absent);
    EXPECT_EQ(t.lookup("zzz").state, State::Absent);
}

TEST(SSTable, EmptyTableIsAllAbsent) {
    SSTable t = SSTable::from_memtable(MemTable{});

    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.lookup("anything").state, State::Absent);
}