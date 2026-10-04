#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "nook/db.hpp"
#include "nook/compaction.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

using nook::DB;
using nook::Entry;
using nook::MemTable;
using nook::Options;
using nook::SSTable;

namespace {

Options tiny() {
    Options o;
    o.memtable_bytes = 64;
    return o;
}

void fill(DB& db, int n, const std::string& prefix = "filler") {
    for (int i = 0; i < n; ++i) {
        db.put(prefix + std::to_string(i), "xxxxxxxxxx");
    }
}

}

TEST(DB, PutThenGet) {
    DB db;
    db.put("cat", "meow");
    EXPECT_EQ(db.get("cat"), "meow");
}

TEST(DB, MissingKeyIsNullopt) {
    DB db;
    EXPECT_EQ(db.get("nope"), std::nullopt);
}

TEST(DB, FlushingCreatesSSTables) {
    DB db(tiny());
    fill(db, 50);
    EXPECT_GT(db.num_sstables(), 1u);
}

TEST(DB, OverwriteAcrossFlushReturnsNewest) {
    DB db(tiny());
    db.put("cat", "meow");
    fill(db, 20);
    ASSERT_GT(db.num_sstables(), 0u);

    db.put("cat", "purr");
    EXPECT_EQ(db.get("cat"), "purr");

    fill(db, 20, "more");
    EXPECT_EQ(db.get("cat"), "purr");
}

TEST(DB, DeleteShadowsFlushedValue) {
    DB db(tiny());
    db.put("dog", "woof");
    fill(db, 20);
    ASSERT_GT(db.num_sstables(), 0u);

    db.del("dog");
    EXPECT_EQ(db.get("dog"), std::nullopt);

    fill(db, 20, "more");
    EXPECT_EQ(db.get("dog"), std::nullopt);
}

TEST(DB, CompactKeepsAnswersAndLeavesOneTable) {
    DB db(tiny());
    db.put("a", "1");
    fill(db, 15);
    db.put("a", "2");
    db.put("b", "bee");
    fill(db, 15, "more");
    db.del("b");
    db.put("c", "sea");

    std::vector<std::string> keys = {"a", "b", "c", "filler3", "more7", "missing"};
    std::vector<std::optional<std::string>> before;
    for (const auto& k : keys) before.push_back(db.get(k));

    db.compact();

    EXPECT_EQ(db.num_sstables(), 1u);
    for (size_t i = 0; i < keys.size(); ++i) {
        EXPECT_EQ(db.get(keys[i]), before[i]) << "key: " << keys[i];
    }
}

TEST(MergeTables, NewestWinsAndTombstonesDropped) {
    MemTable older;
    older.put("a", "old");
    older.put("b", "keep");
    older.put("c", "will be deleted");

    MemTable newer;
    newer.put("a", "new");
    newer.del("c");

    std::vector<SSTable> tables;
    tables.push_back(SSTable::from_memtable(older));
    tables.push_back(SSTable::from_memtable(newer));

    std::vector<Entry> merged = nook::merge_tables(tables, true);

    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0].key, "a");
    EXPECT_EQ(merged[0].value, "new");
    EXPECT_EQ(merged[1].key, "b");
    EXPECT_EQ(merged[1].value, "keep");
}

TEST(MergeTables, KeepsTombstonesWhenAsked) {
    MemTable older;
    older.put("c", "value");

    MemTable newer;
    newer.del("c");

    std::vector<SSTable> tables;
    tables.push_back(SSTable::from_memtable(older));
    tables.push_back(SSTable::from_memtable(newer));

    std::vector<Entry> merged = nook::merge_tables(tables, false);

    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0].key, "c");
    EXPECT_FALSE(merged[0].value.has_value());
}