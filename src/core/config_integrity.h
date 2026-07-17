#pragma once

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

// Canonical ASCII tag "U4CF" stored as an integer. Record CRC is calculated
// over explicit little-endian header fields plus the payload, never over C++
// struct padding, so it is deterministic across supported toolchains.
constexpr uint32_t CONFIG_ENVELOPE_MAGIC = 0x46433455UL;

struct ConfigEnvelope {
    uint32_t magic = CONFIG_ENVELOPE_MAGIC;
    uint16_t schema = 0;
    uint16_t payload_size = 0;
    uint32_t sequence = 0;
    uint32_t crc32 = 0;
};

enum class ConfigValidation : uint8_t {
    VALID = 0,
    NULL_PAYLOAD,
    BAD_MAGIC,
    EMPTY_PAYLOAD,
    TRUNCATED_PAYLOAD,
    CRC_MISMATCH,
    SCHEMA_MISMATCH,
    SIZE_MISMATCH,
};

enum class ConfigSlot : uint8_t {
    NONE = 0,
    A,
    B,
};

enum class ConfigMigrationDecision : uint8_t {
    USE_CURRENT = 0,
    MIGRATE_SUPPORTED,
    RESET_UNSUPPORTED,
    REJECT_CORRUPT,
};

struct ConfigRecordView {
    const ConfigEnvelope* envelope = nullptr;
    const void* payload = nullptr;
    size_t available_payload_bytes = 0;
};

struct ConfigSelection {
    ConfigSlot slot = ConfigSlot::NONE;
    ConfigValidation a_validation = ConfigValidation::NULL_PAYLOAD;
    ConfigValidation b_validation = ConfigValidation::NULL_PAYLOAD;
};

static_assert(std::is_trivially_copyable<ConfigEnvelope>::value,
              "ConfigEnvelope must remain trivially copyable");
static_assert(sizeof(ConfigEnvelope) == 16,
              "ConfigEnvelope binary size is part of the persistence contract");

// IEEE CRC-32 (polynomial 0xEDB88320), compatible with the standard
// "123456789" check value 0xCBF43926.
uint32_t configCrc32(const void* data, size_t size);

uint32_t configRecordCrc(const ConfigEnvelope& envelope,
                         const void* payload);

ConfigEnvelope makeConfigEnvelope(uint16_t schema, uint16_t payload_size,
                                  uint32_t sequence, const void* payload);

// Integrity-only validation is suitable before deciding whether an older
// schema can be migrated.
ConfigValidation validateConfigIntegrity(const ConfigEnvelope& envelope,
                                         const void* payload,
                                         size_t available_payload_bytes);

// Strict validation for directly loading the current in-memory structure.
ConfigValidation validateConfigRecord(const ConfigEnvelope& envelope,
                                      const void* payload,
                                      size_t available_payload_bytes,
                                      uint16_t expected_schema,
                                      uint16_t expected_payload_size);

// Half-range sequence comparison, including uint32_t wrap. Equal or exactly
// half-range-apart values are deliberately not considered newer.
bool configSequenceNewer(uint32_t candidate, uint32_t reference);

ConfigSelection selectNewestConfigRecord(const ConfigRecordView& slot_a,
                                         const ConfigRecordView& slot_b,
                                         uint16_t expected_schema,
                                         uint16_t expected_payload_size);

ConfigMigrationDecision decideConfigMigration(
    ConfigValidation integrity, uint16_t stored_schema,
    uint16_t current_schema, uint16_t oldest_supported_schema);

const char* configValidationName(ConfigValidation validation);
