#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include "nook/compaction.hpp"
#include "nook/db.hpp"
#include "nook/file_util.hpp"
#include "nook/memtable.hpp"
#include "nook/sstable.hpp"

namespace fs = std::filesystem;
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

size_t count_sst_files(const std::string& dir) {
    size_t n = 0;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".sst") ++n;
    }
    return n;
}

}

class DBTest : public ::testing::Test {
protected:
    std::string dir;

    void SetUp() override {
        dir = (fs::temp_directory_path() /
               ("nook_db_test_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name())))
                  .string();
        fs::remove_all(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(DBTest, PutThenGet) {
    DB db(dir);
    db.put("cat", "meow");
    EXPECT_EQ(db.get("cat"), "meow");
}

TEST_F(DBTest, MissingKeyIsNullopt) {
    DB db(dir);
    EXPECT_EQ(db.get("nope"), std::nullopt);
}

TEST_F(DBTest, FlushingCreatesSSTables) {
    DB db(dir, tiny());
    fill(db, 50);
    EXPECT_GT(db.num_sstables(), 1u);
}

TEST_F(DBTest, OverwriteAcrossFlushReturnsNewest) {
    DB db(dir, tiny());
    db.put("cat", "meow");
    fill(db, 20);
    ASSERT_GT(db.num_sstables(), 0u);

    db.put("cat", "purr");
    EXPECT_EQ(db.get("cat"), "purr");

    fill(db, 20, "more");
    EXPECT_EQ(db.get("cat"), "purr");
}

TEST_F(DBTest, DeleteShadowsFlushedValue) {
    DB db(dir, tiny());
    db.put("dog", "woof");
    fill(db, 20);
    ASSERT_GT(db.num_sstables(), 0u);

    db.del("dog");
    EXPECT_EQ(db.get("dog"), std::nullopt);

    fill(db, 20, "more");
    EXPECT_EQ(db.get("dog"), std::nullopt);
}

TEST_F(DBTest, CompactKeepsAnswersAndLeavesOneTable) {
    DB db(dir, tiny());
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

TEST_F(DBTest, DataSurvivesRestart) {
    {
        DB db(dir);
        db.put("cat", "meow");
        db.put("dog", "woof");
    }

    DB reopened(dir);
    EXPECT_EQ(reopened.get("cat"), "meow");
    EXPECT_EQ(reopened.get("dog"), "woof");
}

TEST_F(DBTest, DeletesSurviveRestart) {
    {
        DB db(dir);
        db.put("cat", "meow");
        db.del("cat");
    }

    DB reopened(dir);
    EXPECT_EQ(reopened.get("cat"), std::nullopt);
}

TEST_F(DBTest, OverwritesSurviveRestart) {
    {
        DB db(dir);
        db.put("cat", "meow");
        db.put("cat", "purr");
    }

    DB reopened(dir);
    EXPECT_EQ(reopened.get("cat"), "purr");
}

TEST_F(DBTest, FlushedDataSurvivesRestart) {
    {
        DB db(dir, tiny());
        db.put("cat", "meow");
        fill(db, 30);
        ASSERT_GT(db.num_sstables(), 0u);
    }

    DB reopened(dir, tiny());
    EXPECT_EQ(reopened.get("cat"), "meow");
    EXPECT_EQ(reopened.get("filler7"), "xxxxxxxxxx");
}

TEST_F(DBTest, TornWalTailLosesOnlyTheLastWrite) {
    {
        DB db(dir);
        db.put("a", "1");
        db.put("b", "2");
    }
    fs::path wal_path = fs::path(dir) / "wal.log";
    fs::resize_file(wal_path, fs::file_size(wal_path) - 2);

    {
        DB db(dir);
        EXPECT_EQ(db.get("a"), "1");
        EXPECT_EQ(db.get("b"), std::nullopt);
        db.put("c", "3");
    }

    DB reopened(dir);
    EXPECT_EQ(reopened.get("a"), "1");
    EXPECT_EQ(reopened.get("c"), "3");
}

TEST_F(DBTest, FlushWritesSSTFilesAndClearsWal) {
    DB db(dir, tiny());
    fill(db, 50);

    EXPECT_GT(count_sst_files(dir), 1u);
    EXPECT_LT(fs::file_size(fs::path(dir) / "wal.log"), 200u);
}

TEST_F(DBTest, CompactLeavesOneFileAndEmptyWal) {
    DB db(dir, tiny());
    fill(db, 50);
    db.del("filler3");
    db.compact();

    EXPECT_EQ(count_sst_files(dir), 1u);
    EXPECT_EQ(fs::file_size(fs::path(dir) / "wal.log"), 0u);
}

TEST_F(DBTest, DataSurvivesRestartAfterCompaction) {
    {
        DB db(dir, tiny());
        fill(db, 50);
        db.put("cat", "meow");
        db.del("filler3");
        db.compact();
        db.put("after", "compact");
    }

    DB reopened(dir, tiny());
    EXPECT_EQ(reopened.get("cat"), "meow");
    EXPECT_EQ(reopened.get("filler10"), "xxxxxxxxxx");
    EXPECT_EQ(reopened.get("filler3"), std::nullopt);
    EXPECT_EQ(reopened.get("after"), "compact");
}

TEST_F(DBTest, LeftoverTmpFileIsIgnoredAndRemoved) {
    {
        DB db(dir, tiny());
        db.put("cat", "meow");
    }
    std::string tmp = (fs::path(dir) / "000099.sst.tmp").string();
    nook::write_file_atomically(tmp, "half written garbage");

    DB reopened(dir, tiny());
    EXPECT_EQ(reopened.get("cat"), "meow");
    EXPECT_FALSE(fs::exists(tmp));
}

TEST_F(DBTest, NewFilesGetHigherNumbersAfterRestart) {
    {
        DB db(dir, tiny());
        db.put("a", "old");
        fill(db, 30);
    }
    {
        DB db(dir, tiny());
        db.put("a", "new");
        fill(db, 30, "more");
    }

    DB reopened(dir, tiny());
    EXPECT_EQ(reopened.get("a"), "new");
}

TEST_F(DBTest, MergeTablesNewestWinsAndTombstonesDropped) {
    fs::create_directories(dir);
    MemTable older;
    older.put("a", "old");
    older.put("b", "keep");
    older.put("c", "will be deleted");

    MemTable newer;
    newer.put("a", "new");
    newer.del("c");

    std::string p1 = (fs::path(dir) / "1.sst").string();
    std::string p2 = (fs::path(dir) / "2.sst").string();
    SSTable::write(p1, older);
    SSTable::write(p2, newer);

    std::vector<SSTable> tables;
    tables.push_back(SSTable::open(p1));
    tables.push_back(SSTable::open(p2));

    std::vector<Entry> merged = nook::merge_tables(tables, true);

    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0].key, "a");
    EXPECT_EQ(merged[0].value, "new");
    EXPECT_EQ(merged[1].key, "b");
    EXPECT_EQ(merged[1].value, "keep");
}

TEST_F(DBTest, MergeTablesKeepsTombstonesWhenAsked) {
    fs::create_directories(dir);
    MemTable older;
    older.put("c", "value");

    MemTable newer;
    newer.del("c");

    std::string p1 = (fs::path(dir) / "1.sst").string();
    std::string p2 = (fs::path(dir) / "2.sst").string();
    SSTable::write(p1, older);
    SSTable::write(p2, newer);

    std::vector<SSTable> tables;
    tables.push_back(SSTable::open(p1));
    tables.push_back(SSTable::open(p2));

    std::vector<Entry> merged = nook::merge_tables(tables, false);

    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0].key, "c");
    EXPECT_FALSE(merged[0].value.has_value());
}