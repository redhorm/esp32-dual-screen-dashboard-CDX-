#include <unity.h>

#include <string.h>
#include <type_traits>

#include "core/audio_event_bus.h"
#include "core/experience.h"
#include "core/trail_assault_game.h"

namespace {

constexpr uint32_t SESSION = 0x44001234u;

void startRunning(TrailAssaultGame& game, uint32_t& now) {
    now = 0;
    game.start(now, SESSION);
    now = 3000;
    game.update(now);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailStatus::RUNNING),
                          static_cast<int>(game.snapshot(now).status));
}

TrailInputResult sendInput(TrailAssaultGame& game, uint16_t& sequence,
                           uint32_t now, int steer, int throttle,
                           int brake = 0, uint32_t session = SESSION) {
    TrailInput input;
    input.session_id = session;
    input.sequence = ++sequence;
    input.client_ms = now;
    input.received_ms = now;
    input.steering = static_cast<int8_t>(steer);
    input.throttle = static_cast<uint8_t>(throttle);
    input.brake = static_cast<uint8_t>(brake);
    return game.submitInput(input);
}

int avoidanceTarget(const TrailGameSnapshot& snapshot) {
    if (snapshot.visible_objects == 0u) return 0;
    const TrailObjectView& object = snapshot.objects[0];
    if (object.distance_dm > 900u) return 0;
    if (object.kind == TrailObstacleKind::GATE ||
        object.kind == TrailObstacleKind::JUMP_RAMP)
        return static_cast<int>(object.lateral) * 10;
    return object.lateral >= 0 ? -780 : 780;
}

TrailGameSnapshot driveToResult(TrailAssaultGame& game, uint32_t& now,
                                uint16_t& sequence,
                                uint8_t* terrain_mask = nullptr) {
    for (uint32_t frame = 0; frame < 3600u && !game.isFinished(); ++frame) {
        TrailGameSnapshot snapshot = game.snapshot(now);
        if (terrain_mask)
            *terrain_mask |= static_cast<uint8_t>(
                1u << static_cast<uint8_t>(snapshot.terrain));
        if ((frame & 1u) == 0u) {
            const int target = avoidanceTarget(snapshot);
            const int steer = snapshot.lateral_permille < target - 35 ? 100
                : snapshot.lateral_permille > target + 35 ? -100 : 0;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
                                  static_cast<int>(sendInput(
                                      game, sequence, now, steer, 100)));
        }
        now += 50u;
        game.update(now);
    }
    return game.snapshot(now);
}

void test_compact_trivial_contracts() {
    TEST_ASSERT_TRUE(std::is_trivially_copyable<TrailGameSnapshot>::value);
    TEST_ASSERT_TRUE(std::is_trivially_copyable<TrailInput>::value);
    TEST_ASSERT_TRUE(std::is_trivially_copyable<TrailGhostRecording>::value);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(192u, sizeof(TrailGameSnapshot));
    TEST_ASSERT_EQUAL_UINT32(10u, sizeof(TrailGhostSample));
    TEST_ASSERT_EQUAL_UINT32(300u, TrailGhostRecording::MAX_SAMPLES);
}

void test_audio_bus_orders_and_overwrites_without_heap() {
    AudioEventBus bus;
    for (uint8_t i = 1; i <= 20u; ++i)
        bus.publish(AudioEventType::ENGINE_RPM, i,
                    static_cast<int16_t>(800 + i), i);
    TEST_ASSERT_EQUAL_UINT8(AudioEventBus::CAPACITY, bus.size());

    AudioEvent events[AudioEventBus::CAPACITY];
    uint8_t count = bus.readAfter(0, events, AudioEventBus::CAPACITY);
    TEST_ASSERT_EQUAL_UINT8(AudioEventBus::CAPACITY, count);
    TEST_ASSERT_EQUAL_UINT16(5u, events[0].sequence);
    TEST_ASSERT_EQUAL_UINT16(20u, events[count - 1u].sequence);
    for (uint8_t i = 1; i < count; ++i)
        TEST_ASSERT_TRUE(AudioEventBus::sequenceAfter(
            events[i].sequence, events[i - 1u].sequence));

    count = bus.readAfter(18u, events, AudioEventBus::CAPACITY);
    TEST_ASSERT_EQUAL_UINT8(2u, count);
    TEST_ASSERT_EQUAL_UINT16(19u, events[0].sequence);
    TEST_ASSERT_EQUAL_UINT16(20u, events[1].sequence);

    for (uint32_t i = 0; i < 33000u; ++i)
        bus.publish(AudioEventType::ENGINE_RPM, 1, 900, i);
    count = bus.readAfter(0, events, AudioEventBus::CAPACITY);
    TEST_ASSERT_EQUAL_UINT8(AudioEventBus::CAPACITY, count);
}

