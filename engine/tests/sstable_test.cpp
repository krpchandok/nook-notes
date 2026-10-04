#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "nook/compaction.hpp"
#include "nook/entry.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

namespace fs = std::filesystem;

class SSTableTest : public ::testing::Test {
protected:
    std::string dir;

    void SetUp() override {
        dir = (fs::temp_directory_path() /
               ("nook_sstable_test_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name())))
                  .string();
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }

    nook::SSTable write_and_open(const nook::MemTable& mt) {
        std::string path = (fs::path(dir) / "scan_test.sst").string();
        nook::SSTable::write(path, mt);
        return nook::SSTable::open(path);
    }
};

TEST_F(SSTableTest, MergeTablesNewestWinsAndTombstonesDropped) {
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

TEST_F(SSTableTest, MergeTablesKeepsTombstonesWhenAsked) {
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

TEST_F(SSTableTest, ScanMatchesBruteForceAcrossBlocks) {
    nook::MemTable mt;
    std::vector<std::string> keys;
    for (int i = 0; i < 500; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "key%04d", i * 2);
        keys.push_back(key);
        if (i % 9 == 0) mt.del(key);
        else mt.put(key, "v" + std::to_string(i));
    }

    nook::SSTable t = write_and_open(mt);

    std::vector<std::pair<std::string, std::string>> ranges = {
        {"", ""},
        {"key0000", "key0001"},
        {"key0031", "key0033"},
        {"key0032", "key0064"},
        {"key0500", "key0700"},
        {"a", "key0010"},
        {"key0990", ""},
        {"key2000", ""},
        {"key0100", "key0100"},
        {"zzz", ""},
        {"", "a"},
    };

    for (const auto& [start, end] : ranges) {
        std::vector<nook::Entry> got = t.scan(start, end);
        std::vector<nook::Entry> want = mt.scan(start, end);
        ASSERT_EQ(got.size(), want.size()) << "[" << start << ", " << end << ")";
        for (size_t i = 0; i < got.size(); ++i) {
            EXPECT_EQ(got[i].key, want[i].key);
            EXPECT_EQ(got[i].value, want[i].value);
        }
    }
}