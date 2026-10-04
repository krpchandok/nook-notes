#include <gtest/gtest.h>
#include <string>
#include "nook/crc32.hpp"
#include "nook/wal_record.hpp"

using nook::RecordType;
using nook::WalRecord;

TEST(Crc32, StandardCheckValue) {
    EXPECT_EQ(nook::crc32("123456789"), 0xCBF43926u);
}

TEST(WalRecord, PutRoundTrip) {
    WalRecord in{RecordType::Put, "cat", "meow"};
    std::string bytes = nook::encode_record(in);
    EXPECT_EQ(bytes.size(), 13u + 3 + 4);

    size_t offset = 0;
    auto out = nook::decode_record(bytes, offset);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->type, RecordType::Put);
    EXPECT_EQ(out->key, "cat");
    EXPECT_EQ(out->value, "meow");
    EXPECT_EQ(offset, bytes.size());
}

TEST(WalRecord, DeleteRoundTrip) {
    WalRecord in{RecordType::Delete, "cat", ""};
    std::string bytes = nook::encode_record(in);

    size_t offset = 0;
    auto out = nook::decode_record(bytes, offset);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->type, RecordType::Delete);
    EXPECT_EQ(out->key, "cat");
    EXPECT_EQ(out->value, "");
}

TEST(WalRecord, BackToBackRecords) {
    std::string bytes = nook::encode_record({RecordType::Put, "a", "1"})
                      + nook::encode_record({RecordType::Delete, "b", ""});

    size_t offset = 0;
    auto first = nook::decode_record(bytes, offset);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->key, "a");

    auto second = nook::decode_record(bytes, offset);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->key, "b");
    EXPECT_EQ(second->type, RecordType::Delete);

    EXPECT_EQ(offset, bytes.size());
    EXPECT_FALSE(nook::decode_record(bytes, offset).has_value());
}

TEST(WalRecord, FlippedByteIsRejected) {
    std::string bytes = nook::encode_record({RecordType::Put, "cat", "meow"});
    bytes[15] ^= 0x01;

    size_t offset = 0;
    EXPECT_FALSE(nook::decode_record(bytes, offset).has_value());
    EXPECT_EQ(offset, 0u);
}

TEST(WalRecord, TruncatedRecordIsRejected) {
    std::string bytes = nook::encode_record({RecordType::Put, "cat", "meow"});
    bytes.pop_back();

    size_t offset = 0;
    EXPECT_FALSE(nook::decode_record(bytes, offset).has_value());
    EXPECT_EQ(offset, 0u);
}

TEST(WalRecord, EmptyInputIsRejected) {
    size_t offset = 0;
    EXPECT_FALSE(nook::decode_record("", offset).has_value());
}

TEST(WalRecord, HugeCorruptedLengthIsRejected) {
    std::string bytes = nook::encode_record({RecordType::Put, "cat", "meow"});
    bytes[5] = '\xFF';
    bytes[6] = '\xFF';
    bytes[7] = '\xFF';
    bytes[8] = '\xFF';

    size_t offset = 0;
    EXPECT_FALSE(nook::decode_record(bytes, offset).has_value());
}