void test_seed_generation_is_deterministic_bounded_and_complete() {
    TrailGameConfig config;
    config.mode = TrailMode::TIME_ATTACK;
    config.seed = 0xA17C4400u;
    TrailAssaultGame a;
    TrailAssaultGame b;
    a.configure(config);
    b.configure(config);

    TEST_ASSERT_EQUAL_UINT32(a.trackFingerprint(), b.trackFingerprint());
    TEST_ASSERT_EQUAL_UINT8(a.segmentCount(), b.segmentCount());
    TEST_ASSERT_TRUE(a.segmentCount() >= 64u);
    TEST_ASSERT_TRUE(a.segmentCount() <= TrailAssaultGame::MAX_SEGMENTS);
    TEST_ASSERT_EQUAL_UINT32(a.courseLengthCm(), b.courseLengthCm());

    uint8_t terrains = 0;
    for (uint8_t i = 0; i < a.segmentCount(); ++i) {
        const TrailSegment& left = a.segment(i);
        const TrailSegment& right = b.segment(i);
        TEST_ASSERT_EQUAL_UINT16(left.length_m, right.length_m);
        TEST_ASSERT_EQUAL_INT8(left.curve, right.curve);
        TEST_ASSERT_EQUAL_INT8(left.grade, right.grade);
        TEST_ASSERT_EQUAL_INT8(left.obstacle_lateral,
                               right.obstacle_lateral);
        TEST_ASSERT_EQUAL_UINT8(left.width, right.width);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(left.terrain),
                              static_cast<int>(right.terrain));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(left.obstacle),
                              static_cast<int>(right.obstacle));
        TEST_ASSERT_EQUAL_UINT8(left.flags, right.flags);
        TEST_ASSERT_TRUE(left.length_m >= 10u && left.length_m <= 20u);
        TEST_ASSERT_TRUE(left.curve >= -6 && left.curve <= 6);
        TEST_ASSERT_TRUE(left.grade >= -5 && left.grade <= 7);
        TEST_ASSERT_TRUE(left.width >= 74u && left.width <= 100u);
        terrains |= static_cast<uint8_t>(
            1u << static_cast<uint8_t>(left.terrain));
    }
    TEST_ASSERT_EQUAL_HEX8(0x3Fu, terrains);

    config.seed ^= 0x01010101u;
    b.configure(config);
    TEST_ASSERT_NOT_EQUAL(a.trackFingerprint(), b.trackFingerprint());
}

void test_gate_and_hill_generators_enforce_objectives() {
    TrailGameConfig config;
    config.seed = 0x12345678u;
    TrailAssaultGame game;

    config.mode = TrailMode::GATE_RUSH;
    game.configure(config);
    uint8_t gates = 0;
    for (uint8_t i = 0; i < game.segmentCount(); ++i) {
        const TrailSegment& segment = game.segment(i);
        if (segment.flags & TRAIL_SEGMENT_GATE) {
            ++gates;
            TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailObstacleKind::GATE),
                                  static_cast<int>(segment.obstacle));
        }
    }
    TEST_ASSERT_TRUE(gates >= 10u);

    config.mode = TrailMode::HILL_CLIMB;
    game.configure(config);
    int total_grade = 0;
    for (uint8_t i = 0; i < game.segmentCount(); ++i)
        total_grade += game.segment(i).grade * game.segment(i).length_m;
    TEST_ASSERT_TRUE(total_grade > 0);
}

