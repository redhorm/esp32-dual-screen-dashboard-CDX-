#pragma once
#include <WebServer.h>
namespace StormOta {
void begin(WebServer &server);
void handle();
void health(bool healthy);
} // namespace StormOta
