#include "core/storm_protocol.h"
#include <string.h>
#include <unity.h>
using namespace Storm;
static void act(State &state, Action action, uint32_t at, uint32_t value = 0) {
  Command command;
  command.action = action;
  command.value = value;
  state.apply(command, at);
}
void wire_contract() {
  TEST_ASSERT_EQUAL_STRING("board", actionName(15));
  TEST_ASSERT_EQUAL_STRING("midnight", actionName(16));
  TEST_ASSERT_EQUAL_STRING("audio", actionName(26));
  uint32_t value = 123;
  TEST_ASSERT_TRUE(parseValue("4294967295", 10, value));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, value);
  TEST_ASSERT_FALSE(parseValue("4294967296", 10, value));
  TEST_ASSERT_FALSE(parseValue("-1", 2, value));
  TEST_ASSERT_FALSE(parseValue("1.5", 3, value));
  TEST_ASSERT_FALSE(parseValue("1e3", 3, value));
  TEST_ASSERT_FALSE(parseValue("99999999999", 11, value));
  TEST_ASSERT_TRUE(parseValue("", 0, value));
  TEST_ASSERT_EQUAL(0, value);
  TEST_ASSERT_FALSE(validValue(Action::Steer, 3));
  TEST_ASSERT_FALSE(validValue(Action::Touch, 65536));
  TEST_ASSERT_FALSE(validValue(Action::Orbit, 65536));
  TEST_ASSERT_FALSE(validValue(Action::Explode, 2));
  TEST_ASSERT_FALSE(validValue(Action::Rules, 101));
  TEST_ASSERT_FALSE(validValue(Action::Exit, 1));
  TEST_ASSERT_TRUE(validValue(Action::Audio, UINT32_MAX));
  TEST_ASSERT_FALSE(validValue(Action::Count, 0));
}
void modes_and_return() {
  State state;
  for (unsigned kind = 0; kind < 5; ++kind) {
    act(state, static_cast<Action>(16 + kind), kind * 1000, kind == 0 ? 42 : 0);
    TEST_ASSERT_EQUAL(5 + kind, unsigned(state.s.mode));
    TEST_ASSERT_EQUAL(kind, unsigned(state.s.wow.kind));
    state.tick(kind * 1000 + 100);
    TEST_ASSERT_EQUAL(100, state.s.wow.ms);
    act(state, Action::Exit, kind * 1000 + 100);
    TEST_ASSERT_EQUAL(unsigned(Mode::Instruments), unsigned(state.s.mode));
    state.tick(kind * 1000 + 500);
    TEST_ASSERT_EQUAL(100, state.s.wow.ms);
  }
}
void trail_survives_modes_and_reacts() {
  State state;
  act(state, Action::Start, 0);
  act(state, Action::Living, 100);
  act(state, Action::Gate, 1000);
  TEST_ASSERT_EQUAL(1, state.s.wow.mood);
  act(state, Action::Penalty, 2000);
  TEST_ASSERT_EQUAL(2, state.s.wow.mood);
  act(state, Action::Pause, 3000);
  act(state, Action::Liquid, 4000);
  act(state, Action::Resume, 5000);
  act(state, Action::Finish, 7000);
  TEST_ASSERT_EQUAL(5000, state.s.closed.duration);
  TEST_ASSERT_EQUAL(1, state.s.closed.gates);
  TEST_ASSERT_EQUAL(1, state.s.closed.penalties);
  TEST_ASSERT_EQUAL(4, state.s.closed.score);
  TEST_ASSERT_EQUAL(3, state.s.wow.mood);
  act(state, Action::Living, 7100);
  TEST_ASSERT_EQUAL(3, state.s.wow.mood);
}
void controls_are_mode_scoped() {
  State state;
  act(state, Action::Beat, 0);
  act(state, Action::Audio, 20, 0xFF804020u);
  state.tick(40);
  TEST_ASSERT_TRUE(state.s.wow.audioActive);
  act(state, Action::Background, 50);
  TEST_ASSERT_FALSE(state.s.wow.audioActive);
  TEST_ASSERT_EQUAL(0, state.s.wow.bands[0]);
  act(state, Action::Living, 100);
  act(state, Action::Audio, 120, UINT32_MAX);
  TEST_ASSERT_FALSE(state.s.wow.audioActive);
  act(state, Action::Garage, 200);
  act(state, Action::Explode, 220, 1);
  state.tick(320);
  TEST_ASSERT_TRUE(state.s.wow.exploded > 0);
  act(state, Action::Midnight, 400, 17);
  act(state, Action::Background, 420);
  auto elapsed = state.s.wow.elapsed;
  state.tick(2000);
  TEST_ASSERT_TRUE(state.s.wow.paused);
  TEST_ASSERT_EQUAL(elapsed, state.s.wow.elapsed);
  act(state, Action::Heartbeat, 2000, 1);
  state.tick(2020);
  TEST_ASSERT_FALSE(state.s.wow.paused);
  TEST_ASSERT_EQUAL(elapsed + 20, state.s.wow.elapsed);
}
void record_integrity_and_mode_switch() {
  MidnightRecord record;
  TEST_ASSERT_TRUE(validRecord(record));
  record.best = 12345;
  TEST_ASSERT_FALSE(validRecord(record));
  record.check = ~record.best;
  TEST_ASSERT_TRUE(validRecord(record));
  record.magic ^= 1;
  TEST_ASSERT_FALSE(validRecord(record));
  State state;
  state.wow.s.best = 12345;
  act(state, Action::Midnight, 0, 42);
  act(state, Action::Exit, 100);
  act(state, Action::Liquid, 200);
  TEST_ASSERT_EQUAL(12345, state.s.wow.best);
  TEST_ASSERT_EQUAL(0, state.s.best);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(wire_contract);
  RUN_TEST(modes_and_return);
  RUN_TEST(trail_survives_modes_and_reacts);
  RUN_TEST(controls_are_mode_scoped);
  RUN_TEST(record_integrity_and_mode_switch);
  return UNITY_END();
}
