#include "core/wow_state.h"
#include <unity.h>

using Wow::Control;
using Wow::Kind;
using Wow::State;

void setUp() {}
void tearDown() {}

static void advance(State &s, uint32_t from, uint32_t to) {
  for (uint32_t t = from + 20; t <= to; t += 20)
    s.tick(t);
}

static void drive(State &s, bool seekCollision) {
  for (uint32_t t = 20; t <= 60000; t += 20) {
    if (t % 100 == 0) {
      const float target = seekCollision
                               ? s.s.obstacleLane
                               : (s.s.obstacleLane > 0 ? -.75f : .75f);
      const float gap = target - s.s.car;
      s.control(Control::Steer, gap > .06f ? 2 : (gap < -.06f ? 0 : 1), t);
      s.control(Control::Heartbeat, 0, t);
    }
    s.tick(t);
  }
}

void entry_resets_modes_and_preserves_best() {
  State s;
  s.s.best = 1234;
  s.enter(Kind::Midnight, 50, 0);
  TEST_ASSERT_EQUAL_UINT32(1, s.s.seed);
  TEST_ASSERT_EQUAL_UINT32(1234, s.s.best);
  s.control(Control::Steer, 2, 50);
  s.tick(70);
  TEST_ASSERT_TRUE(s.s.car > 0);
  s.enter(Kind::Beat, 80, 42);
  TEST_ASSERT_EQUAL_INT(int(Kind::Beat), int(s.s.kind));
  TEST_ASSERT_EQUAL_UINT32(1234, s.s.best);
  TEST_ASSERT_EQUAL_UINT32(0, s.s.elapsed);
  TEST_ASSERT_EQUAL_UINT32(0, s.s.ms);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.car);
  TEST_ASSERT_FALSE(s.s.audioActive);
}

void midnight_seeded_replay_and_course_variation() {
  State a, b, c;
  a.enter(Kind::Midnight, 0, 7123);
  b.enter(Kind::Midnight, 0, 7123);
  c.enter(Kind::Midnight, 0, 7124);
  drive(a, false);
  drive(b, false);
  drive(c, false);
  TEST_ASSERT_TRUE(a.s.done);
  TEST_ASSERT_EQUAL_UINT32(60000, a.s.elapsed);
  TEST_ASSERT_EQUAL_UINT32(a.s.score, b.s.score);
  TEST_ASSERT_EQUAL_UINT16(a.s.hits, b.s.hits);
  TEST_ASSERT_EQUAL_FLOAT(a.s.car, b.s.car);
  TEST_ASSERT_EQUAL_FLOAT(a.s.distance, b.s.distance);
  TEST_ASSERT_TRUE(fabsf(a.s.curve - c.s.curve) > .001f);
  TEST_ASSERT_TRUE(a.s.best > 0);
}

void midnight_collision_changes_result_and_finish_is_frozen() {
  State collision, avoid;
  collision.enter(Kind::Midnight, 0, 17);
  avoid.enter(Kind::Midnight, 0, 17);
  drive(collision, true);
  drive(avoid, false);
  TEST_ASSERT_TRUE(collision.s.hits > 0);
  TEST_ASSERT_TRUE(collision.s.hits > avoid.s.hits);
  TEST_ASSERT_TRUE(collision.s.score < avoid.s.score);
  TEST_ASSERT_EQUAL_UINT32(collision.s.score, collision.s.best);
  const auto result = collision.s;
  collision.tick(99999);
  collision.control(Control::Steer, 2, 100000);
  collision.control(Control::Heartbeat, 0, 100000);
  TEST_ASSERT_EQUAL_UINT32(result.elapsed, collision.s.elapsed);
  TEST_ASSERT_EQUAL_UINT32(result.score, collision.s.score);
  TEST_ASSERT_EQUAL_UINT16(result.hits, collision.s.hits);
  TEST_ASSERT_EQUAL_UINT32(result.ms, collision.s.ms);
  TEST_ASSERT_FALSE(collision.s.paused);
}

