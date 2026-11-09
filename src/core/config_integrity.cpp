#include "config_integrity.h"

namespace {
uint32_t crcUpdate(uint32_t crc, const uint8_t* data, size_t size) {
    if (data == nullptr && size != 0) return 0;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return crc;
}

uint32_t crcU16Le(uint32_t crc, uint16_t value) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(value & 0xFFU),
        static_cast<uint8_t>((value >> 8U) & 0xFFU),
    };
    return crcUpdate(crc, bytes, sizeof(bytes));
}

uint32_t crcU32Le(uint32_t crc, uint32_t value) {
    const uint8_t bytes[4] = {
        static_cast<uint8_t>(value & 0xFFU),
        static_cast<uint8_t>((value >> 8U) & 0xFFU),
        static_cast<uint8_t>((value >> 16U) & 0xFFU),
        static_cast<uint8_t>((value >> 24U) & 0xFFU),
    };
    return crcUpdate(crc, bytes, sizeof(bytes));
}
}

uint32_t configCrc32(const void* data, size_t size) {
    if (data == nullptr && size != 0) return 0;
    const uint32_t crc = crcUpdate(0xFFFFFFFFUL,
                                   static_cast<const uint8_t*>(data), size);
    return crc ^ 0xFFFFFFFFUL;
}

uint32_t configRecordCrc(const ConfigEnvelope& envelope,
                         const void* payload) {
    if (payload == nullptr && envelope.payload_size != 0) return 0;
    uint32_t crc = 0xFFFFFFFFUL;
    crc = crcU32Le(crc, envelope.magic);
    crc = crcU16Le(crc, envelope.schema);
    crc = crcU16Le(crc, envelope.payload_size);
    crc = crcU32Le(crc, envelope.sequence);
    crc = crcUpdate(crc, static_cast<const uint8_t*>(payload),
                    envelope.payload_size);
    return crc ^ 0xFFFFFFFFUL;
}

ConfigEnvelope makeConfigEnvelope(uint16_t schema, uint16_t payload_size,
                                  uint32_t sequence, const void* payload) {
    ConfigEnvelope envelope;
    envelope.magic = CONFIG_ENVELOPE_MAGIC;
    envelope.schema = schema;
    envelope.payload_size = payload_size;
    envelope.sequence = sequence;
    envelope.crc32 = configRecordCrc(envelope, payload);
    return envelope;
}

ConfigValidation validateConfigIntegrity(const ConfigEnvelope& envelope,
                                         const void* payload,
                                         size_t available_payload_bytes) {
    if (envelope.magic != CONFIG_ENVELOPE_MAGIC)
        return ConfigValidation::BAD_MAGIC;
    if (envelope.payload_size == 0)
        return ConfigValidation::EMPTY_PAYLOAD;
    if (payload == nullptr)
        return ConfigValidation::NULL_PAYLOAD;
    if (available_payload_bytes < envelope.payload_size)
        return ConfigValidation::TRUNCATED_PAYLOAD;
    if (configRecordCrc(envelope, payload) != envelope.crc32)
        return ConfigValidation::CRC_MISMATCH;
    return ConfigValidation::VALID;
}

ConfigValidation validateConfigRecord(const ConfigEnvelope& envelope,
                                      const void* payload,
                                      size_t available_payload_bytes,
                                      uint16_t expected_schema,
                                      uint16_t expected_payload_size) {
    const ConfigValidation integrity = validateConfigIntegrity(
        envelope, payload, available_payload_bytes);
    if (integrity != ConfigValidation::VALID) return integrity;
    if (envelope.schema != expected_schema)
        return ConfigValidation::SCHEMA_MISMATCH;
    if (envelope.payload_size != expected_payload_size)
        return ConfigValidation::SIZE_MISMATCH;
    return ConfigValidation::VALID;
}

bool configSequenceNewer(uint32_t candidate, uint32_t reference) {
    const uint32_t distance = static_cast<uint32_t>(candidate - reference);
    return distance != 0 && distance < 0x80000000UL;
}

ConfigSelection selectNewestConfigRecord(const ConfigRecordView& slot_a,
                                         const ConfigRecordView& slot_b,
                                         uint16_t expected_schema,
                                         uint16_t expected_payload_size) {
    ConfigSelection result;
    result.a_validation = slot_a.envelope == nullptr
        ? ConfigValidation::NULL_PAYLOAD
        : validateConfigRecord(*slot_a.envelope, slot_a.payload,
                               slot_a.available_payload_bytes,
                               expected_schema, expected_payload_size);
    result.b_validation = slot_b.envelope == nullptr
        ? ConfigValidation::NULL_PAYLOAD
        : validateConfigRecord(*slot_b.envelope, slot_b.payload,
                               slot_b.available_payload_bytes,
                               expected_schema, expected_payload_size);

    const bool a_valid = result.a_validation == ConfigValidation::VALID;
    const bool b_valid = result.b_validation == ConfigValidation::VALID;
    if (!a_valid && !b_valid) return result;
    if (a_valid && !b_valid) {
        result.slot = ConfigSlot::A;
        return result;
    }
    if (!a_valid && b_valid) {
        result.slot = ConfigSlot::B;
        return result;
    }

    // Deterministic tie/ambiguous-half-range policy: retain A unless B is
    // unambiguously newer. This avoids boot-to-boot slot oscillation.
    result.slot = configSequenceNewer(slot_b.envelope->sequence,
                                      slot_a.envelope->sequence)
        ? ConfigSlot::B : ConfigSlot::A;
    return result;
}

ConfigMigrationDecision decideConfigMigration(
    ConfigValidation integrity, uint16_t stored_schema,
    uint16_t current_schema, uint16_t oldest_supported_schema) {
    if (integrity != ConfigValidation::VALID)
        return ConfigMigrationDecision::REJECT_CORRUPT;
    if (stored_schema == current_schema)
        return ConfigMigrationDecision::USE_CURRENT;
    if (stored_schema < current_schema &&
        stored_schema >= oldest_supported_schema)
        return ConfigMigrationDecision::MIGRATE_SUPPORTED;
    return ConfigMigrationDecision::RESET_UNSUPPORTED;
}

const char* configValidationName(ConfigValidation validation) {
    switch (validation) {
        case ConfigValidation::VALID:             return "valid";
        case ConfigValidation::NULL_PAYLOAD:      return "null_payload";
        case ConfigValidation::BAD_MAGIC:         return "bad_magic";
        case ConfigValidation::EMPTY_PAYLOAD:     return "empty_payload";
        case ConfigValidation::TRUNCATED_PAYLOAD: return "truncated_payload";
        case ConfigValidation::CRC_MISMATCH:      return "crc_mismatch";
        case ConfigValidation::SCHEMA_MISMATCH:   return "schema_mismatch";
        case ConfigValidation::SIZE_MISMATCH:     return "size_mismatch";
    }
    return "unknown";
}
