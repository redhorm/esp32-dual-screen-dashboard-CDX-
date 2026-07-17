#include "audio_event_bus.h"

void AudioEventBus::clear() {
    _head = 0;
    _count = 0;
}

bool AudioEventBus::sequenceAfter(uint16_t candidate, uint16_t reference) {
    const uint16_t distance = static_cast<uint16_t>(candidate - reference);
    return distance != 0u && distance < 0x8000u;
}

uint16_t AudioEventBus::publish(AudioEventType type, uint8_t intensity,
                                int16_t value, uint32_t tick) {
    ++_latest_sequence;
    if (_latest_sequence == 0u) ++_latest_sequence;

    AudioEvent& event = _events[_head];
    event.tick = tick;
    event.sequence = _latest_sequence;
    event.value = value;
    event.type = type;
    event.intensity = intensity;

    _head = static_cast<uint8_t>((_head + 1u) % CAPACITY);
    if (_count < CAPACITY) ++_count;
    return _latest_sequence;
}

uint8_t AudioEventBus::readAfter(uint16_t sequence, AudioEvent* output,
                                 uint8_t output_capacity) const {
    if (!output || output_capacity == 0u || _count == 0u) return 0u;

    const uint8_t oldest = static_cast<uint8_t>(
        (_head + CAPACITY - _count) % CAPACITY);
    uint8_t written = 0;
    for (uint8_t i = 0; i < _count && written < output_capacity; ++i) {
        const AudioEvent& event =
            _events[static_cast<uint8_t>((oldest + i) % CAPACITY)];
        // Sequence zero is reserved and means "new consumer": return the
        // retained window even after the 16-bit counter crossed half-range.
        if (sequence == 0u || sequenceAfter(event.sequence, sequence))
            output[written++] = event;
    }
    return written;
}
