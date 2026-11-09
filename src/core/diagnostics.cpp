#include "diagnostics.h"

#include <limits.h>

namespace {
uint32_t maxU32(uint32_t lhs, uint32_t rhs) {
    return lhs > rhs ? lhs : rhs;
}
}

DiagnosticsRegistry::DiagnosticsRegistry(uint32_t now_ms) {
    reset(now_ms);
}

void DiagnosticsRegistry::reset(uint32_t now_ms) {
    TaskGuard guard(_mutex);
    _snapshot = DiagnosticsSnapshot{};
    _driver_window = FrameWindow{};
    _navigator_window = FrameWindow{};
    _driver_window.started_ms = now_ms;
    _navigator_window.started_ms = now_ms;
    _last_clock_ms = now_ms;
    _uptime_ms = 0;
    _rtt_sum_ms = 0;
    _rtt_samples = 0;
    _last_sequence = 0;
    _has_sequence = false;
}

void DiagnosticsRegistry::setIdentity(const char* firmware,
                                      uint16_t protocol_version) {
    TaskGuard guard(_mutex);
    copyText(_snapshot.firmware, sizeof(_snapshot.firmware), firmware);
    _snapshot.protocol_version = protocol_version;
}

DiagnosticsRegistry::FrameWindow& DiagnosticsRegistry::window(
    DiagnosticDisplay display) {
    return display == DiagnosticDisplay::NAVIGATOR
        ? _navigator_window : _driver_window;
}

DisplayDiagnostics& DiagnosticsRegistry::displaySnapshot(
    DiagnosticDisplay display) {
    return display == DiagnosticDisplay::NAVIGATOR
        ? _snapshot.navigator : _snapshot.driver;
}

void DiagnosticsRegistry::advanceClock(uint32_t now_ms) {
    const uint32_t elapsed = static_cast<uint32_t>(now_ms - _last_clock_ms);
    // A slower task may publish an older timestamp after a render frame.
    // Ignore that stale sample instead of adding almost 2^32 ms of uptime.
    if (elapsed >= 0x80000000UL) return;
    _uptime_ms = saturatingAdd(_uptime_ms, elapsed);
    _last_clock_ms = now_ms;
    _snapshot.uptime_ms = _uptime_ms;
}

void DiagnosticsRegistry::recordFrame(DiagnosticDisplay display,
                                      uint32_t now_ms, uint32_t frame_us,
                                      uint32_t render_us, uint32_t push_us) {
    TaskGuard guard(_mutex);
    advanceClock(now_ms);
    FrameWindow& accumulator = window(display);
    DisplayDiagnostics& published = displaySnapshot(display);
    if (accumulator.frames != UINT32_MAX) ++accumulator.frames;
    accumulator.frame_sum_us = saturatingAdd(accumulator.frame_sum_us, frame_us);
    accumulator.render_sum_us = saturatingAdd(accumulator.render_sum_us, render_us);
    accumulator.push_sum_us = saturatingAdd(accumulator.push_sum_us, push_us);
    accumulator.frame_max_us = maxU32(accumulator.frame_max_us, frame_us);
    accumulator.render_max_us = maxU32(accumulator.render_max_us, render_us);
    accumulator.push_max_us = maxU32(accumulator.push_max_us, push_us);
    if (published.total_frames != UINT64_MAX) ++published.total_frames;
}

DisplayDiagnostics DiagnosticsRegistry::resetFrameWindow(
    DiagnosticDisplay display, uint32_t now_ms) {
    TaskGuard guard(_mutex);
    advanceClock(now_ms);
    FrameWindow& accumulator = window(display);
    DisplayDiagnostics& published = displaySnapshot(display);
    const bool ok = published.ok;
    const uint64_t total_frames = published.total_frames;
    const uint32_t elapsed_ms =
        static_cast<uint32_t>(now_ms - accumulator.started_ms);

    published.window_frames = accumulator.frames;
    published.fps = elapsed_ms == 0
        ? 0
        : averageU32(static_cast<uint64_t>(accumulator.frames) * 1000ULL,
                     elapsed_ms);
    published.frame_avg_us = averageU32(accumulator.frame_sum_us,
                                        accumulator.frames);
    published.render_avg_us = averageU32(accumulator.render_sum_us,
                                         accumulator.frames);
    published.push_avg_us = averageU32(accumulator.push_sum_us,
                                       accumulator.frames);
    published.frame_max_us = accumulator.frame_max_us;
    published.render_max_us = accumulator.render_max_us;
    published.push_max_us = accumulator.push_max_us;

    accumulator = FrameWindow{};
    accumulator.started_ms = now_ms;
    published.ok = ok;
    published.total_frames = total_frames;
    return published;
}

