#include <unity.h>

#include <cstring>
#include <type_traits>

#include "core/diagnostics.h"

void setUp() {}
void tearDown() {}

static void test_snapshot_contract_and_slow_health() {
    TEST_ASSERT_TRUE((std::is_trivially_copyable<DiagnosticsSnapshot>::value));

    DiagnosticsRegistry registry(100);
    registry.setIdentity("4.2.0-beta.7", 4);

    DiagnosticsSlowHealth health;
    health.driver_ok = true;
    health.navigator_ok = false;
    health.driver_heartbeat_age_ms = 21;
    health.navigator_heartbeat_age_ms = 900;
    health.driver_stack_free_bytes = 3200;
    health.navigator_stack_free_bytes = 5100;
    health.memory.heap_free_bytes = 123456;
    health.memory.heap_min_free_bytes = 110000;
    health.memory.heap_largest_block_bytes = 64000;
    health.memory.psram_present = true;
    health.memory.psram_free_bytes = 7000000;
    health.memory.psram_total_bytes = 8388608;
    health.filesystem.ready = true;
    health.filesystem.total_bytes = 1000;
    health.filesystem.used_bytes = 600;
    health.filesystem.free_bytes = 400;
    health.filesystem.system_bytes = 100;
    health.filesystem.user_bytes = 300;
    health.filesystem.transaction_bytes = 50;
    health.network.ap_online = true;
    health.network.clients = 2;
    health.output.rtl_ok = true;
    health.output.control_alive = true;
    health.output.control_age_ms = 4;
    health.output.control_stack_free_bytes = 2048;
    health.output.esc_enabled = true;
    health.output.esc_valid = true;
    health.output.esc_pulse_us = 1512;
    health.reboot_count = 8;
    health.reset_reason_code = 3;
    health.reset_reason = "software reset";
    health.last_error = "filesystem recovered";
    registry.publishSlowHealth(1100, health);

    const DiagnosticsSnapshot& snapshot = registry.snapshot();
    TEST_ASSERT_EQUAL_STRING("4.2.0-beta.7", snapshot.firmware);
    TEST_ASSERT_EQUAL_UINT16(4, snapshot.protocol_version);
    TEST_ASSERT_EQUAL_UINT64(1000, snapshot.uptime_ms);
    TEST_ASSERT_TRUE(snapshot.driver.ok);
    TEST_ASSERT_FALSE(snapshot.navigator.ok);
    TEST_ASSERT_EQUAL_UINT32(21, snapshot.driver.heartbeat_age_ms);
    TEST_ASSERT_EQUAL_UINT32(900, snapshot.navigator.heartbeat_age_ms);
    TEST_ASSERT_EQUAL_UINT32(3200, snapshot.driver.stack_free_bytes);
    TEST_ASSERT_EQUAL_UINT32(123456, snapshot.memory.heap_free_bytes);
    TEST_ASSERT_EQUAL_UINT32(400, snapshot.filesystem.free_bytes);
    TEST_ASSERT_EQUAL_UINT32(50, snapshot.filesystem.transaction_bytes);
    TEST_ASSERT_TRUE(snapshot.network.ap_online);
    TEST_ASSERT_EQUAL_UINT8(2, snapshot.network.clients);
    TEST_ASSERT_TRUE(snapshot.output.rtl_ok);
    TEST_ASSERT_TRUE(snapshot.output.control_alive);
    TEST_ASSERT_EQUAL_UINT32(4, snapshot.output.control_age_ms);
    TEST_ASSERT_EQUAL_UINT16(1512, snapshot.output.esc_pulse_us);
    TEST_ASSERT_EQUAL_UINT32(8, snapshot.reboot_count);
    TEST_ASSERT_EQUAL_STRING("software reset", snapshot.reset_reason);
    TEST_ASSERT_EQUAL_STRING("filesystem recovered", snapshot.last_error);
}

static void test_frame_window_average_max_and_fps() {
    DiagnosticsRegistry registry(0);
    registry.recordFrame(DiagnosticDisplay::DRIVER, 100, 1000, 600, 400);
    registry.recordFrame(DiagnosticDisplay::DRIVER, 600, 3000, 1800, 1200);

    const DisplayDiagnostics metrics =
        registry.resetFrameWindow(DiagnosticDisplay::DRIVER, 1000);
    TEST_ASSERT_EQUAL_UINT32(2, metrics.window_frames);
    TEST_ASSERT_EQUAL_UINT32(2, metrics.fps);
    TEST_ASSERT_EQUAL_UINT64(2, metrics.total_frames);
    TEST_ASSERT_EQUAL_UINT32(2000, metrics.frame_avg_us);
    TEST_ASSERT_EQUAL_UINT32(3000, metrics.frame_max_us);
    TEST_ASSERT_EQUAL_UINT32(1200, metrics.render_avg_us);
    TEST_ASSERT_EQUAL_UINT32(1800, metrics.render_max_us);
    TEST_ASSERT_EQUAL_UINT32(800, metrics.push_avg_us);
    TEST_ASSERT_EQUAL_UINT32(1200, metrics.push_max_us);

    const DisplayDiagnostics empty =
        registry.resetFrameWindow(DiagnosticDisplay::DRIVER, 2000);
    TEST_ASSERT_EQUAL_UINT32(0, empty.window_frames);
    TEST_ASSERT_EQUAL_UINT32(0, empty.frame_avg_us);
    TEST_ASSERT_EQUAL_UINT64(2, empty.total_frames);
}