void midnight_controller_timeout_and_foreground_resume() {
  State s;
  s.enter(Kind::Midnight, 0);
  advance(s, 0, 2500);
  TEST_ASSERT_FALSE(s.s.paused);
  TEST_ASSERT_EQUAL_UINT32(2500, s.s.elapsed);
  s.tick(2520);
  TEST_ASSERT_TRUE(s.s.paused);
  s.tick(90000);
  TEST_ASSERT_EQUAL_UINT32(2500, s.s.elapsed);
  s.control(Control::Heartbeat, 0, 90020);
  TEST_ASSERT_FALSE(s.s.paused);
  s.tick(90040);
  TEST_ASSERT_EQUAL_UINT32(2520, s.s.elapsed);
  s.control(Control::Background, 0, 90040);
  s.tick(90100);
  TEST_ASSERT_TRUE(s.s.paused);
  TEST_ASSERT_EQUAL_UINT32(2520, s.s.elapsed);
  s.control(Control::Heartbeat, 1, 90200);
  s.tick(90220);
  TEST_ASSERT_EQUAL_UINT32(2540, s.s.elapsed);
}

void midnight_steering_expires_even_while_heartbeat_is_alive() {
  State a, b;
  a.enter(Kind::Midnight, 0, 2);
  b.enter(Kind::Midnight, 0, 2);
  a.control(Control::Steer, 0, 0);
  b.control(Control::Steer, 0, 0);
  advance(a, 0, 480);
  advance(b, 0, 480);
  b.control(Control::Steer, 1, 480);
  for (uint32_t t = 500; t <= 2000; t += 20) {
    if (t % 200 == 0) {
      a.control(Control::Heartbeat, 0, t);
      b.control(Control::Heartbeat, 0, t);
    }
    a.tick(t);
    b.tick(t);
  }
  TEST_ASSERT_EQUAL_FLOAT(a.s.car, b.s.car);
  TEST_ASSERT_FALSE(a.s.paused);
}

void midnight_boot_pause_and_explicit_resume() {
  State s;
  s.enter(Kind::Midnight, 0);
  s.tick(20);
  s.control(Control::Impulse, 0, 20);
  TEST_ASSERT_TRUE(s.s.paused);
  s.tick(2000);
  TEST_ASSERT_EQUAL_UINT32(20, s.s.elapsed);
  s.control(Control::Heartbeat, 1, 2000);
  TEST_ASSERT_FALSE(s.s.paused);
  s.tick(2020);
  TEST_ASSERT_EQUAL_UINT32(40, s.s.elapsed);
  s.control(Control::Impulse, 0, 2020);
  TEST_ASSERT_TRUE(s.s.paused);
  s.control(Control::Impulse, 0, 2040);
  TEST_ASSERT_FALSE(s.s.paused);
  s.tick(5000);
  TEST_ASSERT_TRUE(s.s.paused);
  s.control(Control::Impulse, 0, 5000);
  TEST_ASSERT_FALSE(s.s.paused);
}

void fixed_step_discards_backlog_and_preserves_substeps() {
  State s;
  s.enter(Kind::Living, 0);
  s.tick(19);
  TEST_ASSERT_EQUAL_UINT32(0, s.s.ms);
  s.tick(20);
  TEST_ASSERT_EQUAL_UINT32(20, s.s.ms);
  s.tick(10020);
  TEST_ASSERT_EQUAL_UINT32(120, s.s.ms);
  s.tick(10020);
  TEST_ASSERT_EQUAL_UINT32(120, s.s.ms);
}

void automatic_heartbeat_cannot_cancel_manual_or_background_pause() {
  State s;
  s.enter(Kind::Midnight, 0);
  s.tick(20);
  s.control(Control::Impulse, 0, 20);
  s.control(Control::Heartbeat, 0, 40);
  s.tick(60);
  TEST_ASSERT_TRUE(s.s.paused);
  TEST_ASSERT_EQUAL_UINT32(20, s.s.elapsed);
  s.control(Control::Heartbeat, 1, 60);
  s.control(Control::Heartbeat, 1, 60);
  TEST_ASSERT_FALSE(s.s.paused);
  s.tick(80);
  TEST_ASSERT_EQUAL_UINT32(40, s.s.elapsed);
  s.control(Control::Background, 0, 80);
  s.control(Control::Heartbeat, 0, 100);
  s.control(Control::Steer, 2, 100);
  s.tick(120);
  TEST_ASSERT_TRUE(s.s.paused);
  TEST_ASSERT_EQUAL_UINT32(40, s.s.elapsed);
  s.control(Control::Heartbeat, 1, 120);
  TEST_ASSERT_FALSE(s.s.paused);
}