void test_fixed_step_is_invariant_to_update_cadence() {
    TrailGameConfig config;
    config.seed = 0xBEEFF00Du;
    TrailAssaultGame fine;
    TrailAssaultGame coarse;
    fine.configure(config);
    coarse.configure(config);
    fine.start(0, SESSION);
    coarse.start(0, SESSION);

    for (uint32_t t = 10; t <= 3000; t += 10) fine.update(t);
    for (uint32_t t = 100; t <= 3000; t += 100) coarse.update(t);
    uint16_t fine_seq = 0, coarse_seq = 0;
    sendInput(fine, fine_seq, 3000, 43, 82, 0);
    sendInput(coarse, coarse_seq, 3000, 43, 82, 0);
    for (uint32_t t = 3005; t <= 3250; t += 5) fine.update(t);
    coarse.update(3250);

    const TrailGameSnapshot a = fine.snapshot(3250);
    const TrailGameSnapshot b = coarse.snapshot(3250);
    TEST_ASSERT_EQUAL_UINT32(a.tick, b.tick);
    TEST_ASSERT_EQUAL_UINT32(a.distance_cm, b.distance_cm);
    TEST_ASSERT_EQUAL_UINT16(a.speed_kph10, b.speed_kph10);
    TEST_ASSERT_EQUAL_INT16(a.lateral_permille, b.lateral_permille);
    TEST_ASSERT_EQUAL_INT16(a.drift_permille, b.drift_permille);
    TEST_ASSERT_EQUAL_UINT16(a.temperature_c10, b.temperature_c10);
    TEST_ASSERT_EQUAL_UINT16(a.damage_permille, b.damage_permille);
}

void test_controller_session_sequence_timeout_and_neutral() {
    TrailAssaultGame game;
    uint32_t now;
    startRunning(game, now);
    uint16_t sequence = 9;

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::WRONG_SESSION),
        static_cast<int>(sendInput(game, sequence, now, 30, 100, 0,
                                   SESSION + 1u)));
    sequence = 9;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, now, 30, 100)));

    TrailInput stale;
    stale.session_id = SESSION;
    stale.sequence = 9;
    stale.received_ms = now + 1u;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::STALE_SEQUENCE),
                          static_cast<int>(game.submitInput(stale)));

    game.update(now + TrailAssaultGame::INPUT_TIMEOUT_MS + 30u);
    TrailGameSnapshot timed_out = game.snapshot(
        now + TrailAssaultGame::INPUT_TIMEOUT_MS + 30u);
    TEST_ASSERT_TRUE(timed_out.controller.timed_out);
    TEST_ASSERT_TRUE(timed_out.controller.neutral);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailEvent::CONTROLLER_TIMEOUT),
                          static_cast<int>(timed_out.last_event));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(game.neutralize(SESSION, 11,
                                         now + 400u)));
    TrailGameSnapshot neutral = game.snapshot(now + 400u);
    TEST_ASSERT_TRUE(neutral.controller.neutral);
    TEST_ASSERT_FALSE(neutral.controller.timed_out);
}

void test_running_stall_is_bounded_and_future_input_stays_fresh() {
    TrailAssaultGame game;
    uint32_t now;
    startRunning(game, now);
    const uint32_t resumed_at = now + 10000u;
    uint16_t sequence = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, resumed_at, 25, 100)));
    const uint32_t tick_before = game.snapshot(now).tick;

    game.update(resumed_at);
    TrailGameSnapshot resumed = game.snapshot(resumed_at);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        TrailAssaultGame::MAX_CATCH_UP_MS * TrailAssaultGame::FIXED_HZ / 1000u,
        resumed.tick - tick_before);
    TEST_ASSERT_FALSE(resumed.controller.timed_out);
    TEST_ASSERT_FALSE(resumed.controller.neutral);
    TEST_ASSERT_EQUAL_UINT8(100u, resumed.controller.throttle);
    TEST_ASSERT_EQUAL_UINT16(0u, resumed.controller.age_ms);

    game.update(resumed_at + TrailAssaultGame::INPUT_TIMEOUT_MS + 40u);
    TrailGameSnapshot expired = game.snapshot(
        resumed_at + TrailAssaultGame::INPUT_TIMEOUT_MS + 40u);
    TEST_ASSERT_TRUE(expired.controller.timed_out);
    TEST_ASSERT_TRUE(expired.controller.neutral);
}

