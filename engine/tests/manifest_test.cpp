#include <gtest/gtest.h>
#include <filesystem>
#include "nook/file_util.hpp"
#include "nook/manifest.hpp"

namespace fs = std::filesystem;
using nook::Manifest;

class ManifestTest : public ::testing::Test {
protected:
    fs::path dir;
    std::string path;

    void SetUp() override {
        dir = fs::temp_directory_path() /
              ("nook_manifest_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir);
        fs::create_directories(dir);
        path = (dir / "MANIFEST").string();
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(ManifestTest, MissingFileIsNullopt) {
    EXPECT_FALSE(Manifest::load(path).has_value());
}

TEST_F(ManifestTest, SaveThenLoadRoundTrips) {
    Manifest m;
    m.next_file_number = 7;
    m.tables = {3, 5, 6};
    m.save(path);

    auto loaded = Manifest::load(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->next_file_number, 7u);
    EXPECT_EQ(loaded->tables, (std::vector<uint64_t>{3, 5, 6}));
}

TEST_F(ManifestTest, EmptyTableListRoundTrips) {
    Manifest m;
    m.next_file_number = 1;
    m.save(path);

    auto loaded = Manifest::load(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->tables.empty());
}

TEST_F(ManifestTest, GarbageIsRejected) {
    nook::write_file_atomically(path, "this is not a manifest");
    EXPECT_THROW(Manifest::load(path), std::runtime_error);
}

TEST_F(ManifestTest, UnknownEntryIsRejected) {
    nook::write_file_atomically(path, "nook-manifest 1\nnext 2\nbanana 4\n");
    EXPECT_THROW(Manifest::load(path), std::runtime_error);
}