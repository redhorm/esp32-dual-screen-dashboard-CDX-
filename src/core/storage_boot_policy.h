#pragma once

#include <stddef.h>
#include <stdint.h>

namespace StorageBootPolicy {
constexpr size_t kScanChunkBytes = 1024;

enum class State : uint8_t { ERASED, DATA_PRESENT, READ_FAILED, INELIGIBLE };

struct Check {
    State state;
    size_t verified_bytes;
    bool mayInitialize() const { return state == State::ERASED; }
};

// A failed mount says nothing about whether user files exist. Only a complete,
// successful read of an eligible partition containing exclusively erased bytes
// can permit its first initialization. In particular, an erased superblock is
// insufficient: an interrupted filesystem can retain data near the very end.
// Reader must return true only after filling every requested byte.
template<class Reader>
Check checkErased(size_t partition_size, bool eligible, Reader reader) {
    if (!eligible || partition_size == 0) return {State::INELIGIBLE, 0};
    uint8_t buffer[kScanChunkBytes];
    size_t verified = 0;
    while (verified < partition_size) {
        const size_t left = partition_size - verified;
        const size_t count = left < sizeof(buffer) ? left : sizeof(buffer);
        if (!reader(verified, buffer, count)) return {State::READ_FAILED, verified};
        for (size_t i = 0; i < count; ++i) {
            if (buffer[i] != 0xff) return {State::DATA_PRESENT, verified + i};
        }
        verified += count;
    }
    return {State::ERASED, verified};
}
}  // namespace StorageBootPolicy