void test_countdown_never_refreshes_stale_controller_input() {
    TrailAssaultGame game;
    game.start(0, SESSION);
    uint16_t sequence = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, 100u, 40, 100)));
    game.update(100u);
    game.update(3000u);

    const TrailGameSnapshot running = game.snapshot(3000u);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailStatus::RUNNING),
                          static_cast<int>(running.status));
    TEST_ASSERT_TRUE(running.controller.timed_out);
    TEST_ASSERT_TRUE(running.controller.neutral);
    TEST_ASSERT_EQUAL_UINT16(0u, running.speed_kph10);
}

void test_long_countdown_stall_resynchronizes_input_clock() {
    TrailAssaultGame game;
    game.start(0, SESSION);
    game.update(8000u);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailStatus::RUNNING),
        static_cast<int>(game.snapshot(8000u).status));

    uint16_t sequence = 0;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, 8000u, 0, 100)));
    game.update(8017u);
    TrailGameSnapshot fresh = game.snapshot(8017u);
    TEST_ASSERT_FALSE(fresh.controller.timed_out);
    TEST_ASSERT_EQUAL_UINT8(100u, fresh.controller.throttle);

    game.update(8350u);
    TrailGameSnapshot expired = game.snapshot(8350u);
    TEST_ASSERT_TRUE(expired.controller.timed_out);
    TEST_ASSERT_TRUE(expired.controller.neutral);
}

void test_pause_preserves_sequence_replay_barrier() {
    TrailAssaultGame game;
    uint32_t now;
    startRunning(game, now);
    uint16_t sequence = 9;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, now, 25, 80)));
    game.pause(now + 20u);
    game.resume(now + 1000u);

    TrailInput delayed;
    delayed.session_id = SESSION;
    delayed.sequence = 10u;
    delayed.received_ms = now + 1000u;
    delayed.throttle = 100u;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::STALE_SEQUENCE),
                          static_cast<int>(game.submitInput(delayed)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(sendInput(game, sequence, now + 1001u, 0, 60)));
}

void test_brake_reduces_speed_and_terrain_changes_grip() {
    TrailAssaultGame game;
    uint32_t now;
    startRunning(game, now);
    uint16_t sequence = 0;
    uint8_t terrain_mask = 0;
    uint16_t min_grip = 1000, max_grip = 0;

    for (uint32_t frame = 0; frame < 1000u && !game.isFinished(); ++frame) {
        TrailGameSnapshot before = game.snapshot(now);
        terrain_mask |= static_cast<uint8_t>(
            1u << static_cast<uint8_t>(before.terrain));
        if (before.grip_permille < min_grip) min_grip = before.grip_permille;
        if (before.grip_permille > max_grip) max_grip = before.grip_permille;
        if ((frame & 1u) == 0u) {
            int target = avoidanceTarget(before);
            int steer = before.lateral_permille < target - 35 ? 100
                : before.lateral_permille > target + 35 ? -100 : 0;
            sendInput(game, sequence, now, steer, 100);
        }
        now += 50u;
        game.update(now);
    }
    TEST_ASSERT_EQUAL_HEX8(0x3Fu, terrain_mask);
    TEST_ASSERT_TRUE(max_grip > min_grip + 200u);

    TrailAssaultGame braking;
    startRunning(braking, now);
    sequence = 0;
    for (uint8_t i = 0; i < 15u; ++i) {
        if ((i & 1u) == 0u) sendInput(braking, sequence, now, 0, 100);
        now += 50u;
        braking.update(now);
    }
    const uint16_t fast = braking.snapshot(now).speed_kph10;
    for (uint8_t i = 0; i < 8u; ++i) {
        if ((i & 1u) == 0u) sendInput(braking, sequence, now, 0, 0, 100);
        now += 50u;
        braking.update(now);
    }
    TEST_ASSERT_TRUE(braking.snapshot(now).speed_kph10 < fast);
}

