#pragma once
#include "storm_state.h"
namespace Storm {
// Keep numeric IDs compatible with the original serial and web commands.
inline const char *actionName(unsigned index) {
  static const char *const names[] = {
      "start",     "pause",      "resume", "finish", "gate",     "penalty",
      "undo",      "rules",      "show",   "exit",   "game",     "jump",
      "heartbeat", "background", "pit",    "board",  "midnight", "living",
      "liquid",    "garage",     "beat",   "steer",  "touch",    "impulse",
      "explode",   "orbit",      "audio"};
  static_assert(sizeof(names) / sizeof(names[0]) == unsigned(Action::Count),
                "Every command must have a stable wire name");
  return index < unsigned(Action::Count) ? names[index] : "";
}
inline bool validValue(Action action, uint32_t value) {
  if (action >= Action::Count)
    return false;
  switch (action) {
  case Action::Rules:
    return value <= 100;
  case Action::Steer:
    return value <= 2;
  case Action::Touch:
  case Action::Orbit:
    return value <= 65535;
  case Action::Explode:
  case Action::Heartbeat:
  case Action::Jump:
    return value <= 1;
  case Action::Game:
  case Action::Midnight:
  case Action::Audio:
    return true;
  default:
    return value == 0;
  }
}
inline bool parseValue(const char *text, size_t length, uint32_t &value) {
  if (!text || length > 10)
    return false;
  uint64_t number = 0;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9')
      return false;
    number = number * 10 + unsigned(text[i] - '0');
  }
  if (number > UINT32_MAX)
    return false;
  value = uint32_t(number);
  return true;
}
struct MidnightRecord {
  uint32_t magic = 0x4D494431, best = 0, check = ~uint32_t(0);
};
inline bool validRecord(const MidnightRecord &record) {
  return record.magic == 0x4D494431 && record.check == ~record.best;
}
} // namespace Storm
