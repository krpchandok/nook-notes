#include <gtest/gtest.h>
#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "nook/core/note.hpp"
#include "nook/core/note_store.hpp"
#include "nook/db.hpp"

namespace fs = std::filesystem;
using nook::DB;
using nook::core::Note;
using nook::core::NoteStore;

class NoteStoreTest : public ::testing::Test {
protected:
    std::string dir;

    void SetUp() override {
        dir = (fs::temp_directory_path() /
               ("nook_notes_" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name())))
                  .string();
        fs::remove_all(dir);
    }

    void TearDown() override {
        fs::remove_all(dir);
    }
};

TEST_F(NoteStoreTest, AddThenGet) {
    DB db(dir);
    NoteStore notes(db);

    Note added = notes.add("cut load times 90%", "bmo-2026", {"react", "perf"});
    auto fetched = notes.get(added.created_at, added.id);

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->id, added.id);
    EXPECT_EQ(fetched->created_at, added.created_at);
    EXPECT_EQ(fetched->text, "cut load times 90%");
    EXPECT_EQ(fetched->role, "bmo-2026");
    EXPECT_EQ(fetched->tags, (std::vector<std::string>{"react", "perf"}));
}

TEST_F(NoteStoreTest, GetMissingIsNullopt) {
    DB db(dir);
    NoteStore notes(db);
    EXPECT_FALSE(notes.get("2026-01-01T00:00:00Z", "abcdef").has_value());
}

TEST_F(NoteStoreTest, TimestampsAndIdsLookRight) {
    DB db(dir);
    NoteStore notes(db);
    Note n = notes.add("hello", "", {});

    EXPECT_EQ(n.created_at.size(), 20u);
    EXPECT_EQ(n.created_at[4], '-');
    EXPECT_EQ(n.created_at[10], 'T');
    EXPECT_EQ(n.created_at.back(), 'Z');
    EXPECT_EQ(n.id.size(), 6u);
}

TEST_F(NoteStoreTest, IdsAreUnique) {
    std::set<std::string> ids;
    for (int i = 0; i < 1000; ++i) ids.insert(nook::core::random_id());
    EXPECT_GT(ids.size(), 990u);
}

TEST_F(NoteStoreTest, BetweenReturnsRangeInTimeOrder) {
    DB db(dir);
    NoteStore notes(db);
    notes.add("april", "", {}, "2026-04-15T12:00:00Z");
    notes.add("june", "", {}, "2026-06-01T09:00:00Z");
    notes.add("may", "", {}, "2026-05-20T10:00:00Z");
    notes.add("october", "", {}, "2026-10-04T17:00:00Z");

    std::vector<Note> summer = notes.between("2026-05", "2026-09");
    ASSERT_EQ(summer.size(), 2u);
    EXPECT_EQ(summer[0].text, "may");
    EXPECT_EQ(summer[1].text, "june");
}

TEST_F(NoteStoreTest, BetweenPartialDateBoundaries) {
    DB db(dir);
    NoteStore notes(db);
    notes.add("last second of august", "", {}, "2026-08-31T23:59:59Z");
    notes.add("first second of september", "", {}, "2026-09-01T00:00:00Z");

    std::vector<Note> result = notes.between("2026-05", "2026-09");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].text, "last second of august");
}

TEST_F(NoteStoreTest, WithTagReturnsTaggedNotesInTimeOrder) {
    DB db(dir);
    NoteStore notes(db);
    notes.add("cache fix", "bmo-2026", {"react", "perf"}, "2026-07-02T10:00:00Z");
    notes.add("schema", "bmo-2025", {"dynamodb"}, "2025-06-10T10:00:00Z");
    notes.add("dashboard", "bmo-2025", {"react"}, "2025-05-20T10:00:00Z");

    std::vector<Note> react = notes.with_tag("react");
    ASSERT_EQ(react.size(), 2u);
    EXPECT_EQ(react[0].text, "dashboard");
    EXPECT_EQ(react[1].text, "cache fix");

    EXPECT_EQ(notes.with_tag("perf").size(), 1u);
    EXPECT_EQ(notes.with_tag("dynamodb").size(), 1u);
    EXPECT_TRUE(notes.with_tag("cuda").empty());
}

TEST_F(NoteStoreTest, TagPrefixesDontLeak) {
    DB db(dir);
    NoteStore notes(db);
    notes.add("a", "", {"react"}, "2026-01-01T00:00:00Z");
    notes.add("b", "", {"react-native"}, "2026-01-02T00:00:00Z");

    std::vector<Note> react = notes.with_tag("react");
    ASSERT_EQ(react.size(), 1u);
    EXPECT_EQ(react[0].text, "a");
}

TEST_F(NoteStoreTest, NoTagsMeansNoIndexEntries) {
    DB db(dir);
    NoteStore notes(db);
    notes.add("untagged", "", {}, "2026-01-01T00:00:00Z");

    EXPECT_TRUE(db.scan_prefix("idx/").empty());
    EXPECT_EQ(notes.between("2026", "2027").size(), 1u);
}

TEST_F(NoteStoreTest, BadTagsAreRejectedAndNothingIsWritten) {
    DB db(dir);
    NoteStore notes(db);

    EXPECT_THROW(notes.add("x", "", {"c++/perf"}), std::invalid_argument);
    EXPECT_THROW(notes.add("x", "", {""}), std::invalid_argument);
    EXPECT_TRUE(db.scan("", "").empty());
}

TEST_F(NoteStoreTest, NotesSurviveRestart) {
    Note added;
    {
        DB db(dir);
        NoteStore notes(db);
        added = notes.add("remember me", "bmo-2026", {"perf"}, "2026-07-01T12:00:00Z");
    }

    DB db(dir);
    NoteStore notes(db);
    auto fetched = notes.get(added.created_at, added.id);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->text, "remember me");
    EXPECT_EQ(notes.with_tag("perf").size(), 1u);
    EXPECT_EQ(notes.between("2026-07", "2026-08").size(), 1u);
}