void test_collision_causes_damage_and_audio_event() {
    TrailGameConfig config;
    config.seed = 0xC0111DEu;
    TrailAssaultGame game;
    game.configure(config);
    uint32_t now;
    startRunning(game, now);
    uint16_t sequence = 0;

    for (uint32_t frame = 0; frame < 1800u &&
         game.snapshot(now).damage_permille == 0u; ++frame) {
        TrailGameSnapshot snapshot = game.snapshot(now);
        int target = 0;
        if (snapshot.visible_objects) {
            const TrailObjectView& object = snapshot.objects[0];
            if (object.kind != TrailObstacleKind::GATE &&
                object.kind != TrailObstacleKind::JUMP_RAMP)
                target = static_cast<int>(object.lateral) * 10;
        }
        if ((frame & 1u) == 0u) {
            const int steer = snapshot.lateral_permille < target - 20 ? 100
                : snapshot.lateral_permille > target + 20 ? -100 : 0;
            sendInput(game, sequence, now, steer, 100);
        }
        now += 50u;
        game.update(now);
    }
    TrailGameSnapshot result = game.snapshot(now);
    TEST_ASSERT_TRUE(result.damage_permille > 0u);

    AudioEvent events[AudioEventBus::CAPACITY];
    uint8_t count = game.audioEvents().readAfter(0, events,
                                                  AudioEventBus::CAPACITY);
    bool collision = false;
    for (uint8_t i = 0; i < count; ++i)
        collision |= events[i].type == AudioEventType::COLLISION_SMALL ||
                     events[i].type == AudioEventType::COLLISION_LARGE;
    TEST_ASSERT_TRUE(collision);
}

void test_all_three_modes_reach_their_objective() {
    const TrailMode modes[] = {TrailMode::TIME_ATTACK,
                               TrailMode::GATE_RUSH,
                               TrailMode::HILL_CLIMB};
    for (uint8_t i = 0; i < 3u; ++i) {
        TrailGameConfig config;
        config.mode = modes[i];
        config.seed = 0x44000010u + i;
        TrailAssaultGame game;
        game.configure(config);
        uint32_t now;
        startRunning(game, now);
        uint16_t sequence = 0;
        TrailGameSnapshot result = driveToResult(game, now, sequence);
        TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(TrailStatus::FINISHED),
                                      static_cast<int>(result.status),
                                      "mode did not finish cleanly");
        TEST_ASSERT_TRUE(result.course_clear);
        TEST_ASSERT_EQUAL_UINT16(1000u, result.progress_permille);
        TEST_ASSERT_EQUAL_UINT8(result.total_checkpoints, result.checkpoint);
        if (modes[i] == TrailMode::GATE_RUSH)
            TEST_ASSERT_EQUAL_UINT8(result.total_gates, result.gate);
    }
}

void test_ghost_crc_load_and_delta_view() {
    TrailGameConfig config;
    config.mode = TrailMode::TIME_ATTACK;
    config.seed = 0x600D4400u;
    TrailAssaultGame game;
    game.configure(config);
    uint32_t now;
    startRunning(game, now);
    uint16_t sequence = 0;
    TrailGameSnapshot result = driveToResult(game, now, sequence);
    TEST_ASSERT_TRUE(result.course_clear);

    TrailGhostRecording recording = game.bestGhost();
    TEST_ASSERT_TRUE(TrailAssaultGame::ghostValid(recording));
    TEST_ASSERT_TRUE(recording.sample_count > 20u);
    TEST_ASSERT_TRUE(recording.sample_count <=
                     TrailGhostRecording::MAX_SAMPLES);
    const TrailGhostSample& finish =
        recording.samples[recording.sample_count - 1u];
    TEST_ASSERT_INT_WITHIN(1, recording.finish_ms / 100u, finish.time_ds);
    TEST_ASSERT_INT_WITHIN(1, game.courseLengthCm() / 10u,
                           finish.distance_dm);
    TEST_ASSERT_EQUAL_UINT32(recording.crc32,
                             TrailAssaultGame::ghostCrc(recording));

    TrailAssaultGame replay;
    replay.configure(config);
    TEST_ASSERT_TRUE(replay.loadBestGhost(recording));
    replay.start(0, SESSION);
    replay.update(3000);
    TEST_ASSERT_TRUE(replay.snapshot(3000).ghost.available);

    recording.samples[0].distance_dm ^= 1u;
    TEST_ASSERT_FALSE(TrailAssaultGame::ghostValid(recording));
    TEST_ASSERT_FALSE(replay.loadBestGhost(recording));
}

