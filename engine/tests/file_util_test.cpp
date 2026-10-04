#include <gtest/gtest.h>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#include "nook/file_util.hpp"

namespace fs = std::filesystem;

class FileUtilTest : public ::testing::Test {
protected:
    fs::path dir;

    void SetUp() override {
        dir = fs::temp_directory_path() /
              ("nook_file_util_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(FileUtilTest, ReadMissingFileIsNullopt) {
    EXPECT_FALSE(nook::read_file((dir / "nope").string()).has_value());
}

TEST_F(FileUtilTest, AtomicWriteThenRead) {
    std::string path = (dir / "data.bin").string();
    nook::write_file_atomically(path, "hello world");

    auto data = nook::read_file(path);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(*data, "hello world");
}

TEST_F(FileUtilTest, AtomicWriteReplacesAndLeavesNoTmp) {
    std::string path = (dir / "data.bin").string();
    nook::write_file_atomically(path, "first version, quite long");
    nook::write_file_atomically(path, "second");

    EXPECT_EQ(*nook::read_file(path), "second");
    EXPECT_FALSE(fs::exists(path + ".tmp"));
}

TEST_F(FileUtilTest, BinaryDataRoundTrips) {
    std::string path = (dir / "bin").string();
    std::string bytes = std::string("a\0b\xFF", 4);
    nook::write_file_atomically(path, bytes);
    EXPECT_EQ(*nook::read_file(path), bytes);
}

TEST_F(FileUtilTest, PreadExactReadsRange) {
    std::string path = (dir / "data.bin").string();
    nook::write_file_atomically(path, "0123456789");

    int fd = ::open(path.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(nook::pread_exact(fd, 3, 4), "3456");
    EXPECT_EQ(nook::pread_exact(fd, 0, 1), "0");
    EXPECT_THROW(nook::pread_exact(fd, 8, 5), std::runtime_error);
    ::close(fd);
}