void clock_wrap_matches_normal_time() {
  State a, b;
  const uint32_t origin = UINT32_MAX - 99;
  a.enter(Kind::Midnight, 0, 8);
  b.enter(Kind::Midnight, origin, 8);
  a.control(Control::Steer, 2, 0);
  b.control(Control::Steer, 2, origin);
  for (uint32_t t = 20; t <= 2000; t += 20) {
    a.tick(t);
    b.tick(origin + t);
  }
  TEST_ASSERT_EQUAL_UINT32(a.s.elapsed, b.s.elapsed);
  TEST_ASSERT_EQUAL_FLOAT(a.s.car, b.s.car);
  TEST_ASSERT_EQUAL_FLOAT(a.s.distance, b.s.distance);
  a.tick(2520);
  b.tick(origin + 2520);
  TEST_ASSERT_TRUE(a.s.paused);
  TEST_ASSERT_TRUE(b.s.paused);
}

void living_gaze_blink_and_expiring_event_memory() {
  State s;
  s.react(4, 50);
  s.enter(Kind::Living, 100);
  TEST_ASSERT_EQUAL_UINT8(4, s.s.mood);
  s.control(Control::Touch, 65535, 100);
  advance(s, 100, 700);
  TEST_ASSERT_TRUE(s.s.gazeX > .9f);
  TEST_ASSERT_TRUE(s.s.gazeY > .9f);
  s.control(Control::Impulse, 0, 700);
  TEST_ASSERT_TRUE(s.s.blink > 0);
  advance(s, 700, 1000);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.blink);
  advance(s, 1000, 5100);
  TEST_ASSERT_EQUAL_UINT8(0, s.s.mood);
  TEST_ASSERT_TRUE(s.s.gazeY < .5f);
  s.react(2, 5100);
  s.enter(Kind::Garage, 5120);
  s.enter(Kind::Living, 5160);
  TEST_ASSERT_EQUAL_UINT8(2, s.s.mood);
  s.react(255, 5160);
  TEST_ASSERT_EQUAL_UINT8(2, s.s.mood);
}

void liquid_drag_and_impulse_change_bounded_motion() {
  State base, touched;
  base.enter(Kind::Liquid, 0);
  touched.enter(Kind::Liquid, 0);
  touched.control(Control::Touch, 0xFF20, 0);
  TEST_ASSERT_EQUAL_FLOAT(1, touched.s.gazeX);
  TEST_ASSERT_TRUE(touched.s.gazeY < -.7f);
  for (uint32_t t = 20; t <= 120000; t += 20) {
    if (t % 1400 == 0)
      touched.control(Control::Impulse, 0, t);
    if (t % 600 == 0)
      touched.control(Control::Touch, ((t / 600) % 2) ? 0xFF00 : 0x00FF, t);
    base.tick(t);
    touched.tick(t);
    for (unsigned i = 0; i < 48; ++i) {
      const auto &p = touched.s.particles[i];
      TEST_ASSERT_TRUE(isfinite(p.x) && isfinite(p.y));
      TEST_ASSERT_TRUE(p.x >= .035f && p.x <= .965f);
      TEST_ASSERT_TRUE(p.y >= .06f && p.y <= .94f);
    }
    TEST_ASSERT_TRUE(touched.s.energy >= .05f && touched.s.energy <= 1);
  }
  float difference = 0;
  for (unsigned i = 0; i < 48; ++i)
    difference += fabsf(base.s.particles[i].x - touched.s.particles[i].x);
  TEST_ASSERT_TRUE(difference > 1);
}

void liquid_is_deterministic_for_same_input() {
  State a, b;
  a.enter(Kind::Liquid, 0, 32);
  b.enter(Kind::Liquid, 0, 32);
  for (uint32_t t = 20; t <= 3000; t += 20) {
    if (t == 600) {
      a.control(Control::Impulse, 0, t);
      b.control(Control::Impulse, 0, t);
    }
    if (t == 1000) {
      a.control(Control::Touch, 0x5588, t);
      b.control(Control::Touch, 0x5588, t);
    }
    a.tick(t);
    b.tick(t);
  }
  for (unsigned i = 0; i < 48; ++i) {
    TEST_ASSERT_EQUAL_FLOAT(a.s.particles[i].x, b.s.particles[i].x);
    TEST_ASSERT_EQUAL_FLOAT(a.s.particles[i].y, b.s.particles[i].y);
  }
}

void garage_explodes_reassembles_and_orbits() {
  State s;
  s.enter(Kind::Garage, 0);
  s.control(Control::Explode, 1, 0);
  advance(s, 0, 2200);
  TEST_ASSERT_EQUAL_FLOAT(1, s.s.exploded);
  s.control(Control::Orbit, 0xFF00, 2200);
  advance(s, 2200, 3200);
  TEST_ASSERT_FLOAT_WITHIN(.02f, 3.14159f, fabsf(s.s.yaw));
  TEST_ASSERT_FLOAT_WITHIN(.02f, .45f, s.s.tilt);
  s.control(Control::Impulse, 0, 3200);
  advance(s, 3200, 5400);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.exploded);
  TEST_ASSERT_TRUE(fabsf(s.s.yaw) <= 3.1416f);
  TEST_ASSERT_TRUE(s.s.tilt < .4f);
}