void test_event_age_and_millis_wrap_are_safe() {
    TrailAssaultGame game;
    const uint32_t start = 0xFFFFFF00u;
    game.start(start, SESSION);
    game.update(start + 3000u);
    TrailGameSnapshot running = game.snapshot(start + 3000u);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailStatus::RUNNING),
                          static_cast<int>(running.status));

    uint16_t sequence = 0;
    sendInput(game, sequence, start + 3000u, 0, 80);
    game.update(start + 3100u);
    TEST_ASSERT_TRUE(game.snapshot(start + 3100u).tick > running.tick);

    game.pause(start + 3100u);
    TrailGameSnapshot paused = game.snapshot(start + 3100u);
    TEST_ASSERT_TRUE(paused.event_age_ms == 0xFFFFu ||
                     paused.event_age_ms < 2000u);
}

void test_experience_wraps_game_profile_audio_and_neutral() {
    ExperienceEngine experience;
    experience.reset(0);
    TrailGameConfig game_config;
    game_config.mode = TrailMode::HILL_CLIMB;
    game_config.seed = 77u;
    experience.configureGame(game_config);
    experience.enterGameAttract(10);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ExperienceMode::GAME),
        static_cast<int>(experience.frame(10).mode));
    experience.startGame(20, SESSION);
    VehicleState vehicle;
    UiFx fx;
    experience.update(3020, vehicle, fx);

    TrailInput input;
    input.session_id = SESSION;
    input.sequence = 1;
    input.received_ms = 3020;
    input.throttle = 100;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
                          static_cast<int>(experience.controlGame(input)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TrailInputResult::ACCEPTED),
        static_cast<int>(experience.neutralGame(SESSION, 2, 3030)));
    TEST_ASSERT_TRUE(experience.frame(3030).game.controller.neutral);

    AudioEvent events[4];
    TEST_ASSERT_TRUE(experience.readAudioEvents(0, events, 4) > 0u);
    experience.stopGame();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ExperienceMode::NORMAL),
        static_cast<int>(experience.frame(3040).mode));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_compact_trivial_contracts);
    RUN_TEST(test_audio_bus_orders_and_overwrites_without_heap);
    RUN_TEST(test_seed_generation_is_deterministic_bounded_and_complete);
    RUN_TEST(test_gate_and_hill_generators_enforce_objectives);
    RUN_TEST(test_fixed_step_is_invariant_to_update_cadence);
    RUN_TEST(test_controller_session_sequence_timeout_and_neutral);
    RUN_TEST(test_running_stall_is_bounded_and_future_input_stays_fresh);
    RUN_TEST(test_countdown_never_refreshes_stale_controller_input);
    RUN_TEST(test_long_countdown_stall_resynchronizes_input_clock);
    RUN_TEST(test_pause_preserves_sequence_replay_barrier);
    RUN_TEST(test_brake_reduces_speed_and_terrain_changes_grip);
    RUN_TEST(test_collision_causes_damage_and_audio_event);
    RUN_TEST(test_all_three_modes_reach_their_objective);
    RUN_TEST(test_ghost_crc_load_and_delta_view);
    RUN_TEST(test_event_age_and_millis_wrap_are_safe);
    RUN_TEST(test_experience_wraps_game_profile_audio_and_neutral);
    return UNITY_END();
}
