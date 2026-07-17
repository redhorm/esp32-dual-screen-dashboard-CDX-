#pragma once

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

// Hardware-independent diagnostics aggregation. The registry is intentionally
// single-writer: a platform adapter may publish a copy of snapshot() under its
// existing task lock or double-buffer boundary.

constexpr size_t DIAGNOSTICS_FIRMWARE_TEXT_SIZE = 32;
constexpr size_t DIAGNOSTICS_ERROR_TEXT_SIZE = 48;
constexpr size_t DIAGNOSTICS_RESET_TEXT_SIZE = 28;

enum class DiagnosticDisplay : uint8_t {
    DRIVER = 0,
    NAVIGATOR = 1,
};

struct DisplayDiagnostics {
    bool ok = false;
    uint32_t heartbeat_age_ms = 0;
    uint32_t stack_free_bytes = 0;
    uint32_t fps = 0;
    uint32_t window_frames = 0;
    uint64_t total_frames = 0;
    uint32_t frame_avg_us = 0;
    uint32_t frame_max_us = 0;
    uint32_t render_avg_us = 0;
    uint32_t render_max_us = 0;
    uint32_t push_avg_us = 0;
    uint32_t push_max_us = 0;
};

struct MemoryDiagnostics {
    uint32_t heap_free_bytes = 0;
    uint32_t heap_min_free_bytes = 0;
    uint32_t heap_largest_block_bytes = 0;
    bool psram_present = false;
    uint32_t psram_free_bytes = 0;
    uint32_t psram_total_bytes = 0;
};

struct FilesystemDiagnostics {
    bool ready = false;
    uint32_t total_bytes = 0;
    uint32_t used_bytes = 0;
    uint32_t free_bytes = 0;
    uint32_t system_bytes = 0;
    uint32_t user_bytes = 0;
    uint32_t game_bytes = 0;
    uint32_t transaction_bytes = 0;
};

struct NetworkDiagnostics {
    bool ap_online = false;
    uint8_t clients = 0;
};

struct InputDiagnostics {
    uint64_t packets_received = 0;
    uint64_t packets_accepted = 0;
    uint64_t sequence_gaps = 0;
    uint64_t stale_packets = 0;
    uint64_t timeouts = 0;
    uint32_t rtt_last_ms = 0;
    uint32_t rtt_avg_ms = 0;
    uint32_t rtt_max_ms = 0;
    bool controller_alive = false;
};

struct OutputDiagnostics {
    bool rtl_ok = false;
    bool control_alive = false;
    uint32_t control_age_ms = 0;
    uint32_t control_stack_free_bytes = 0;
    bool esc_enabled = false;
    bool esc_valid = false;
    uint16_t esc_pulse_us = 0;
};

struct DiagnosticsSnapshot {
    char firmware[DIAGNOSTICS_FIRMWARE_TEXT_SIZE] = {};
    uint16_t protocol_version = 0;
    uint64_t uptime_ms = 0;
    DisplayDiagnostics driver;
    DisplayDiagnostics navigator;
    MemoryDiagnostics memory;
    FilesystemDiagnostics filesystem;
    NetworkDiagnostics network;
    InputDiagnostics input;
    OutputDiagnostics output;
    char last_error[DIAGNOSTICS_ERROR_TEXT_SIZE] = {};
    uint32_t reboot_count = 0;
    uint32_t reset_reason_code = 0;
    char reset_reason[DIAGNOSTICS_RESET_TEXT_SIZE] = {};
};

struct DiagnosticsSlowHealth {
    bool driver_ok = false;
    bool navigator_ok = false;
    uint32_t driver_heartbeat_age_ms = 0;
    uint32_t navigator_heartbeat_age_ms = 0;
    uint32_t driver_stack_free_bytes = 0;
    uint32_t navigator_stack_free_bytes = 0;
    MemoryDiagnostics memory;
    FilesystemDiagnostics filesystem;
    NetworkDiagnostics network;
    OutputDiagnostics output;
    uint32_t reboot_count = 0;
    uint32_t reset_reason_code = 0;
    const char* reset_reason = nullptr;
    const char* last_error = nullptr;
};

static_assert(std::is_trivially_copyable<DiagnosticsSnapshot>::value,
              "DiagnosticsSnapshot must remain safe to copy between tasks");
static_assert(std::is_trivially_copyable<DisplayDiagnostics>::value,
              "DisplayDiagnostics must remain trivially copyable");

class DiagnosticsRegistry {
public:
    explicit DiagnosticsRegistry(uint32_t now_ms = 0);

    void reset(uint32_t now_ms = 0);
    void setIdentity(const char* firmware, uint16_t protocol_version);

    void recordFrame(DiagnosticDisplay display, uint32_t now_ms,
                     uint32_t frame_us, uint32_t render_us,
                     uint32_t push_us);

    // Publishes the accumulated metrics for this display and starts a fresh
    // window at now_ms. Unsigned time subtraction keeps millis() wrap-safe.
    DisplayDiagnostics resetFrameWindow(DiagnosticDisplay display,
                                        uint32_t now_ms);

    // Single-writer bridge for metrics accumulated by another task. All
    // window values are copied, while total_frames remains registry-owned and
    // advances by window_metrics.window_frames. Use either this path or
    // recordFrame() for a given display, never both.
    void publishDisplayWindow(DiagnosticDisplay display,
                              const DisplayDiagnostics& window_metrics);

    // Returns true only when the packet is accepted and newer than the last
    // accepted sequence. Duplicate/out-of-order packets are counted as stale.
    bool recordInput(uint32_t sequence, uint32_t rtt_ms, bool accepted);
    void recordTimeout();
    // Updates connection state without counting a gameplay timeout (pause,
    // finish and explicit exit are normal lifecycle transitions).
    void setControllerAlive(bool alive);

    // Call at a low frequency (for example 1 Hz). now_ms also extends the
    // 32-bit platform clock into the 64-bit uptime exposed by the snapshot.
    void publishSlowHealth(uint32_t now_ms,
                           const DiagnosticsSlowHealth& health);
    void setLastError(const char* error);

    const DiagnosticsSnapshot& snapshot() const { return _snapshot; }

private:
    struct FrameWindow {
        uint32_t started_ms = 0;
        uint32_t frames = 0;
        uint64_t frame_sum_us = 0;
        uint64_t render_sum_us = 0;
        uint64_t push_sum_us = 0;
        uint32_t frame_max_us = 0;
        uint32_t render_max_us = 0;
        uint32_t push_max_us = 0;
    };

    DiagnosticsSnapshot _snapshot;
    FrameWindow _driver_window;
    FrameWindow _navigator_window;
    uint32_t _last_clock_ms = 0;
    uint64_t _uptime_ms = 0;
    uint64_t _rtt_sum_ms = 0;
    uint64_t _rtt_samples = 0;
    uint32_t _last_sequence = 0;
    bool _has_sequence = false;

    FrameWindow& window(DiagnosticDisplay display);
    DisplayDiagnostics& displaySnapshot(DiagnosticDisplay display);
    void advanceClock(uint32_t now_ms);
    static void copyText(char* destination, size_t capacity,
                         const char* source);
    static uint64_t saturatingAdd(uint64_t lhs, uint64_t rhs);
    static uint32_t averageU32(uint64_t sum, uint64_t count);
};
