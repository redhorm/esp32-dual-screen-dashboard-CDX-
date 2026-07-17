#include <unity.h>

#include <cstring>
#include <type_traits>

#include "core/config_integrity.h"

void setUp() {}
void tearDown() {}

namespace {
struct TestPayload {
    uint32_t marker;
    uint16_t value;
    uint8_t mode;
    uint8_t flags;
};

ConfigRecordView view(const ConfigEnvelope& envelope,
                      const TestPayload& payload) {
    ConfigRecordView result;
    result.envelope = &envelope;
    result.payload = &payload;
    result.available_payload_bytes = sizeof(payload);
    return result;
}
}

static void test_crc32_standard_vector() {
    const char* text = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL,
                            configCrc32(text, std::strlen(text)));
    TEST_ASSERT_EQUAL_HEX32(0, configCrc32(nullptr, 1));
}

static void test_record_round_trip_and_corruption_detection() {
    TestPayload payload{0xA55A1234UL, 42, 2, 7};
    const ConfigEnvelope envelope = makeConfigEnvelope(
        3, sizeof(payload), 17, &payload);
    TEST_ASSERT_EQUAL_HEX32(CONFIG_ENVELOPE_MAGIC, envelope.magic);
    TEST_ASSERT_EQUAL_UINT16(sizeof(payload), envelope.payload_size);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::VALID),
        static_cast<int>(validateConfigRecord(envelope, &payload,
                                              sizeof(payload), 3,
                                              sizeof(payload))));

    payload.value ^= 1U;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::CRC_MISMATCH),
        static_cast<int>(validateConfigIntegrity(envelope, &payload,
                                                 sizeof(payload))));

    payload.value ^= 1U;
    ConfigEnvelope damaged_header = envelope;
    ++damaged_header.sequence;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::CRC_MISMATCH),
        static_cast<int>(validateConfigIntegrity(damaged_header, &payload,
                                                 sizeof(payload))));
}

static void test_wrong_magic_schema_size_and_truncation() {
    const TestPayload payload{0x12345678UL, 9, 1, 0};
    ConfigEnvelope envelope = makeConfigEnvelope(2, sizeof(payload), 1, &payload);

    ConfigEnvelope bad_magic = envelope;
    bad_magic.magic ^= 1U;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::BAD_MAGIC),
        static_cast<int>(validateConfigIntegrity(bad_magic, &payload,
                                                 sizeof(payload))));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::SCHEMA_MISMATCH),
        static_cast<int>(validateConfigRecord(envelope, &payload,
                                              sizeof(payload), 3,
                                              sizeof(payload))));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::SIZE_MISMATCH),
        static_cast<int>(validateConfigRecord(envelope, &payload,
                                              sizeof(payload), 2,
                                              sizeof(payload) + 1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::TRUNCATED_PAYLOAD),
        static_cast<int>(validateConfigIntegrity(envelope, &payload,
                                                 sizeof(payload) - 1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::NULL_PAYLOAD),
        static_cast<int>(validateConfigIntegrity(envelope, nullptr,
                                                 sizeof(payload))));
}

static void test_newest_valid_slot_selection() {
    const TestPayload a_payload{1, 10, 0, 0};
    const TestPayload b_payload{2, 20, 0, 0};
    const ConfigEnvelope a = makeConfigEnvelope(
        4, sizeof(a_payload), 40, &a_payload);
    ConfigEnvelope b = makeConfigEnvelope(
        4, sizeof(b_payload), 41, &b_payload);

    ConfigSelection selected = selectNewestConfigRecord(
        view(a, a_payload), view(b, b_payload), 4, sizeof(TestPayload));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigSlot::B),
                          static_cast<int>(selected.slot));

    b.crc32 ^= 1U;
    selected = selectNewestConfigRecord(
        view(a, a_payload), view(b, b_payload), 4, sizeof(TestPayload));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigSlot::A),
                          static_cast<int>(selected.slot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigValidation::CRC_MISMATCH),
                          static_cast<int>(selected.b_validation));
}

static void test_sequence_and_slot_selection_survive_wrap() {
    TEST_ASSERT_TRUE(configSequenceNewer(1, 0xFFFFFFFEUL));
    TEST_ASSERT_FALSE(configSequenceNewer(0xFFFFFFFEUL, 1));
    TEST_ASSERT_FALSE(configSequenceNewer(7, 7));
    TEST_ASSERT_FALSE(configSequenceNewer(0x80000000UL, 0));

    const TestPayload a_payload{1, 1, 0, 0};
    const TestPayload b_payload{2, 2, 0, 0};
    const ConfigEnvelope a = makeConfigEnvelope(
        1, sizeof(a_payload), 0xFFFFFFFEUL, &a_payload);
    const ConfigEnvelope b = makeConfigEnvelope(
        1, sizeof(b_payload), 1, &b_payload);
    const ConfigSelection selected = selectNewestConfigRecord(
        view(a, a_payload), view(b, b_payload), 1, sizeof(TestPayload));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigSlot::B),
                          static_cast<int>(selected.slot));
}

static void test_migration_decisions() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ConfigMigrationDecision::USE_CURRENT),
        static_cast<int>(decideConfigMigration(
            ConfigValidation::VALID, 5, 5, 3)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigMigrationDecision::MIGRATE_SUPPORTED),
        static_cast<int>(decideConfigMigration(
            ConfigValidation::VALID, 3, 5, 3)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigMigrationDecision::RESET_UNSUPPORTED),
        static_cast<int>(decideConfigMigration(
            ConfigValidation::VALID, 2, 5, 3)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigMigrationDecision::RESET_UNSUPPORTED),
        static_cast<int>(decideConfigMigration(
            ConfigValidation::VALID, 6, 5, 3)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ConfigMigrationDecision::REJECT_CORRUPT),
        static_cast<int>(decideConfigMigration(
            ConfigValidation::CRC_MISMATCH, 4, 5, 3)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_crc32_standard_vector);
    RUN_TEST(test_record_round_trip_and_corruption_detection);
    RUN_TEST(test_wrong_magic_schema_size_and_truncation);
    RUN_TEST(test_newest_valid_slot_selection);
    RUN_TEST(test_sequence_and_slot_selection_survive_wrap);
    RUN_TEST(test_migration_decisions);
    return UNITY_END();
}
