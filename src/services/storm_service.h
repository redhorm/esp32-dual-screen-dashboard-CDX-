#pragma once
#include "core/storm_state.h"
#include <Arduino.h>
#include <WebServer.h>
namespace StormService {
void begin(WebServer &server);
void handle();
void update(uint32_t now);
bool command(Storm::Action action, uint32_t value = 0, bool test = false);
Storm::Snapshot snapshot();
} // namespace StormService