static void test_clock_and_frame_window_survive_millis_wrap() {
    DiagnosticsRegistry registry(0xFFFFFF00UL);
    registry.recordFrame(DiagnosticDisplay::NAVIGATOR, 0xFFFFFF80UL,
                         4000, 3000, 1000);
    registry.recordFrame(DiagnosticDisplay::NAVIGATOR, 0x00000080UL,
                         6000, 4500, 1500);
    const DisplayDiagnostics metrics =
        registry.resetFrameWindow(DiagnosticDisplay::NAVIGATOR, 0x000002E8UL);

    // 0x3E8 ms elapsed across wrap, therefore exactly two FPS.
    TEST_ASSERT_EQUAL_UINT32(2, metrics.fps);
    TEST_ASSERT_EQUAL_UINT32(5000, metrics.frame_avg_us);
    TEST_ASSERT_EQUAL_UINT64(1000, registry.snapshot().uptime_ms);
}

static void test_input_gap_stale_timeout_and_rtt() {
    DiagnosticsRegistry registry;
    TEST_ASSERT_TRUE(registry.recordInput(0xFFFFFFFEUL, 10, true));
    TEST_ASSERT_TRUE(registry.recordInput(1, 20, true));
    TEST_ASSERT_FALSE(registry.recordInput(1, 99, true));
    TEST_ASSERT_FALSE(registry.recordInput(2, 99, false));

    const InputDiagnostics& input = registry.snapshot().input;
    TEST_ASSERT_EQUAL_UINT64(4, input.packets_received);
    TEST_ASSERT_EQUAL_UINT64(2, input.packets_accepted);
    TEST_ASSERT_EQUAL_UINT64(2, input.sequence_gaps);
    TEST_ASSERT_EQUAL_UINT64(2, input.stale_packets);
    TEST_ASSERT_EQUAL_UINT32(20, input.rtt_last_ms);
    TEST_ASSERT_EQUAL_UINT32(15, input.rtt_avg_ms);
    TEST_ASSERT_EQUAL_UINT32(20, input.rtt_max_ms);
    TEST_ASSERT_TRUE(input.controller_alive);

    registry.recordTimeout();
    TEST_ASSERT_EQUAL_UINT64(1, registry.snapshot().input.timeouts);
    TEST_ASSERT_FALSE(registry.snapshot().input.controller_alive);
    registry.setControllerAlive(true);
    registry.setControllerAlive(false);
    TEST_ASSERT_EQUAL_UINT64(1, registry.snapshot().input.timeouts);
    TEST_ASSERT_FALSE(registry.snapshot().input.controller_alive);
}

static void test_identity_and_error_strings_are_bounded() {
    DiagnosticsRegistry registry;
    registry.setIdentity("ABCDEFGHIJKLMNOPQRSTUVWXYZ-TOO-LONG", 9);
    registry.setLastError(
        "012345678901234567890123456789012345678901234567890123456789");
    const DiagnosticsSnapshot& snapshot = registry.snapshot();
    TEST_ASSERT_EQUAL_UINT(DIAGNOSTICS_FIRMWARE_TEXT_SIZE - 1,
                           std::strlen(snapshot.firmware));
    TEST_ASSERT_EQUAL_UINT(DIAGNOSTICS_ERROR_TEXT_SIZE - 1,
                           std::strlen(snapshot.last_error));
    TEST_ASSERT_EQUAL_CHAR('\0',
        snapshot.firmware[DIAGNOSTICS_FIRMWARE_TEXT_SIZE - 1]);
    TEST_ASSERT_EQUAL_CHAR('\0',
        snapshot.last_error[DIAGNOSTICS_ERROR_TEXT_SIZE - 1]);
}

static void test_external_display_window_preserves_running_total() {
    DiagnosticsRegistry registry;
    DisplayDiagnostics first;
    first.ok = true;
    first.fps = 12;
    first.window_frames = 60;
    first.total_frames = 9999;  // ignored: the registry owns this counter.
    first.render_avg_us = 2400;
    first.render_max_us = 4100;
    registry.publishDisplayWindow(DiagnosticDisplay::NAVIGATOR, first);

    DisplayDiagnostics second = first;
    second.window_frames = 55;
    second.fps = 11;
    second.total_frames = 1;
    registry.publishDisplayWindow(DiagnosticDisplay::NAVIGATOR, second);

    const DisplayDiagnostics& published = registry.snapshot().navigator;
    TEST_ASSERT_EQUAL_UINT64(115, published.total_frames);
    TEST_ASSERT_EQUAL_UINT32(55, published.window_frames);
    TEST_ASSERT_EQUAL_UINT32(11, published.fps);
    TEST_ASSERT_EQUAL_UINT32(2400, published.render_avg_us);
    TEST_ASSERT_EQUAL_UINT32(4100, published.render_max_us);
    TEST_ASSERT_TRUE(published.ok);
}

static void test_out_of_order_task_clocks_do_not_jump_uptime() {
    DiagnosticsRegistry registry(100);
    registry.recordFrame(DiagnosticDisplay::DRIVER, 200, 10, 5, 5);
    DiagnosticsSlowHealth health;
    registry.publishSlowHealth(190, health);
    TEST_ASSERT_EQUAL_UINT64(100, registry.snapshot().uptime_ms);
    registry.recordFrame(DiagnosticDisplay::NAVIGATOR, 210, 10, 5, 5);
    TEST_ASSERT_EQUAL_UINT64(110, registry.snapshot().uptime_ms);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_out_of_order_task_clocks_do_not_jump_uptime);
    RUN_TEST(test_snapshot_contract_and_slow_health);
    RUN_TEST(test_frame_window_average_max_and_fps);
    RUN_TEST(test_clock_and_frame_window_survive_millis_wrap);
    RUN_TEST(test_input_gap_stale_timeout_and_rtt);
    RUN_TEST(test_identity_and_error_strings_are_bounded);
    RUN_TEST(test_external_display_window_preserves_running_total);
    return UNITY_END();
}
