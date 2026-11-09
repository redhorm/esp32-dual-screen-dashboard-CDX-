#include "storm_ota.h"
#include "storm_service.h"
#include "storm_update_page.h"
#include <Update.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <esp_system.h>
// Arduino otherwise confirms pending images inside initArduino(), before our
// memory/display/network checks. Keep the bootloader pending until health().
extern "C" bool verifyRollbackLater() { return true; }
namespace StormOta {
static char token[33];
static bool accepted = false, complete = false, failed = false;
static uint32_t rebootAt = 0, started = 0;
static std::atomic<bool> healthOk{false};
void health(bool healthy) { healthOk.store(healthy); }
void handle() {
  if (rebootAt && int32_t(millis() - rebootAt) >= 0)
    ESP.restart();
  static bool checked = false;
  if (!checked && millis() > 10000) {
    checked = true;
    esp_ota_img_states_t status;
    const auto p = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(p, &status) == ESP_OK &&
        status == ESP_OTA_IMG_PENDING_VERIFY) {
      if (healthOk.load() && ESP.getPsramSize() >= 7 * 1024 * 1024 &&
          ESP.getFlashChipSize() == 16 * 1024 * 1024) {
        esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
        Serial.printf("[stormDASH OTA] confirm=%d\n", e);
      } else {
        Serial.println("[stormDASH OTA] health failed: rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
    }
  }
  if (accepted && !complete && millis() - started > 120000) {
    Update.abort();
    accepted = false;
    failed = true;
  }
}
void begin(WebServer &s) {
  snprintf(token, sizeof token, "%08lx%08lx%08lx%08lx",
           (unsigned long)esp_random(), (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random());
  s.on("/update", HTTP_GET, [&s] {
    String page = FPSTR(STORM_UPDATE_PAGE);
    page.replace("{{TOKEN}}", token);
    s.sendHeader("Cache-Control", "no-store");
    s.send(200, "text/html", page);
  });
  s.on(
      "/api/storm/update", HTTP_POST,
      [&s] {
        if (complete && !failed) {
          s.send(200, "text/plain",
                 "stormDASH: immagine verificata. Riavvio tra 2 secondi; "
                 "riconnettiti al portale.");
          rebootAt = millis() + 2000;
        } else {
          s.send(400, "text/plain",
                 "Aggiornamento rifiutato o incompleto. Firmware corrente "
                 "conservato.");
        }
      },
      [&s] {
        auto &u = s.upload();
        if (accepted && millis() - started > 120000) {
          Update.abort();
          failed = true;
          accepted = false;
        }

        if (u.status == UPLOAD_FILE_START) {
          accepted = complete = failed = false;
          auto state = StormService::snapshot();
          auto next = esp_ota_get_next_update_partition(nullptr);
          bool eligible = s.arg("token") == token && !state.session &&
                          state.mode == Storm::Mode::Instruments && next &&
                          next != esp_ota_get_running_partition() &&
                          ESP.getFlashChipSize() == 16777216;
          if (eligible) {
            accepted = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
            started = millis();
          }
          failed = !accepted;
        } else if (u.status == UPLOAD_FILE_WRITE && accepted && !failed) {
          if (Update.write(u.buf, u.currentSize) != u.currentSize) {
            failed = true;
            Update.abort();
          }
        } else if (u.status == UPLOAD_FILE_END && accepted && !failed) {
          complete = Update.end(true);
          failed = !complete;
          accepted = false;
        } else if (u.status == UPLOAD_FILE_ABORTED) {
          Update.abort();
          failed = true;
          accepted = false;
        }
      });
}
} // namespace StormOta
