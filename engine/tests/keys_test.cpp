#include <gtest/gtest.h>
#include "nook/keys.hpp"

using nook::prefix_successor;

TEST(PrefixSuccessor, BumpsLastByte) {
    EXPECT_EQ(prefix_successor("abc"), "abd");
    EXPECT_EQ(prefix_successor("bullet/"), "bullet0");
}

TEST(PrefixSuccessor, SkipsMaxBytes) {
    EXPECT_EQ(prefix_successor(std::string("a\xFF", 2)), "b");
    EXPECT_EQ(prefix_successor(std::string("ab\xFF\xFF", 4)), "ac");
}

TEST(PrefixSuccessor, AllMaxBytesMeansNoLimit) {
    EXPECT_EQ(prefix_successor(std::string("\xFF\xFF", 2)), "");
    EXPECT_EQ(prefix_successor(""), "");
}