void DiagnosticsRegistry::publishDisplayWindow(
    DiagnosticDisplay display, const DisplayDiagnostics& window_metrics) {
    TaskGuard guard(_mutex);
    DisplayDiagnostics& published = displaySnapshot(display);
    const uint64_t total_frames = saturatingAdd(
        published.total_frames, window_metrics.window_frames);
    const uint32_t heartbeat_age_ms = published.heartbeat_age_ms;
    const uint32_t stack_free_bytes = published.stack_free_bytes;
    published = window_metrics;
    published.total_frames = total_frames;
    published.heartbeat_age_ms = heartbeat_age_ms;
    published.stack_free_bytes = stack_free_bytes;
}

bool DiagnosticsRegistry::recordInput(uint32_t sequence, uint32_t rtt_ms,
                                      bool accepted) {
    TaskGuard guard(_mutex);
    if (_snapshot.input.packets_received != UINT64_MAX)
        ++_snapshot.input.packets_received;

    if (!accepted) {
        if (_snapshot.input.stale_packets != UINT64_MAX)
            ++_snapshot.input.stale_packets;
        return false;
    }

    if (_has_sequence) {
        const uint32_t distance = static_cast<uint32_t>(sequence - _last_sequence);
        // Half-range ordering is the conventional wrap-safe sequence rule.
        if (distance == 0 || distance >= 0x80000000UL) {
            if (_snapshot.input.stale_packets != UINT64_MAX)
                ++_snapshot.input.stale_packets;
            return false;
        }
        if (distance > 1) {
            _snapshot.input.sequence_gaps = saturatingAdd(
                _snapshot.input.sequence_gaps,
                static_cast<uint64_t>(distance - 1));
        }
    }

    _last_sequence = sequence;
    _has_sequence = true;
    if (_snapshot.input.packets_accepted != UINT64_MAX)
        ++_snapshot.input.packets_accepted;
    _snapshot.input.controller_alive = true;
    _snapshot.input.rtt_last_ms = rtt_ms;
    _snapshot.input.rtt_max_ms = maxU32(_snapshot.input.rtt_max_ms, rtt_ms);
    _rtt_sum_ms = saturatingAdd(_rtt_sum_ms, rtt_ms);
    if (_rtt_samples != UINT64_MAX) ++_rtt_samples;
    _snapshot.input.rtt_avg_ms = averageU32(_rtt_sum_ms, _rtt_samples);
    return true;
}

void DiagnosticsRegistry::recordTimeout() {
    TaskGuard guard(_mutex);
    if (_snapshot.input.timeouts != UINT64_MAX)
        ++_snapshot.input.timeouts;
    _snapshot.input.controller_alive = false;
}

void DiagnosticsRegistry::setControllerAlive(bool alive) {
    TaskGuard guard(_mutex);
    _snapshot.input.controller_alive = alive;
}

void DiagnosticsRegistry::publishSlowHealth(
    uint32_t now_ms, const DiagnosticsSlowHealth& health) {
    TaskGuard guard(_mutex);
    advanceClock(now_ms);
    _snapshot.driver.ok = health.driver_ok;
    _snapshot.navigator.ok = health.navigator_ok;
    _snapshot.driver.heartbeat_age_ms = health.driver_heartbeat_age_ms;
    _snapshot.navigator.heartbeat_age_ms = health.navigator_heartbeat_age_ms;
    _snapshot.driver.stack_free_bytes = health.driver_stack_free_bytes;
    _snapshot.navigator.stack_free_bytes = health.navigator_stack_free_bytes;
    _snapshot.memory = health.memory;
    _snapshot.filesystem = health.filesystem;
    _snapshot.network = health.network;
    _snapshot.output = health.output;
    _snapshot.reboot_count = health.reboot_count;
    _snapshot.reset_reason_code = health.reset_reason_code;
    copyText(_snapshot.reset_reason, sizeof(_snapshot.reset_reason),
             health.reset_reason);
    if (health.last_error != nullptr)
        setLastError(health.last_error);
}

void DiagnosticsRegistry::setLastError(const char* error) {
    TaskGuard guard(_mutex);
    copyText(_snapshot.last_error, sizeof(_snapshot.last_error), error);
}

void DiagnosticsRegistry::copyText(char* destination, size_t capacity,
                                   const char* source) {
    if (destination == nullptr || capacity == 0) return;
    size_t i = 0;
    if (source != nullptr) {
        while (i + 1 < capacity && source[i] != '\0') {
            destination[i] = source[i];
            ++i;
        }
    }
    destination[i] = '\0';
    for (++i; i < capacity; ++i) destination[i] = '\0';
}

uint64_t DiagnosticsRegistry::saturatingAdd(uint64_t lhs, uint64_t rhs) {
    return UINT64_MAX - lhs < rhs ? UINT64_MAX : lhs + rhs;
}

uint32_t DiagnosticsRegistry::averageU32(uint64_t sum, uint64_t count) {
    if (count == 0) return 0;
    const uint64_t average = sum / count;
    return average > UINT32_MAX ? UINT32_MAX
                                : static_cast<uint32_t>(average);
}