void beat_band_order_expiry_decay_and_background() {
  State s;
  s.enter(Kind::Beat, 0);
  s.control(Control::Audio, 0xA04010FF, 0);
  s.tick(20);
  TEST_ASSERT_TRUE(s.s.audioActive);
  TEST_ASSERT_EQUAL_UINT8(127, s.s.bands[0]);
  TEST_ASSERT_EQUAL_UINT8(8, s.s.bands[1]);
  TEST_ASSERT_EQUAL_UINT8(32, s.s.bands[2]);
  TEST_ASSERT_EQUAL_UINT8(80, s.s.bands[3]);
  advance(s, 20, 480);
  TEST_ASSERT_TRUE(s.s.audioActive);
  s.tick(500);
  TEST_ASSERT_FALSE(s.s.audioActive);
  advance(s, 500, 900);
  for (unsigned i = 0; i < 4; ++i)
    TEST_ASSERT_EQUAL_UINT8(0, s.s.bands[i]);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.energy);
  s.control(Control::Audio, UINT32_MAX, 900);
  s.tick(920);
  TEST_ASSERT_TRUE(s.s.bands[0] > 0);
  s.control(Control::Background, 0, 920);
  TEST_ASSERT_FALSE(s.s.audioActive);
  for (unsigned i = 0; i < 4; ++i)
    TEST_ASSERT_EQUAL_UINT8(0, s.s.bands[i]);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.energy);
}

void beat_silence_palette_and_clock_wrap() {
  State s;
  const uint32_t origin = UINT32_MAX - 199;
  s.enter(Kind::Beat, origin);
  s.control(Control::Audio, 0, origin);
  s.tick(origin + 20);
  TEST_ASSERT_TRUE(s.s.audioActive);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.energy);
  s.tick(origin + 500);
  TEST_ASSERT_FALSE(s.s.audioActive);
  for (unsigned i = 1; i <= 5; ++i) {
    s.control(Control::Impulse, 0, origin + 500);
    TEST_ASSERT_EQUAL_UINT8(i % 4, s.s.palette);
  }
}

void controls_ignore_incompatible_modes_and_invalid_ranges() {
  State s;
  s.enter(Kind::Living, 0);
  s.control(Control::Audio, UINT32_MAX, 0);
  s.control(Control::Steer, 2, 0);
  s.control(Control::Touch, 65536, 0);
  s.tick(20);
  TEST_ASSERT_FALSE(s.s.audioActive);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.car);
  TEST_ASSERT_TRUE(fabsf(s.s.gazeX) < .1f);
  s.enter(Kind::Garage, 20);
  s.control(Control::Explode, 2, 20);
  s.control(Control::Orbit, 65536, 20);
  s.tick(40);
  TEST_ASSERT_EQUAL_FLOAT(0, s.s.exploded);
  TEST_ASSERT_FLOAT_WITHIN(.001f, .009f, s.s.yaw);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(entry_resets_modes_and_preserves_best);
  RUN_TEST(midnight_seeded_replay_and_course_variation);
  RUN_TEST(midnight_collision_changes_result_and_finish_is_frozen);
  RUN_TEST(midnight_controller_timeout_and_foreground_resume);
  RUN_TEST(midnight_steering_expires_even_while_heartbeat_is_alive);
  RUN_TEST(midnight_boot_pause_and_explicit_resume);
  RUN_TEST(fixed_step_discards_backlog_and_preserves_substeps);
  RUN_TEST(automatic_heartbeat_cannot_cancel_manual_or_background_pause);
  RUN_TEST(clock_wrap_matches_normal_time);
  RUN_TEST(living_gaze_blink_and_expiring_event_memory);
  RUN_TEST(liquid_drag_and_impulse_change_bounded_motion);
  RUN_TEST(liquid_is_deterministic_for_same_input);
  RUN_TEST(garage_explodes_reassembles_and_orbits);
  RUN_TEST(beat_band_order_expiry_decay_and_background);
  RUN_TEST(beat_silence_palette_and_clock_wrap);
  RUN_TEST(controls_ignore_incompatible_modes_and_invalid_ranges);
  return UNITY_END();
}
