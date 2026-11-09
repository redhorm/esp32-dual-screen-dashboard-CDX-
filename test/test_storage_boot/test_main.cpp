#include <unity.h>
#include <cstring>
#include <vector>
#include "core/storage_boot_policy.h"

void setUp() {}
void tearDown() {}

static void test_complete_real_partition_required_before_initialization() {
    constexpr size_t bytes = 0x4f0000;
    size_t next_offset = 0, reads = 0;
    const auto result = StorageBootPolicy::checkErased(bytes, true,
        [&](size_t offset, uint8_t* output, size_t count) {
            TEST_ASSERT_EQUAL_UINT32(next_offset, offset);
            TEST_ASSERT_TRUE(count > 0 && count <= 1024);
            TEST_ASSERT_TRUE(offset + count <= bytes);
            memset(output, 0xff, count);
            next_offset += count;
            ++reads;
            return true;
        });
    TEST_ASSERT_TRUE(result.mayInitialize());
    TEST_ASSERT_EQUAL_UINT32(bytes, result.verified_bytes);
    TEST_ASSERT_EQUAL_UINT32(bytes, next_offset);
    TEST_ASSERT_EQUAL_UINT32(bytes / 1024, reads);
}

static void test_one_programmed_byte_anywhere_preserves_partition() {
    // Including the last byte proves that checking just the superblock or a
    // prefix cannot authorize formatting a damaged filesystem.
    constexpr size_t bytes = 0x4f0000;
    const size_t positions[] = {0, 1, 1023, 1024, 8192, 0x300000, bytes - 1};
    for (const size_t position : positions) {
        const auto result = StorageBootPolicy::checkErased(bytes, true,
            [&](size_t offset, uint8_t* output, size_t count) {
                memset(output, 0xff, count);
                if (position >= offset && position < offset + count)
                    output[position - offset] = 0xfe;  // A single programmed bit.
                return true;
            });
        TEST_ASSERT_FALSE(result.mayInitialize());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(StorageBootPolicy::State::DATA_PRESENT),
                              static_cast<int>(result.state));
        TEST_ASSERT_EQUAL_UINT32(position, result.verified_bytes);
    }
}

static void test_failed_read_never_authorizes_initialization() {
    const size_t failures[] = {0, 1024, 4096};
    for (const size_t failure : failures) {
        const auto result = StorageBootPolicy::checkErased(5000, true,
            [&](size_t offset, uint8_t* output, size_t count) {
                memset(output, 0xff, count);
                return offset != failure;
            });
        TEST_ASSERT_FALSE(result.mayInitialize());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(StorageBootPolicy::State::READ_FAILED),
                              static_cast<int>(result.state));
        TEST_ASSERT_EQUAL_UINT32(failure, result.verified_bytes);
    }
}

static void test_ineligible_or_empty_partition_is_not_read_or_initialized() {
    int reads = 0;
    auto reader = [&](size_t, uint8_t*, size_t) { ++reads; return true; };
    TEST_ASSERT_FALSE(StorageBootPolicy::checkErased(4096, false, reader).mayInitialize());
    TEST_ASSERT_FALSE(StorageBootPolicy::checkErased(0, true, reader).mayInitialize());
    TEST_ASSERT_EQUAL_INT(0, reads);
}

static void test_partial_last_chunk_is_read_exactly_without_rounding() {
    std::vector<uint8_t> data(2049, 0xff);
    size_t last_count = 0, read_bytes = 0;
    auto reader = [&](size_t offset, uint8_t* output, size_t count) {
        TEST_ASSERT_TRUE(offset + count <= data.size());
        memcpy(output, data.data() + offset, count);
        last_count = count;
        read_bytes += count;
        return true;
    };
    TEST_ASSERT_TRUE(StorageBootPolicy::checkErased(data.size(), true, reader).mayInitialize());
    TEST_ASSERT_EQUAL_UINT32(2049, read_bytes);
    TEST_ASSERT_EQUAL_UINT32(1, last_count);
    data.back() = 0;
    TEST_ASSERT_FALSE(StorageBootPolicy::checkErased(data.size(), true, reader).mayInitialize());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_complete_real_partition_required_before_initialization);
    RUN_TEST(test_one_programmed_byte_anywhere_preserves_partition);
    RUN_TEST(test_failed_read_never_authorizes_initialization);
    RUN_TEST(test_ineligible_or_empty_partition_is_not_read_or_initialized);
    RUN_TEST(test_partial_last_chunk_is_read_exactly_without_rounding);
    return UNITY_END();
}
