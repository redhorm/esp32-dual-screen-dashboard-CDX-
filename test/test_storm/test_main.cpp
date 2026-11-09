#include "core/storm_archive.h"
#include "core/storm_state.h"
#include <unity.h>
using namespace Storm;
static void act(State &s, Action a, uint32_t now, uint32_t value = 0) {
  Command c;
  c.action = a;
  c.value = value;
  s.apply(c, now);
}
void timer() {
  State s;
  act(s, Action::Start, 0);
  s.tick(10000);
  TEST_ASSERT_EQUAL(10000, s.s.elapsed);
  act(s, Action::Pause, 11000);
  s.tick(50000);
  TEST_ASSERT_EQUAL(11000, s.s.elapsed);
  act(s, Action::Resume, 50000);
  act(s, Action::Finish, 55000);
  TEST_ASSERT_EQUAL(16000, s.s.closed.duration);
  act(s, Action::Start, 56000);
  TEST_ASSERT_EQUAL(16000, s.s.closed.duration);
  TEST_ASSERT_EQUAL(0, s.s.elapsed);
}
void events() {
  State s;
  act(s, Action::Rules, 0, 7);
  act(s, Action::Start, 0);
  act(s, Action::Gate, 0);
  act(s, Action::Penalty, 0);
  TEST_ASSERT_EQUAL(6, s.s.score);
  act(s, Action::Undo, 0);
  TEST_ASSERT_EQUAL(-1, s.s.score);
  for (int i = 0; i < 200; i++)
    act(s, Action::Gate, 0);
  TEST_ASSERT_EQUAL(128, s.s.gates);
  act(s, Action::Rules, 0, 99);
  TEST_ASSERT_EQUAL(7, s.s.weight);
}
void wrap() {
  State s;
  act(s, Action::Start, UINT32_MAX - 100);
  s.tick(99);
  TEST_ASSERT_EQUAL(200, s.s.elapsed);
}
void timeout() {
  State s;
  act(s, Action::Game, 0, 42);
  s.tick(20);
  s.tick(3000);
  TEST_ASSERT_TRUE(s.s.gamePaused);
  auto t = s.s.gameMs;
  act(s, Action::Jump, 3010);
  s.tick(3030);
  TEST_ASSERT_FALSE(s.s.gamePaused);
  TEST_ASSERT_EQUAL(t + 20, s.s.gameMs);
  act(s, Action::Background, 3040);
  s.tick(3100);
  TEST_ASSERT_TRUE(s.s.gamePaused);
}
void deterministic() {
  State a, b;
  act(a, Action::Game, 0, 42);
  act(b, Action::Game, 0, 42);
  for (unsigned t = 20; t <= 45000; t += 20) {
    if (t % 500 == 0) {
      act(a, Action::Heartbeat, t);
      act(b, Action::Heartbeat, t);
    }
    if (t % 900 == 0) {
      act(a, Action::Jump, t);
      act(b, Action::Jump, t);
    }
    a.tick(t);
    b.tick(t);
  }
  TEST_ASSERT_TRUE(a.s.gameDone);
  TEST_ASSERT_EQUAL(a.s.hits, b.s.hits);
  TEST_ASSERT_EQUAL(a.s.distance, b.s.distance);
  TEST_ASSERT_TRUE(a.s.best > 0);
}
void show() {
  State s;
  act(s, Action::Show, 0);
  s.tick(5999);
  TEST_ASSERT_EQUAL((int)Mode::Show, (int)s.s.mode);
  s.tick(6000);
  TEST_ASSERT_EQUAL((int)Mode::Instruments, (int)s.s.mode);
}
void long_session() {
  State s;
  act(s, Action::Start, 0);
  s.tick(90000000);
  TEST_ASSERT_EQUAL(86400000, s.s.elapsed);
  TEST_ASSERT_FALSE(s.s.running);
}
void archive_integrity() {
  Archive a;
  Result r;
  for (unsigned i = 0; i < 100; i++) {
    r.duration = i;
    appendResult(a, r);
  }
  TEST_ASSERT_EQUAL(64, a.count);
  TEST_ASSERT_EQUAL(36, a.results[0].duration);
  a.checksum = archiveChecksum(a);
  TEST_ASSERT_TRUE(validArchive(a));
  a.results[0].duration++;
  TEST_ASSERT_FALSE(validArchive(a));
  a.count = 65;
  a.checksum = archiveChecksum(a);
  TEST_ASSERT_FALSE(validArchive(a));
}
void ghost_bounds() {
  State s;
  uint8_t memory[4502] = {};
  memory[0] = 71;
  memory[4501] = 91;
  s.attachGhost(memory + 1, memory + 2251);
  act(s, Action::Game, 0, 42);
  act(s, Action::Jump, 0, 1);
  for (unsigned t = 20; t <= 45000; t += 20)
    s.tick(t);
  TEST_ASSERT_TRUE(s.s.gameDone);
  TEST_ASSERT_EQUAL(71, memory[0]);
  TEST_ASSERT_EQUAL(91, memory[4501]);
  act(s, Action::Game, 46000, 42);
  act(s, Action::Jump, 46000, 1);
  s.tick(46020);
  TEST_ASSERT_TRUE(s.s.ghostJump >= 0);
  act(s, Action::Game, 47000, 43);
  s.tick(47020);
  TEST_ASSERT_EQUAL(-1, s.s.ghostJump);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(archive_integrity);
  RUN_TEST(ghost_bounds);
  RUN_TEST(timer);
  RUN_TEST(events);
  RUN_TEST(wrap);
  RUN_TEST(timeout);
  RUN_TEST(deterministic);
  RUN_TEST(show);
  RUN_TEST(long_session);
  return UNITY_END();
}
