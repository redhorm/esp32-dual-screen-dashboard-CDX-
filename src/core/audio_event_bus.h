#pragma once

#include <stddef.h>
#include <stdint.h>

// Compact audio messages consumed by the browser today and by any future
// hardware sink.  The bus owns no heap memory and never blocks the game loop.
enum class AudioEventType : uint8_t {
    NONE = 0,
    ENGINE_START,
    ENGINE_RPM,
    GEAR_CHANGE,
    SKID,
    COLLISION_SMALL,
    COLLISION_LARGE,
    MUD,
    WATER,
    CHECKPOINT,
    COUNTDOWN,
    FINISH,
    WARNING,
    PAUSE,
};

struct AudioEvent {
    uint32_t tick = 0;
    uint16_t sequence = 0;
    int16_t value = 0;       // RPM, gear, countdown number, etc.
    AudioEventType type = AudioEventType::NONE;
    uint8_t intensity = 0;   // 0..255
};

class AudioEventBus {
public:
    static constexpr uint8_t CAPACITY = 16;

    void clear();
    uint16_t publish(AudioEventType type, uint8_t intensity, int16_t value,
                     uint32_t tick);
    uint8_t readAfter(uint16_t sequence, AudioEvent* output,
                      uint8_t output_capacity) const;

    uint16_t latestSequence() const { return _latest_sequence; }
    uint8_t size() const { return _count; }

    static bool sequenceAfter(uint16_t candidate, uint16_t reference);

private:
    AudioEvent _events[CAPACITY] = {};
    uint16_t _latest_sequence = 0;
    uint8_t _head = 0;   // next insertion slot
    uint8_t _count = 0;
};

