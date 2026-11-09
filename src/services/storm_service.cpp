#include <HTTPClient.h>
#include <atomic>
#include "storm_service.h"
#include "core/storm_archive.h"
#include "core/storm_protocol.h"
#include "storm_ota.h"
#include "storm_page.h"
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
namespace StormService {
static Storm::State state;
static Storm::Snapshot published;
struct Finished {
  Storm::Result result;
  bool test;
};
static QueueHandle_t commands = nullptr, results = nullptr;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
using Storm::Archive;
static Archive archive;
static uint32_t savedBest = 0;
static uint32_t savedMidnightBest = 0;
static bool midnightStorageOk = true;
static bool storageOk = false, archiveMigrationPending = false;
bool command(Storm::Action a, uint32_t v, bool test) {
  if (!Storm::validValue(a, v))
    return false;
  Storm::Command c;
  c.action = a;
  c.value = v;
  c.test = test;
  return commands && xQueueSend(commands, &c, 0) == pdTRUE;
}
Storm::Snapshot snapshot() {
  taskENTER_CRITICAL(&mux);
  auto s = published;
  taskEXIT_CRITICAL(&mux);
  return s;
}
void update(uint32_t now) {
  Storm::Command c;
  for (int i = 0;
       i < 16 && commands && results && uxQueueSpacesAvailable(results) > 0 &&
       xQueueReceive(commands, &c, 0) == pdTRUE;
       i++) {
    auto revision = state.s.finishedRevision;
    state.apply(c, now);
    if (state.s.finishedRevision != revision) {
      Finished f;
      f.result = state.s.closed;
      f.test = state.testSession;
      xQueueSend(results, &f, 0);
    }
  }
  state.tick(now);
  taskENTER_CRITICAL(&mux);
  published = state.s;
  taskEXIT_CRITICAL(&mux);
}
static bool save() {
  archive.checksum = Storm::archiveChecksum(archive);
  Preferences p;
  if (!p.begin("stormdash", false))
    return false;
  bool ok = p.putBytes("archive1", &archive, sizeof archive) == sizeof archive;
  Archive verified;
  ok = ok &&
       p.getBytes("archive1", &verified, sizeof verified) == sizeof verified &&
       memcmp(&verified, &archive, sizeof archive) == 0;
  p.end();
  return ok;
}
// USB-only, read-only HTTP diagnostic. Exercise the actual WebServer routes
// through lwIP loopback without changing the host's Wi-Fi network.
static std::atomic<bool> httpProbeBusy{false};
static char httpProbePath[64];
static void httpProbeTask(void*) {
  HTTPClient http;
  http.setConnectTimeout(2000); http.setTimeout(3000);
  http.begin(String("http://127.0.0.1")+httpProbePath);
  const int status=http.GET();
  String body=status>0?http.getString():String("transport_error");
  http.end();
  Serial.printf("@http %d %s %s\n",status,httpProbePath,body.c_str());
  httpProbeBusy.store(false);
  vTaskDelete(nullptr);
}
static void probeHttp(const char* path) {
  const char* allowed[]={"/api/storm","/api/settings","/api/v3/state",
    "/api/v4/state","/api/v4/diagnostics","/api/v3/rtl/state",
    "/api/esc/state","/api/lights/state","/api/photos","/api/copilot-display"};
  bool valid=false; for(const char* item:allowed)if(strcmp(item,path)==0)valid=true;
  if(!valid){Serial.println("@http_error unsupported_path");return;}
  if(httpProbeBusy.exchange(true)){Serial.println("@http_error busy");return;}
  strlcpy(httpProbePath,path,sizeof httpProbePath);
  if(xTaskCreate(httpProbeTask,"http-probe",8192,nullptr,1,nullptr)!=pdPASS){
    httpProbeBusy.store(false);Serial.println("@http_error task_failed");
  }
}
static void serialConsole() {
  static char line[64];
  static size_t length = 0;
  for (unsigned n = 0; n < 64 && Serial.available(); n++) {
    char c = Serial.read();
    if (c == '\r')
      continue;
    if (c != '\n') {
      if (length < sizeof(line) - 1)
        line[length++] = c;
      continue;
    }
    line[length] = 0;
    length = 0;
    if (strncmp(line, "@http ", 6) == 0) {
      probeHttp(line+6);
    } else if (strcmp(line, "@state") == 0) {
      auto s = snapshot();
      Serial.printf("@state "
                    "{\"mode\":%u,\"session\":%u,\"running\":%u,\"elapsed\":%"
                    "lu,\"gates\":%u,\"penalties\":%u,\"score\":%d,\"gameMs\":%"
                    "lu,\"hits\":%u,\"paused\":%u,\"done\":%u,\"best\":%lu,"
                    "\"revision\":%lu,\"saved\":%u,\"history\":%lu}\n",
                    (unsigned)s.mode, s.session, s.running,
                    (unsigned long)s.elapsed, s.gates, s.penalties, s.score,
                    (unsigned long)s.gameMs, s.hits, s.gamePaused, s.gameDone,
                    (unsigned long)s.best, (unsigned long)s.revision, storageOk,
                    (unsigned long)archive.count);
    } else if (strncmp(line, "@command ", 9) == 0) {
      unsigned action = 99, value = 0;
      if (sscanf(line + 9, "%u %u", &action, &value) >= 1 &&
          action < unsigned(Storm::Action::Count))
        Serial.printf("@accepted %u\n",
                      command(static_cast<Storm::Action>(action), value, true));
      else
        Serial.println("@error invalid_command");
    }
  }
}
void handle() {
  serialConsole();
  StormOta::handle();
  auto s = snapshot();
  static uint32_t recordRetry = 0;
  if (s.wow.best != savedMidnightBest &&
      (midnightStorageOk || int32_t(millis() - recordRetry) >= 0)) {
    Storm::MidnightRecord record, verified;
    record.best = s.wow.best;
    record.check = ~record.best;
    Preferences p;
    midnightStorageOk = false;
    if (p.begin("stormdash", false)) {
      midnightStorageOk =
          p.putBytes("midnight1", &record, sizeof record) == sizeof record &&
          p.getBytes("midnight1", &verified, sizeof verified) ==
              sizeof verified &&
          Storm::validRecord(verified) && verified.best == record.best;
      p.end();
    }
    if (midnightStorageOk)
      savedMidnightBest = record.best;
    recordRetry = millis() + 5000;
  }
  bool dirty = archiveMigrationPending;
  archiveMigrationPending = false;
  Finished closed;
  while (results && xQueueReceive(results, &closed, 0) == pdTRUE) {
    Storm::appendResult(archive, closed.result, closed.test);
    dirty = true;
  }
  if (s.best != savedBest) {
    archive.best = s.best;
    savedBest = s.best;
    dirty = true;
  }
  if (s.weight != archive.weight) {
    archive.weight = s.weight;
    dirty = true;
  }
  static bool pending = false;
  static uint32_t retry = 0;
  if (dirty)
    pending = true;
  if (pending && (dirty || int32_t(millis() - retry) >= 0)) {
    storageOk = save();
    pending = !storageOk;
    retry = millis() + 5000;
  }
}
void begin(WebServer &server) {
  StormOta::begin(server);
  commands = xQueueCreate(16, sizeof(Storm::Command));
  results = xQueueCreate(16, sizeof(Finished));
  auto ghosts =
      (uint8_t *)heap_caps_malloc(4500, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ghosts)
    state.attachGhost(ghosts, ghosts + 2250);
  Preferences p;
  if (p.begin("stormdash", true)) {
    if (p.isKey("midnight1")) {
      Storm::MidnightRecord record;
      midnightStorageOk =
          p.getBytesLength("midnight1") == sizeof record &&
          p.getBytes("midnight1", &record, sizeof record) == sizeof record &&
          Storm::validRecord(record);
      if (midnightStorageOk)
        state.wow.s.best = record.best;
    }
    struct LegacyV2 {
      uint32_t magic, version, count, best, weight;
      Storm::Result results[64];
      uint32_t checksum;
    };
    if (p.getBytesLength("archive1") == sizeof(LegacyV2)) {
      LegacyV2 old;
      if (p.getBytes("archive1", &old, sizeof old) == sizeof old) {
        uint32_t h = 2166136261;
        auto bytes = (const uint8_t *)&old;
        for (size_t i = 0; i < offsetof(LegacyV2, checksum); i++) {
          h ^= bytes[i];
          h *= 16777619;
        }
        if (old.magic == 0x53544431 && old.version == 2 && old.count <= 64 &&
            old.weight <= 100 && old.checksum == h) {
          archive.count = old.count;
          archive.best = old.best;
          archive.weight = old.weight;
          for (unsigned i = 0; i < old.count; i++)
            archive.results[i] = old.results[i];
          archive.testMask =
              old.count == 64 ? UINT64_MAX : ((uint64_t(1) << old.count) - 1);
          archive.checksum = Storm::archiveChecksum(archive);
          storageOk = true;
          archiveMigrationPending = true;
        }
      }
    }
    if (p.getBytesLength("archive1") == sizeof archive) {
      Archive loaded;
      if (p.getBytes("archive1", &loaded, sizeof loaded) == sizeof loaded &&
          Storm::validArchive(loaded)) {
        archive = loaded;
        storageOk = true;
      }
    }
    p.end();
  }
  state.s.weight = archive.weight;
  state.s.best = archive.best;
  savedBest = archive.best;
  savedMidnightBest = state.wow.s.best;
  state.s.wow = state.wow.s;
  published = state.s;
  server.on("/api/storm", HTTP_GET, [&server] {
    auto s = snapshot();
    char out[1024];
    snprintf(
        out, sizeof out,
        "{\"mode\":%u,\"running\":%s,\"session\":%s,\"elapsed\":%lu,"
        "\"gates\":%u,\"penalties\":%u,\"weight\":%u,\"score\":%d,"
        "\"gameMs\":%lu,\"hits\":%u,\"paused\":%s,\"done\":%s,\"best\":%"
        "lu,\"seed\":%lu,\"history\":%lu,\"saved\":%s,"
        "\"wow\":{\"ms\":%lu,\"elapsed\":%lu,\"score\":%lu,\"best\":%lu,"
        "\"hits\":%u,\"paused\":%s,\"done\":%s,\"audioActive\":%s,"
        "\"seed\":%lu,\"exploded\":%.3f,\"bands\":[%u,%u,%u,%u],\"saved\":%s}}",
        (unsigned)s.mode, s.running ? "true" : "false",
        s.session ? "true" : "false", (unsigned long)s.elapsed, s.gates,
        s.penalties, s.weight, s.score, (unsigned long)s.gameMs, s.hits,
        s.gamePaused ? "true" : "false", s.gameDone ? "true" : "false",
        (unsigned long)s.best, (unsigned long)s.seed,
        (unsigned long)archive.count, storageOk ? "true" : "false",
        (unsigned long)s.wow.ms, (unsigned long)s.wow.elapsed,
        (unsigned long)s.wow.score, (unsigned long)s.wow.best, s.wow.hits,
        s.wow.paused ? "true" : "false", s.wow.done ? "true" : "false",
        s.wow.audioActive ? "true" : "false", (unsigned long)s.wow.seed,
        double(s.wow.exploded), s.wow.bands[0], s.wow.bands[1], s.wow.bands[2],
        s.wow.bands[3], midnightStorageOk ? "true" : "false");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });
  server.on("/api/storm", HTTP_POST, [&server] {
    int index = -1;
    String a = server.arg("action");
    for (unsigned i = 0; i < unsigned(Storm::Action::Count); i++)
      if (a == Storm::actionName(i))
        index = i;
    String val = server.arg("value");
    uint32_t n = 0;
    if (index < 0 || !Storm::parseValue(val.c_str(), val.length(), n) ||
        !Storm::validValue(static_cast<Storm::Action>(index), n)) {
      server.send(400, "application/json", "{\"error\":\"invalid_command\"}");
      return;
    }
    bool ok = command(static_cast<Storm::Action>(index), n,
                      server.arg("test") == "1");
    server.send(ok ? 202 : 503, "application/json",
                ok ? "{\"accepted\":true}" : "{\"error\":\"queue_full\"}");
  });
  server.on("/api/storm/results", HTTP_GET, [&server] {
    String out = "[";
    for (uint32_t i = 0; i < archive.count; i++) {
      if (i)
        out += ",";
      auto r = archive.results[i];
      out += "{\"duration\":" + String(r.duration) +
             ",\"gates\":" + String(r.gates) +
             ",\"penalties\":" + String(r.penalties) +
             ",\"score\":" + String(r.score) +
             ",\"penaltyWeight\":" + String(r.penaltyWeight) + ",\"test\":" +
             ((archive.testMask & (uint64_t(1) << i)) ? "true" : "false") + "}";
    }
    out += "]";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });
  server.on("/api/storm/history", HTTP_GET, [&server] {
    String csv =
        "session,duration_ms,gates,penalties,penalty_weight,score,source\n";
    for (uint32_t i = 0; i < archive.count; i++) {
      const auto &r = archive.results[i];
      csv += String(i + 1) + "," + String(r.duration) + "," + String(r.gates) +
             "," + String(r.penalties) + "," + String(r.penaltyWeight) + "," +
             String(r.score) + "," +
             ((archive.testMask & (uint64_t(1) << i)) ? "COMMISSIONING"
                                                      : "MANUAL") +
             "\n";
    }
    server.sendHeader("Content-Disposition",
                      "attachment; filename=stormDASH-sessions.csv");
    server.send(200, "text/csv", csv);
  });
  server.on("/api/storm/hardware", HTTP_GET, [&server] {
    String out = "{\"flashPhysical\":" + String(ESP.getFlashChipSize()) +
                 ",\"flashConfigured\":16777216,\"psramTotal\":" +
                 String(ESP.getPsramSize()) +
                 ",\"psramFree\":" + String(ESP.getFreePsram()) +
                 ",\"heapFree\":" + String(ESP.getFreeHeap()) +
                 ",\"heapMinimum\":" + String(ESP.getMinFreeHeap()) +
                 ",\"partitions\":[";
    bool comma = false;
    auto it = esp_partition_find(ESP_PARTITION_TYPE_ANY,
                                 ESP_PARTITION_SUBTYPE_ANY, nullptr);
    while (it) {
      auto p = esp_partition_get(it);
      if (comma)
        out += ",";
      comma = true;
      out += "{\"label\":\"" + String(p->label) +
             "\",\"offset\":" + String(p->address) +
             ",\"size\":" + String(p->size) + "}";
      it = esp_partition_next(it);
    }
    out += "]}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", out);
  });
}
} // namespace StormService
