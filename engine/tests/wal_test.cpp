#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "nook/wal.hpp"

namespace fs = std::filesystem;
using nook::RecordType;
using nook::Wal;
using nook::WalRecord;

class WalTest : public ::testing::Test {
protected:
    fs::path dir;
    std::string path;

    void SetUp() override {
        dir = fs::temp_directory_path() /
              ("nook_wal_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
               "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(dir);
        fs::create_directories(dir);
        path = (dir / "wal.log").string();
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(WalTest, ReplayOfMissingFileIsEmpty) {
    auto result = Wal::replay(path);
    EXPECT_TRUE(result.records.empty());
    EXPECT_EQ(result.valid_bytes, 0u);
}

TEST_F(WalTest, AppendedRecordsReplayInOrder) {
    {
        Wal wal(path);
        wal.append({RecordType::Put, "a", "1"});
        wal.append({RecordType::Put, "b", "2"});
        wal.append({RecordType::Delete, "a", ""});
    }

    auto result = Wal::replay(path);
    ASSERT_EQ(result.records.size(), 3u);
    EXPECT_EQ(result.records[0].key, "a");
    EXPECT_EQ(result.records[1].key, "b");
    EXPECT_EQ(result.records[2].type, RecordType::Delete);
}

TEST_F(WalTest, ReopeningAppendsAfterExistingRecords) {
    {
        Wal wal(path);
        wal.append({RecordType::Put, "a", "1"});
    }
    {
        Wal wal(path);
        wal.append({RecordType::Put, "b", "2"});
    }

    auto result = Wal::replay(path);
    ASSERT_EQ(result.records.size(), 2u);
    EXPECT_EQ(result.records[0].key, "a");
    EXPECT_EQ(result.records[1].key, "b");
}

TEST_F(WalTest, TornTailIsIgnoredOnReplay) {
    {
        Wal wal(path);
        wal.append({RecordType::Put, "a", "1"});
        wal.append({RecordType::Put, "b", "2"});
    }
    fs::resize_file(path, fs::file_size(path) - 3);

    auto result = Wal::replay(path);
    ASSERT_EQ(result.records.size(), 1u);
    EXPECT_EQ(result.records[0].key, "a");
}

TEST_F(WalTest, TornTailIsTruncatedSoNewWritesSurvive) {
    {
        Wal wal(path);
        wal.append({RecordType::Put, "a", "1"});
        wal.append({RecordType::Put, "b", "2"});
    }
    fs::resize_file(path, fs::file_size(path) - 3);

    {
        Wal wal(path);
        wal.append({RecordType::Put, "c", "3"});
    }

    auto result = Wal::replay(path);
    ASSERT_EQ(result.records.size(), 2u);
    EXPECT_EQ(result.records[0].key, "a");
    EXPECT_EQ(result.records[1].key, "c");
}

TEST_F(WalTest, GarbageAppendedToFileIsIgnored) {
    {
        Wal wal(path);
        wal.append({RecordType::Put, "a", "1"});
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        out << "this is not a record";
    }

    auto result = Wal::replay(path);
    ASSERT_EQ(result.records.size(), 1u);
    EXPECT_EQ(result.records[0].key, "a");
}