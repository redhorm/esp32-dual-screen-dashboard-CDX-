#pragma once
#include "wow_state.h"
#include <stddef.h>
#include <stdint.h>
namespace Storm {
enum class Mode : uint8_t {
  Instruments,
  Show,
  Game,
  Pit,
  Board,
  Midnight,
  Living,
  Liquid,
  Garage,
  Beat
};
inline bool isWow(Mode mode) {
  return mode >= Mode::Midnight && mode <= Mode::Beat;
}
enum class Action : uint8_t {
  Start,
  Pause,
  Resume,
  Finish,
  Gate,
  Penalty,
  Undo,
  Rules,
  Show,
  Exit,
  Game,
  Jump,
  Heartbeat,
  Background,
  Pit,
  Board,
  Midnight,
  Living,
  Liquid,
  Garage,
  Beat,
  Steer,
  Touch,
  Impulse,
  Explode,
  Orbit,
  Audio,
  Count
};
struct Command {
  Action action;
  uint32_t value = 0;
  bool test = false;
};
struct Result {
  uint32_t duration = 0;
  int16_t gates = 0, penalties = 0, score = 0;
  uint16_t penaltyWeight = 5;
};
struct Snapshot {
  Result closed;
  Wow::Snapshot wow;
  Mode mode = Mode::Instruments;
  bool running = false, session = false, finished = false, gamePaused = false,
       gameDone = false;
  uint32_t elapsed = 0, showMs = 0, gameMs = 0, seed = 1, best = 0,
           revision = 0, finishedRevision = 0;
  uint16_t gates = 0, penalties = 0, weight = 5;
  int score = 0;
  float jump = 0;
  uint16_t hits = 0;
  uint32_t distance = 0;
  int obstacle = 240;
  int ghostJump = -1;
};
class State {
public:
  Snapshot s;
  Wow::State wow;
  uint8_t events[128] = {};
  uint16_t count = 0;
  uint32_t last = 0, showStart = 0, heartbeat = 0, stepRemainder = 0, rng = 1;
  float vy = 0;
  bool initialized = false, hit = false, localController = false,
       testSession = false;
  uint8_t *recording = nullptr;
  uint8_t *ghost = nullptr;
  uint32_t ghostSeed = 0, finishAt = 0;
  uint16_t ghostHits = 65535;
  void attachGhost(uint8_t *a, uint8_t *b) {
    recording = a;
    ghost = b;
  }
  static uint32_t random(uint32_t &x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }
  void apply(Command c, uint32_t now) {
    tick(now);
    switch (c.action) {
    case Action::Start:
      if (!s.session) {
        s.mode = Mode::Instruments;
        testSession = c.test;
        s.session = true;
        s.running = true;
        s.finished = false;
        s.elapsed = 0;
        s.gates = s.penalties = 0;
        count = 0;
      }
      break;
    case Action::Pause:
      s.running = false;
      break;
    case Action::Resume:
      if (s.session)
        s.running = true;
      break;
    case Action::Finish:
      if (s.session) {
        s.closed = result();
        s.session = s.running = false;
        s.finished = true;
        ++s.finishedRevision;
        wow.react(3, now);
      }
      break;
    case Action::Gate:
    case Action::Penalty:
      if (s.session && count < 128) {
        bool p = c.action == Action::Penalty;
        events[count++] = p;
        if (p)
          ++s.penalties;
        else
          ++s.gates;
        wow.react(p ? 2 : 1, now);
      }
      break;
    case Action::Undo:
      if (s.session && count) {
        if (events[--count])
          --s.penalties;
        else
          --s.gates;
      }
      break;
    case Action::Rules:
      if (!s.session && c.value <= 100)
        s.weight = c.value;
      break;
    case Action::Show:
      s.mode = Mode::Show;
      showStart = now;
      s.showMs = 0;
      break;
    case Action::Exit:
      s.mode = Mode::Instruments;
      break;
    case Action::Game:
      localController = false;
      s.mode = Mode::Game;
      s.gameMs = 0;
      s.gameDone = false;
      s.gamePaused = false;
      s.seed = c.value ? c.value : 1;
      rng = s.seed;
      s.hits = 0;
      s.distance = 0;
      s.jump = 0;
      vy = 0;
      s.obstacle = 240;
      heartbeat = now;
      stepRemainder = 0;
      hit = false;
      s.ghostJump = -1;
      if (ghostSeed != s.seed) {
        ghostSeed = s.seed;
        ghostHits = 65535;
      }
      break;
    case Action::Jump:
      if (s.mode == Mode::Game && !s.gameDone) {
        if (c.value == 1)
          localController = true;
        heartbeat = now;
        s.gamePaused = false;
        if (s.jump == 0)
          vy = 5.5f;
      }
      break;
    case Action::Heartbeat:
      heartbeat = now;
      s.gamePaused = false;
      if (isWow(s.mode))
        wow.control(Wow::Control::Heartbeat, c.value, now);
      break;
    case Action::Background:
      if (s.mode == Mode::Game)
        s.gamePaused = true;
      heartbeat = now - 2501;
      if (isWow(s.mode))
        wow.control(Wow::Control::Background, c.value, now);
      break;
    case Action::Pit:
      s.mode = Mode::Pit;
      showStart = now;
      break;
    case Action::Board:
      s.mode = Mode::Board;
      break;
    case Action::Midnight:
    case Action::Living:
    case Action::Liquid:
    case Action::Garage:
    case Action::Beat: {
      unsigned kind = unsigned(c.action) - unsigned(Action::Midnight);
      s.mode = static_cast<Mode>(unsigned(Mode::Midnight) + kind);
      wow.enter(static_cast<Wow::Kind>(kind), now, c.value ? c.value : 1);
      break;
    }
    case Action::Steer:
      if (s.mode == Mode::Midnight && c.value <= 2)
        wow.control(Wow::Control::Steer, c.value, now);
      break;
    case Action::Touch:
      if ((s.mode == Mode::Living || s.mode == Mode::Liquid) &&
          c.value <= 65535)
        wow.control(Wow::Control::Touch, c.value, now);
      break;
    case Action::Impulse:
      if (isWow(s.mode))
        wow.control(Wow::Control::Impulse, c.value, now);
      break;
    case Action::Explode:
      if (s.mode == Mode::Garage && c.value <= 1)
        wow.control(Wow::Control::Explode, c.value, now);
      break;
    case Action::Orbit:
      if (s.mode == Mode::Garage && c.value <= 65535)
        wow.control(Wow::Control::Orbit, c.value, now);
      break;
    case Action::Audio:
      if (s.mode == Mode::Beat)
        wow.control(Wow::Control::Audio, c.value, now);
      break;
    case Action::Count:
      break;
    }
    s.wow = wow.s;
    s.score = int(s.penalties) * s.weight - int(s.gates);
    ++s.revision;
  }
  void tick(uint32_t now) {
    if (!initialized) {
      last = now;
      initialized = true;
    }
    uint32_t dt = now - last;
    last = now;
    if (s.running) {
      uint32_t room = 86400000 - s.elapsed;
      s.elapsed += dt < room ? dt : room;
      if (s.elapsed == 86400000)
        s.running = false;
    }
    if (s.mode == Mode::Show) {
      s.showMs = now - showStart;
      if (s.showMs >= 6000)
        s.mode = Mode::Instruments;
    }
    if (s.mode == Mode::Pit)
      s.showMs = now - showStart;
    if (isWow(s.mode)) {
      wow.tick(now);
      s.wow = wow.s;
    }
    if (s.mode != Mode::Game)
      return;
    if (s.gameDone) {
      s.showMs = now - finishAt;
      return;
    }
    if (!localController && now - heartbeat > 2500)
      s.gamePaused = true;
    if (s.gamePaused)
      return;
    // Fixed 20 ms steps: bounded catch-up, no frame-dependent physics.
    stepRemainder += dt > 100 ? 100 : dt;
    while (stepRemainder >= 20 && !s.gameDone) {
      stepRemainder -= 20;
      s.gameMs += 20;
      s.jump += vy;
      vy -= 0.36f;
      if (s.jump <= 0) {
        s.jump = 0;
        vy = 0;
      }
      unsigned frame = s.gameMs / 20 - 1;
      if (recording)
        recording[frame] = (uint8_t)s.jump;
      if (ghost && ghostHits != 65535)
        s.ghostJump = ghost[frame];
      int speed = 2 + int(s.gameMs / 15000);
      s.distance += speed;
      s.obstacle -= speed;
      if (!hit && s.obstacle < 60 && s.obstacle + 12 > 35 && s.jump < 13) {
        ++s.hits;
        hit = true;
      }
      if (s.obstacle < -16) {
        s.obstacle = 250 + random(rng) % 100;
        hit = false;
      }
      if (s.gameMs >= 45000) {
        s.gameDone = true;
        finishAt = now;
        s.showMs = 0;
        if (recording && ghost && s.hits < ghostHits) {
          for (unsigned i = 0; i < 2250; i++)
            ghost[i] = recording[i];
          ghostHits = s.hits;
        }
        uint32_t score = 45000 / (1 + s.hits);
        if (score > s.best) {
          s.best = score;
          wow.react(4, now);
        }
      }
    }
  }
  Result result() const {
    Result r;
    r.duration = s.elapsed;
    r.gates = s.gates;
    r.penalties = s.penalties;
    r.score = s.score;
    r.penaltyWeight = s.weight;
    return r;
  }
};
} // namespace Storm
