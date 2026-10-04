#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "nook/compaction.hpp"
#include "nook/entry.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

namespace fs = std::filesystem;

class SSTableMergeTest : public ::testing::Test {
protected:
    std::string dir;

    void SetUp() override {
        dir = (fs::temp_directory_path() /
               ("nook_sstable_test_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name())))
                  .string();
        fs::remove_all(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(SSTableMergeTest, MergeTablesNewestWinsAndTombstonesDropped) {
    fs::create_directories(dir);

    nook::MemTable older;
    older.put("a", "old");
    older.put("b", "keep");
    older.put("c", "will be deleted");

    nook::MemTable newer;
    newer.put("a", "new");
    newer.del("c");

    std::string p1 = (fs::path(dir) / "1.sst").string();
    std::string p2 = (fs::path(dir) / "2.sst").string();
    nook::SSTable::write(p1, older);
    nook::SSTable::write(p2, newer);

    std::vector<nook::SSTable> tables;
    tables.push_back(nook::SSTable::open(p1));
    tables.push_back(nook::SSTable::open(p2));

    std::vector<nook::Entry> merged = nook::merge_tables(tables, true);

    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0].key, "a");
    EXPECT_EQ(merged[0].value, "new");
    EXPECT_EQ(merged[1].key, "b");
    EXPECT_EQ(merged[1].value, "keep");
}

TEST_F(SSTableMergeTest, MergeTablesKeepsTombstonesWhenAsked) {
    fs::create_directories(dir);

    nook::MemTable older;
    older.put("c", "value");

    nook::MemTable newer;
    newer.del("c");

    std::string p1 = (fs::path(dir) / "1.sst").string();
    std::string p2 = (fs::path(dir) / "2.sst").string();
    nook::SSTable::write(p1, older);
    nook::SSTable::write(p2, newer);

    std::vector<nook::SSTable> tables;
    tables.push_back(nook::SSTable::open(p1));
    tables.push_back(nook::SSTable::open(p2));

    std::vector<nook::Entry> merged = nook::merge_tables(tables, false);

    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0].key, "c");
    EXPECT_FALSE(merged[0].value.has_value());
}