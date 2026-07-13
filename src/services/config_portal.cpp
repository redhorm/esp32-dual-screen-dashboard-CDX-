#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include "config.h"
#include "config_portal.h"
#include "settings_page.h"
#include "photo_store.h"

namespace {
const char* experienceModeName(ExperienceMode mode) {
    return mode == ExperienceMode::GAME ? "game" : "dashboard";
}
const char* gameStatusName(RockGameStatus status) {
    switch (status) {
        case RockGameStatus::COUNTDOWN: return "countdown";
        case RockGameStatus::ATTRACT:   return "attract";
        case RockGameStatus::PLAYING:   return "playing";
        case RockGameStatus::PAUSED:    return "paused";
        case RockGameStatus::CRASHED:   return "crashed";
        case RockGameStatus::FINISHED:  return "finished";
        default:                        return "idle";
    }
}
const char* gameEventName(RockGameEvent event) {
    switch (event) {
        case RockGameEvent::DODGED:    return "dodged";
        case RockGameEvent::NEAR_MISS: return "near_miss";
        case RockGameEvent::HIT:       return "hit";
        default:                       return "none";
    }
}
const char* obstacleKindName(RockObstacleKind kind) {
    switch (kind) {
        case RockObstacleKind::LOG: return "log";
        case RockObstacleKind::RUT: return "rut";
        default:                    return "boulder";
    }
}
void copyDisplaySafe(char* out, size_t len, const String& source) {
    if (!len) return;
    size_t n = 0;
    for (size_t i = 0; i < source.length() && n + 1 < len; i++) {
        char c = source[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        bool safe = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == ' ' || c == '.' || c == '_' || c == '-';
        if (safe) out[n++] = c;
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = '\0';
}
RtlFunction rtlFunctionFromArg(const String& value) {
    if (value == "brake") return RtlFunction::BRAKE;
    if (value == "left") return RtlFunction::LEFT;
    if (value == "right") return RtlFunction::RIGHT;
    if (value == "hazard") return RtlFunction::HAZARD;
    if (value == "reverse_work") return RtlFunction::REVERSE_WORK;
    if (value == "dust_chase") return RtlFunction::DUST_CHASE;
    if (value == "pit_test") return RtlFunction::PIT_TEST;
    return RtlFunction::RUNNING;
}
int uploadHttpStatus(PhotoUploadError error) {
    switch(error) {
        case PhotoUploadError::INVALID_SLOT:  return 400;
        case PhotoUploadError::INVALID_TRANSACTION: return 409;
        case PhotoUploadError::BUSY:          return 409;
        case PhotoUploadError::TOO_LARGE:     return 413;
        case PhotoUploadError::INVALID_IMAGE: return 415;
        case PhotoUploadError::FILESYSTEM:    return 503;
        case PhotoUploadError::NO_SPACE:      return 507;
        case PhotoUploadError::INTERRUPTED:   return 400;
        default:                              return 500;
    }
}
bool parseSlotArg(WebServer& server, uint8_t& slot) {
    if (!server.hasArg("slot")) return false;
    String value = server.arg("slot");
    if (!value.length() || value.length() > 2) return false;
    for (size_t i = 0; i < value.length(); i++)
        if (value[i] < '0' || value[i] > '9') return false;
    int parsed = value.toInt();
    if (parsed < 0 || parsed >= PHOTO_MAX_SLOTS) return false;
    slot = (uint8_t)parsed;
    return true;
}
bool validTransactionId(const String& value) {
    if (value.length() < 8 || value.length() > 16) return false;
    for (size_t i=0;i<value.length();i++) {
        char c=value[i];
        if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))
            return false;
    }
    return true;
}
}

ConfigPortal::ConfigPortal(ExperienceEngine& experience, RtlLogic& rtl,
                           Ws2812Rtl& rtl_hw, EscPwmInput& esc_input)
    : _server(80), _experience(experience), _rtl(rtl), _rtl_hw(rtl_hw),
      _esc_input(esc_input) {}

void ConfigPortal::_loadNetworkIdentity() {
    const uint64_t chip_id = ESP.getEfuseMac();
    snprintf(_ssid, sizeof(_ssid), "%s-%08lX", CONFIG_AP_SSID_PREFIX,
             static_cast<unsigned long>(chip_id & 0xFFFFFFFFUL));

    _prefs.begin("ultra4-ui", true);
    const String stored_password = _prefs.getString("apPass", "");
    _prefs.end();

    bool stored_valid = stored_password.length() >= 8 &&
                        stored_password.length() < sizeof(_password);
    for (size_t i = 0; stored_valid && i < stored_password.length(); ++i) {
        const char c = stored_password[i];
        stored_valid = (c >= 'A' && c <= 'Z') || (c >= '2' && c <= '9') ||
                       c == '-';
    }
    if (stored_valid) {
        stored_password.toCharArray(_password, sizeof(_password));
        return;
    }

    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    _password[0] = 'U';
    _password[1] = '4';
    _password[2] = '-';
    uint32_t random_state = esp_random() ^ static_cast<uint32_t>(chip_id) ^
                            static_cast<uint32_t>(chip_id >> 32) ^ micros();
    for (size_t i = 3; i < 13; ++i) {
        random_state ^= random_state << 13;
        random_state ^= random_state >> 17;
        random_state ^= random_state << 5;
        random_state ^= esp_random();
        _password[i] = alphabet[random_state % (sizeof(alphabet) - 1)];
    }
    _password[13] = '\0';

    _prefs.begin("ultra4-ui", false);
    const size_t written = _prefs.putString("apPass", _password);
    _prefs.end();
    if (written < strlen(_password))
        Serial.println("[portal] AP password persistence failed");
}

void ConfigPortal::_load() {
    _prefs.begin("ultra4-ui", true);
    _settings.driver_theme = sanitizeTheme(_prefs.getUChar("dashTheme", 0));
    _settings.navigator_theme = sanitizeTheme(_prefs.getUChar("navTheme", 0));
    _settings.language = sanitizeLanguage(_prefs.getUChar("uiLang", 0));
    _settings.use_kmh = _prefs.getBool("kmh", USE_KMH);
    _settings.use_celsius = _prefs.getBool("celsius", USE_CELSIUS);
    _settings.driver_brightness = sanitizeBrightness(_prefs.getUChar("dashBright", 220));
    _settings.navigator_brightness = sanitizeBrightness(_prefs.getUChar("navBright", 220));
    _settings.boot_image_enabled = _prefs.getBool("bootImg", true);
    _settings.boot_hold_ms = clampU16(_prefs.getUShort("bootMs", 1800), 500, 3500);
    _settings.photo_mode_enabled = _prefs.getBool("photoMode", false);
    _settings.photo_target = _prefs.getUChar("photoTarget", 3) & 0x03;
    if (_settings.photo_target == 0) _settings.photo_target = 3;
    _settings.photo_interval_s = clampU16(_prefs.getUShort("photoSec", 12), 3, 300);
    _settings.photo_transition = _prefs.getUChar("photoFx", 1) > 1 ? 1 : _prefs.getUChar("photoFx", 1);
    _settings.photo_random = _prefs.getBool("photoRnd", false);
    _settings.photo_overlay = _prefs.getBool("photoOvl", true);
    _setup_complete = _prefs.getBool("setupV4", false);
    RtlProfile rtl_profile = _prefs.getUChar("rtlProfile", 1) == 0
        ? RtlProfile::RTL_S : RtlProfile::ULTRA4_4400;
    int rtl_brightness = _prefs.getUChar("rtlBright", RTL_LED_BRIGHTNESS);
    if (rtl_brightness < 8) rtl_brightness = 8;
    if (rtl_brightness > 255) rtl_brightness = 255;
    _rtl_brightness = static_cast<uint8_t>(rtl_brightness);
    _esc_enabled = _prefs.getBool("escInput", ESC_PWM_INPUT_ENABLED != 0);
    _esc_config.reverse_us = ESC_PWM_REVERSE_US;
    _esc_config.forward_us = ESC_PWM_FORWARD_US;
    _esc_config.neutral_us = clampU16(
        _prefs.getUShort("escNeutral", ESC_PWM_NEUTRAL_US), 1300, 1700);
    _esc_deadband_us = clampU16(_prefs.getUShort("escDeadUs", 45), 10, 180);
    const uint8_t esc_type = _prefs.getUChar("escType", 0);
    _esc_config.type = esc_type <= static_cast<uint8_t>(EscType::FORWARD_BRAKE_ONLY)
        ? static_cast<EscType>(esc_type)
        : EscType::FORWARD_BRAKE_REVERSE;
    const uint16_t span = _esc_config.forward_us > _esc_config.neutral_us
        ? _esc_config.forward_us - _esc_config.neutral_us : 500;
    _esc_config.enter_threshold = static_cast<float>(_esc_deadband_us) / span;
    if (_esc_config.enter_threshold < 0.03f) _esc_config.enter_threshold = 0.03f;
    if (_esc_config.enter_threshold > 0.36f) _esc_config.enter_threshold = 0.36f;
    _esc_config.exit_threshold = _esc_config.enter_threshold * 0.55f;
    _esc_config.reverse_confirm_ms = clampU16(
        _prefs.getUShort("escRevMs", ESC_REVERSE_CONFIRM_MS), 100, 2000);
    _esc_input.configureSignal(_esc_config);
    _rtl.configure(rtl_profile, RTL_LED_COUNT, RTL_COMMAND_TIMEOUT_MS);
    RigProfile profile = _experience.config();
    String value = _prefs.getString("bVehicle", "");
    if (!value.length())
        value = _prefs.getString("bCall", _prefs.getString("bModel", profile.vehicle_name));
    value.toCharArray(profile.vehicle_name, sizeof(profile.vehicle_name));
    value = _prefs.getString("bDriver", _prefs.getString("bHandle", profile.driver));
    value.toCharArray(profile.driver, sizeof(profile.driver));
    value = _prefs.getString("bNumber", profile.number);
    value.toCharArray(profile.number, sizeof(profile.number));
    profile.accent = _prefs.getUShort("bAccent", profile.accent);
    profile.best_game_score = _prefs.getULong("bGameBest", profile.best_game_score);
    profile.game_wins = _prefs.getUShort("bGameWins", profile.game_wins);
    _experience.setConfig(profile);
    _prefs.end();
}

void ConfigPortal::_save() {
    _prefs.begin("ultra4-ui", false);
    _prefs.putUChar("dashTheme", static_cast<uint8_t>(_settings.driver_theme));
    _prefs.putUChar("navTheme", static_cast<uint8_t>(_settings.navigator_theme));
    _prefs.putUChar("uiLang", static_cast<uint8_t>(_settings.language));
    _prefs.putBool("kmh", _settings.use_kmh);
    _prefs.putBool("celsius", _settings.use_celsius);
    _prefs.putUChar("dashBright", _settings.driver_brightness);
    _prefs.putUChar("navBright", _settings.navigator_brightness);
    _prefs.putBool("bootImg", _settings.boot_image_enabled);
    _prefs.putUShort("bootMs", _settings.boot_hold_ms);
    _prefs.putBool("photoMode", _settings.photo_mode_enabled);
    _prefs.putUChar("photoTarget", _settings.photo_target);
    _prefs.putUShort("photoSec", _settings.photo_interval_s);
    _prefs.putUChar("photoFx", _settings.photo_transition);
    _prefs.putBool("photoRnd", _settings.photo_random);
    _prefs.putBool("photoOvl", _settings.photo_overlay);
    _prefs.putBool("setupV4", _setup_complete);
    _prefs.putUChar("rtlProfile", _rtl.profile() == RtlProfile::ULTRA4_4400 ? 1 : 0);
    _prefs.putUChar("rtlBright", _rtl_brightness);
    _prefs.putBool("escInput", _esc_enabled);
    _prefs.putUChar("escType", static_cast<uint8_t>(_esc_config.type));
    _prefs.putUShort("escNeutral", _esc_config.neutral_us);
    _prefs.putUShort("escDeadUs", _esc_deadband_us);
    _prefs.putUShort("escRevMs", _esc_config.reverse_confirm_ms);
    const RigProfile& profile = _experience.config();
    _prefs.putString("bVehicle", profile.vehicle_name);
    _prefs.putString("bDriver", profile.driver);
    _prefs.putString("bNumber", profile.number);
    _prefs.putUShort("bAccent", profile.accent);
    _prefs.putULong("bGameBest", profile.best_game_score);
    _prefs.putUShort("bGameWins", profile.game_wins);
    _prefs.end();
}

void ConfigPortal::_sendPortal() {
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.sendHeader("Expires", "0");
    _server.send_P(200, "text/html; charset=utf-8", SETTINGS_PAGE);
}

void ConfigPortal::_sendSettings() {
    char json[560];
    snprintf(json, sizeof(json),
        "{\"displayTheme\":%u,\"driverTheme\":%u,\"navigatorTheme\":%u,\"language\":%u,"
        "\"setupComplete\":%s,\"useKmh\":%s,"
        "\"useCelsius\":%s,\"driverBrightness\":%u,\"navigatorBrightness\":%u,"
        "\"bootImageEnabled\":%s,\"bootHoldMs\":%u,\"photoModeEnabled\":%s,"
        "\"photoTarget\":%u,\"photoIntervalS\":%u,\"photoTransition\":%u,"
        "\"photoRandom\":%s,\"photoOverlay\":%s}",
        static_cast<unsigned>(_settings.driver_theme),
        static_cast<unsigned>(_settings.driver_theme),
        static_cast<unsigned>(_settings.navigator_theme),
        static_cast<unsigned>(_settings.language),
        _setup_complete ? "true" : "false",
        _settings.use_kmh ? "true" : "false",
        _settings.use_celsius ? "true" : "false",
        _settings.driver_brightness,
        _settings.navigator_brightness,
        _settings.boot_image_enabled ? "true" : "false", _settings.boot_hold_ms,
        _settings.photo_mode_enabled ? "true" : "false", _settings.photo_target,
        _settings.photo_interval_s, _settings.photo_transition,
        _settings.photo_random ? "true" : "false",
        _settings.photo_overlay ? "true" : "false");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_saveSettings() {
    if ((!_server.hasArg("displayTheme") &&
         (!_server.hasArg("driverTheme") || !_server.hasArg("navigatorTheme"))) ||
        !_server.hasArg("units") || !_server.hasArg("temperature") ||
        !_server.hasArg("driverBrightness") || !_server.hasArg("navigatorBrightness")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing fields\"}");
        return;
    }

    if (_server.hasArg("displayTheme")) {
        UiTheme paired = sanitizeTheme(_server.arg("displayTheme").toInt());
        _settings.driver_theme = paired;
        _settings.navigator_theme = paired;
    } else {
        _settings.driver_theme = sanitizeTheme(_server.arg("driverTheme").toInt());
        _settings.navigator_theme = sanitizeTheme(_server.arg("navigatorTheme").toInt());
    }
    if (_server.hasArg("language"))
        _settings.language = sanitizeLanguage(_server.arg("language").toInt());
    _settings.use_kmh = _server.arg("units") == "metric";
    _settings.use_celsius = _server.arg("temperature") == "c";
    _settings.driver_brightness = sanitizeBrightness(_server.arg("driverBrightness").toInt());
    _settings.navigator_brightness = sanitizeBrightness(_server.arg("navigatorBrightness").toInt());
    _settings.boot_image_enabled = _server.arg("bootImageEnabled") == "on";
    _settings.boot_hold_ms = clampU16(_server.arg("bootHoldMs").toInt(), 500, 3500);
    _settings.photo_mode_enabled = _server.arg("photoModeEnabled") == "on";
    _settings.photo_target = _server.arg("photoTarget").toInt() & 0x03;
    if (_settings.photo_target == 0) _settings.photo_target = 3;
    _settings.photo_interval_s = clampU16(_server.arg("photoIntervalS").toInt(), 3, 300);
    _settings.photo_transition = _server.arg("photoTransition").toInt() == 0 ? 0 : 1;
    _settings.photo_random = _server.arg("photoRandom") == "on";
    _settings.photo_overlay = _server.arg("photoOverlay") == "on";
    _save();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_sendPhotos() {
    char json[900];
    uint32_t mask = photoStore.slotMask();
    char slots[100]; size_t pos = 0;
    pos += snprintf(slots + pos, sizeof(slots) - pos, "[");
    bool first = true;
    for (uint8_t i = 0; i < PHOTO_MAX_SLOTS; i++) if (mask & (1UL << i)) {
        pos += snprintf(slots + pos, sizeof(slots) - pos, "%s%u", first ? "" : ",", i);
        first = false;
    }
    snprintf(slots + pos, sizeof(slots) - pos, "]");
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"schemaVersion\":3,\"slots\":%s,\"maxSlots\":%u,"
        "\"bootDriver\":%s,\"bootNavigator\":%s,"
        "\"usedBytes\":%lu,\"totalBytes\":%lu,\"freeBytes\":%lu,\"fsReady\":%s,"
        "\"storageState\":\"%s\",\"partitionLabel\":\"%s\","
        "\"formattedOnBegin\":%s,\"recoveredOnBegin\":%s,\"degraded\":%s,"
        "\"uploadActive\":%s,\"lastError\":\"%s\","
        "\"driverMaxBytes\":%lu,\"navigatorMaxBytes\":%lu,\"reserveBytes\":%lu}",
        slots, PHOTO_MAX_SLOTS,
        photoStore.hasBoot(false) ? "true" : "false",
        photoStore.hasBoot(true) ? "true" : "false",
        (unsigned long)photoStore.usedBytes(), (unsigned long)photoStore.totalBytes(),
        (unsigned long)photoStore.freeBytes(), photoStore.ready() ? "true" : "false",
        photoStore.storageStateName(), photoStore.partitionLabel(),
        photoStore.formattedOnBegin() ? "true" : "false",
        photoStore.recoveredOnBegin() ? "true" : "false",
        photoStore.degraded() ? "true" : "false",
        photoStore.uploadActive() ? "true" : "false",
        PhotoStore::uploadErrorName(photoStore.uploadError()),
        (unsigned long)PHOTO_DRIVER_MAX_BYTES, (unsigned long)PHOTO_NAV_MAX_BYTES,
        (unsigned long)PHOTO_STORAGE_RESERVE_BYTES);
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendMediaAsset(bool boot) {
    if (!photoStore.ready()) {
        _server.send(503, "application/json",
                     "{\"ok\":false,\"error\":\"filesystem_unavailable\"}");
        return;
    }

    const bool navigator = _server.arg("display") == "b";
    char path[24];
    if (boot) {
        snprintf(path, sizeof(path), "%s", PhotoStore::bootPath(navigator));
    } else {
        uint8_t slot = 0;
        if (!parseSlotArg(_server, slot)) {
            _server.send(400, "application/json",
                         "{\"ok\":false,\"error\":\"invalid_slot\"}");
            return;
        }
        PhotoStore::photoPath(path, sizeof(path), slot, navigator);
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        _server.send(404, "application/json",
                     "{\"ok\":false,\"error\":\"not_found\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.sendHeader("X-Content-Type-Options", "nosniff");
    _server.streamFile(file, "image/jpeg");
    file.close();
}

void ConfigPortal::_deletePhoto() {
    uint8_t slot = 0;
    if (!parseSlotArg(_server, slot)) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_slot\"}");
        return;
    }
    if(_pair_active && !_pair_boot && _pair_slot==slot && !_abortPairUpload()) {
        _server.send(500,"application/json","{\"ok\":false,\"error\":\"abort_failed\"}");
        return;
    }
    bool ok = photoStore.removeSlot(slot);
    _server.send(ok ? 200 : 404, "application/json",
                 ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"not_found\"}");
}

void ConfigPortal::_deleteBoot() {
    if(_pair_active && _pair_boot && !_abortPairUpload()) {
        _server.send(500,"application/json","{\"ok\":false,\"error\":\"abort_failed\"}");
        return;
    }
    bool ok=photoStore.removeBoot();
    _server.send(ok?200:500,"application/json",
                 ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"delete_failed\"}");
}

void ConfigPortal::_resetPairUpload() {
    _pair_active=false;
    _pair_boot=false;
    _pair_a_ready=false;
    _pair_b_ready=false;
    _pair_slot=0;
    _pair_started_ms=0;
    _pair_tx[0]='\0';
}

bool ConfigPortal::_abortPairUpload() {
    if(!_pair_active)return true;
    bool ok=_pair_boot?photoStore.abortBootUpload():photoStore.abortSlotUpload(_pair_slot);
    if(ok)_resetPairUpload();
    return ok;
}

bool ConfigPortal::_pairRequestMatches(bool boot, uint8_t slot) {
    String tx=_server.arg("tx");
    return _pair_active && validTransactionId(tx) && _pair_boot==boot &&
           (boot || _pair_slot==slot) && tx.equals(_pair_tx);
}

bool ConfigPortal::_preparePairUpload(bool boot, bool navigator, uint8_t slot) {
    String tx=_server.arg("tx");
    if(!validTransactionId(tx)) {
        _upload_error=PhotoUploadError::INVALID_TRANSACTION;
        return false;
    }
    uint32_t now=millis();
    if(_pair_active && now-_pair_started_ms>30000UL && !_abortPairUpload()) {
        _upload_error=PhotoUploadError::FILESYSTEM;
        return false;
    }
    if(!_pair_active) {
        // A always opens a transaction; B can only complete that same lease.
        if(navigator) {
            _upload_error=PhotoUploadError::INVALID_TRANSACTION;
            return false;
        }
        if(!photoStore.ready()) {
            _upload_error=PhotoUploadError::FILESYSTEM;
            return false;
        }
        bool clean=boot?photoStore.abortBootUpload():photoStore.abortSlotUpload(slot);
        if(!clean){_upload_error=PhotoUploadError::FILESYSTEM;return false;}
        _pair_active=true;
        _pair_boot=boot;
        _pair_slot=slot;
        tx.toCharArray(_pair_tx,sizeof(_pair_tx));
        _pair_a_ready=false;
        _pair_b_ready=false;
    } else if(!_pairRequestMatches(boot,slot)) {
        _upload_error=PhotoUploadError::INVALID_TRANSACTION;
        return false;
    } else if(navigator && !_pair_a_ready) {
        _upload_error=PhotoUploadError::INVALID_TRANSACTION;
        return false;
    } else if(navigator) {
        // A B retry must never inherit the ready flag from an older stage.
        _pair_b_ready=false;
    } else if(!navigator) {
        // An explicit A retry restarts the pair and cannot reuse an old B.
        bool clean=boot?photoStore.abortBootUpload():photoStore.abortSlotUpload(slot);
        if(!clean){_upload_error=PhotoUploadError::FILESYSTEM;return false;}
        _pair_a_ready=false;
        _pair_b_ready=false;
    }
    _pair_started_ms=now;
    return true;
}

void ConfigPortal::_commitPhotoUpload() {
    uint8_t slot=0;
    if(!parseSlotArg(_server,slot)) {
        _server.send(400,"application/json","{\"ok\":false,\"error\":\"invalid_slot\"}");
        return;
    }
    if(!_pairRequestMatches(false,slot)||!_pair_a_ready||!_pair_b_ready) {
        _server.send(409,"application/json","{\"ok\":false,\"error\":\"invalid_transaction\"}");
        return;
    }
    bool ok=photoStore.commitSlotUpload(slot);
    if(ok){_resetPairUpload();_server.send(200,"application/json","{\"ok\":true}");return;}
    PhotoUploadError error=photoStore.uploadError();
    char json[100];snprintf(json,sizeof(json),"{\"ok\":false,\"error\":\"%s\"}",PhotoStore::uploadErrorName(error));
    _server.send(uploadHttpStatus(error),"application/json",json);
}

void ConfigPortal::_abortPhotoUpload() {
    uint8_t slot=0;
    if(!parseSlotArg(_server,slot)) {
        _server.send(400,"application/json","{\"ok\":false,\"error\":\"invalid_slot\"}");
        return;
    }
    if(!_pair_active){_server.send(200,"application/json","{\"ok\":true}");return;}
    if(!_pairRequestMatches(false,slot)) {
        _server.send(409,"application/json","{\"ok\":false,\"error\":\"invalid_transaction\"}");
        return;
    }
    bool ok=_abortPairUpload();
    _server.send(ok?200:500,"application/json",ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"abort_failed\"}");
}

void ConfigPortal::_commitBootUpload() {
    if(!_pairRequestMatches(true,0)||!_pair_a_ready||!_pair_b_ready) {
        _server.send(409,"application/json","{\"ok\":false,\"error\":\"invalid_transaction\"}");
        return;
    }
    bool ok=photoStore.commitBootUpload();
    if(ok){_resetPairUpload();_server.send(200,"application/json","{\"ok\":true}");return;}
    PhotoUploadError error=photoStore.uploadError();
    char json[100];snprintf(json,sizeof(json),"{\"ok\":false,\"error\":\"%s\"}",PhotoStore::uploadErrorName(error));
    _server.send(uploadHttpStatus(error),"application/json",json);
}

void ConfigPortal::_abortBootUpload() {
    if(!_pair_active){_server.send(200,"application/json","{\"ok\":true}");return;}
    if(!_pairRequestMatches(true,0)) {
        _server.send(409,"application/json","{\"ok\":false,\"error\":\"invalid_transaction\"}");
        return;
    }
    bool ok=_abortPairUpload();
    _server.send(ok?200:500,"application/json",ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"abort_failed\"}");
}

void ConfigPortal::_handleUpload(bool boot, bool navigator) {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        _upload_seen=true;_upload_started=false;_upload_ok=false;
        _upload_navigator=navigator;_upload_error=PhotoUploadError::NONE;
        char path[24];
        uint8_t slot=0;
        if (!boot) {
            if (!parseSlotArg(_server, slot)) {
                _upload_error=PhotoUploadError::INVALID_SLOT; return;
            }
        }
        if(!_preparePairUpload(boot,navigator,slot)) return;
        if (boot) snprintf(path, sizeof(path), "%s", PhotoStore::bootStagePath(navigator));
        else {
            PhotoStore::photoStagePath(path, sizeof(path), slot, navigator);
        }
        _upload_started=photoStore.beginUpload(path);
        _upload_ok=_upload_started;
        if(!_upload_ok)_upload_error=photoStore.uploadError();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if(_upload_ok) {
            _upload_ok=photoStore.writeUpload(upload.buf,upload.currentSize);
            if(!_upload_ok)_upload_error=photoStore.uploadError();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        bool committed=_upload_started && photoStore.endUpload(_upload_ok);
        if (_upload_ok) {
            _upload_ok=committed;
            if(!_upload_ok)_upload_error=photoStore.uploadError();
        } else if (_upload_error == PhotoUploadError::NONE) {
            _upload_error=photoStore.uploadError();
        }
        if(_upload_ok) {
            if(_upload_navigator)_pair_b_ready=true;
            else _pair_a_ready=true;
            _pair_started_ms=millis();
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if(_upload_started)photoStore.endUpload(false);_upload_ok=false;
        _upload_error=PhotoUploadError::INTERRUPTED;
    }
}

void ConfigPortal::_finishUpload() {
    if(!_upload_seen && _upload_error==PhotoUploadError::NONE)
        _upload_error=PhotoUploadError::INVALID_IMAGE;
    if(_upload_ok)_server.send(200,"application/json","{\"ok\":true}");
    else {
        if(_upload_error==PhotoUploadError::NONE)_upload_error=PhotoUploadError::INTERRUPTED;
        const char* name=PhotoStore::uploadErrorName(_upload_error);
        char json[100];
        snprintf(json,sizeof(json),"{\"ok\":false,\"error\":\"%s\"}",name);
        _server.send(uploadHttpStatus(_upload_error),"application/json",json);
    }
    _upload_seen=false;_upload_started=false;_upload_ok=false;
    _upload_navigator=false;_upload_error=PhotoUploadError::NONE;
}

void ConfigPortal::_sendCapabilities() {
    char json[420];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"protocol\":3,\"version\":\"%s\",\"features\":["
        "\"offroadArcade\",\"rtlController\",\"escStateMachine\","
        "\"realismDash\",\"stockOffroadDash\",\"bilingualUi\","
        "\"photoStorageV3\",\"mediaPreview\","
        "\"uniqueApCredentials\",\"rtlOutputWatchdog\"],"
        "\"maxClients\":%u,\"stateHz\":5,\"inputHz\":10}",
        EXPERIENCE_VERSION, CONFIG_AP_MAX_CLIENTS);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendExperienceState() {
    const uint32_t now = millis();
    const ExperienceFrame f = _experience.frame(now);
    String json;
    json.reserve(1900);
    json += F("{\"ok\":true,\"version\":\"");
    json += EXPERIENCE_VERSION;
    json += F("\",\"nowMs\":"); json += now;
    json += F(",\"mode\":\""); json += experienceModeName(f.mode);
    json += F("\",\"identity\":{\"number\":\""); json += f.identity.number;
    json += F("\",\"vehicle\":\""); json += f.identity.vehicle_name;
    json += F("\",\"vehicleName\":\""); json += f.identity.vehicle_name;
    json += F("\",\"driver\":\""); json += f.identity.driver;
    json += F("\",\"accent\":"); json += f.identity.accent;
    json += F("},\"vehicle\":{\"speedMph\":"); json += String(f.speed_mph, 1);
    json += F(",\"rpm\":"); json += String(f.rpm, 0);
    json += F(",\"gear\":"); json += f.gear;
    json += F(",\"heading\":"); json += String(f.heading_deg, 0);
    json += F(",\"batteryV\":"); json += String(f.battery_v, 1);
    json += F(",\"activeWp\":"); json += f.active_wp;
    json += F("},\"game\":{\"status\":\""); json += gameStatusName(f.game.status);
    json += F("\",\"event\":\""); json += gameEventName(f.game.last_event);
    json += F("\",\"x\":"); json += f.game.line_x;
    json += F(",\"lineX\":"); json += f.game.line_x;
    json += F(",\"speed\":"); json += f.game.speed;
    json += F(",\"nearestDistance\":"); json += f.game.nearest_distance;
    json += F(",\"progress\":"); json += f.game.progress_permille;
    json += F(",\"score\":"); json += f.game.score;
    json += F(",\"combo\":"); json += f.game.combo;
    json += F(",\"lives\":"); json += f.game.lives;
    json += F(",\"obstacle\":"); json += f.game.obstacle;
    json += F(",\"totalObstacles\":"); json += f.game.total_obstacles;
    json += F(",\"phaseMs\":"); json += f.game.phase_ms;
    json += F(",\"steer\":"); json += f.game.steering;
    json += F(",\"throttle\":"); json += f.game.throttle;
    json += F(",\"worldDistance\":"); json += f.game.world_distance;
    json += F(",\"resultAgeMs\":"); json += f.game.result_age_ms;
    json += F(",\"elapsedMs\":"); json += f.game.elapsed_ms;
    json += F(",\"visibleObstacles\":"); json += f.game.visible_obstacles;
    json += F(",\"courseClear\":"); json += f.game.course_clear ? F("true") : F("false");
    json += F(",\"bestScore\":"); json += f.identity.best_game_score;
    json += F(",\"wins\":"); json += f.identity.game_wins;
    json += F(",\"obstacles\":[");
    for (uint8_t i = 0; i < f.game.visible_obstacles; ++i) {
        if (i) json += ',';
        const RockObstacleView& obstacle = f.game.obstacles[i];
        json += F("{\"lineX\":"); json += obstacle.line_x;
        json += F(",\"distance\":"); json += obstacle.distance;
        json += F(",\"radius\":"); json += obstacle.radius;
        json += F(",\"kind\":"); json += static_cast<uint8_t>(obstacle.kind);
        json += F(",\"kindName\":\""); json += obstacleKindName(obstacle.kind);
        json += F("\"}");
    }
    json += F("]}}");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendProfile() { _sendExperienceState(); }

void ConfigPortal::_saveProfile() {
    RigProfile config = _experience.config();
    const String vehicle = _server.hasArg("vehicle")
        ? _server.arg("vehicle") : _server.arg("vehicleName");
    if (!_server.hasArg("number") || !vehicle.length() || !_server.hasArg("driver")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing fields\"}");
        return;
    }
    copyDisplaySafe(config.number, sizeof(config.number), _server.arg("number"));
    copyDisplaySafe(config.vehicle_name, sizeof(config.vehicle_name), vehicle);
    copyDisplaySafe(config.driver, sizeof(config.driver), _server.arg("driver"));
    if (!config.number[0] || !config.vehicle_name[0] || !config.driver[0]) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid profile\"}");
        return;
    }
    _experience.setConfig(config);
    _setup_complete = true;
    _save();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_startGame() {
    _experience.startGame(millis());
    _sendExperienceState();
}

void ConfigPortal::_inputGame() {
    int steer = _server.arg("steer").toInt();
    bool throttle = _server.arg("throttle").toInt() > 0;
    _experience.controlGame(steer, throttle, millis());
    _server.send(204, "text/plain", "");
}

void ConfigPortal::_gameAction() {
    const String action = _server.arg("action");
    const uint32_t now = millis();
    if (action == "enter") _experience.enterGameAttract(now);
    else if (action == "start" || action == "restart") _experience.startGame(now);
    else if (action == "pause") _experience.pauseGame(now);
    else if (action == "resume") _experience.resumeGame(now);
    else if (action == "quit") _experience.stopGame();
    else {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_action\"}");
        return;
    }
    _sendExperienceState();
}

void ConfigPortal::_sendRtlState() {
    const uint32_t now = millis();
    const RtlFrame frame = _rtl.frame(now);
    String json;
    json.reserve(1100);
    json += F("{\"ok\":true,\"profile\":\"");
    json += RtlLogic::profileName(frame.profile);
    json += F("\",\"function\":\"");
    json += RtlLogic::functionName(frame.primary);
    json += F("\",\"brightness\":");
    json += static_cast<unsigned>(_rtl_brightness);
    json += F(",\"hardwareReady\":");
    json += _rtl_hw.ready() ? F("true") : F("false");
    json += F(",\"timedOut\":");
    json += frame.command_timed_out ? F("true") : F("false");
    json += F(",\"escEnabled\":");
    json += _esc_enabled ? F("true") : F("false");
    json += F(",\"escValid\":");
    json += _esc_sample.valid ? F("true") : F("false");
    json += F(",\"escPulse\":");
    json += _esc_sample.filtered_us;
    json += F(",\"escType\":\"");
    json += EscSignalProcessor::typeName(_esc_config.type);
    json += F("\",\"escState\":\"");
    json += EscSignalProcessor::stateName(_esc_sample.state);
    json += F("\",\"escNormalized\":");
    json += static_cast<int>(_esc_sample.normalized * 100.0f);
    json += F(",\"escNeutralUs\":"); json += _esc_config.neutral_us;
    json += F(",\"escDeadbandUs\":"); json += _esc_deadband_us;
    json += F(",\"escReverseConfirmMs\":"); json += _esc_config.reverse_confirm_ms;
    json += F(",\"pixels\":[");
    for (uint16_t i = 0; i < frame.count; ++i) {
        if (i) json += ',';
        json += '['; json += static_cast<unsigned>(frame.pixels[i].r); json += ',';
        json += static_cast<unsigned>(frame.pixels[i].g); json += ',';
        json += static_cast<unsigned>(frame.pixels[i].b);
        json += ']';
    }
    json += F("]}");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_saveRtlConfig() {
    const RtlProfile profile = _server.arg("profile") == "rtl_s"
        ? RtlProfile::RTL_S : RtlProfile::ULTRA4_4400;
    int brightness = _server.arg("brightness").toInt();
    if (brightness < 8) brightness = 8;
    if (brightness > 255) brightness = 255;
    _rtl_brightness = static_cast<uint8_t>(brightness);
    _esc_enabled = _server.arg("escEnabled").toInt() > 0;
    _esc_config.type = EscSignalProcessor::typeFromName(_server.arg("escType").c_str());
    _esc_config.neutral_us = clampU16(_server.arg("escNeutralUs").toInt(), 1300, 1700);
    _esc_deadband_us = clampU16(_server.arg("escDeadbandUs").toInt(), 10, 180);
    _esc_config.reverse_confirm_ms = clampU16(
        _server.arg("escReverseConfirmMs").toInt(), 100, 2000);
    const uint16_t span = _esc_config.forward_us > _esc_config.neutral_us
        ? _esc_config.forward_us - _esc_config.neutral_us : 500;
    _esc_config.enter_threshold = static_cast<float>(_esc_deadband_us) / span;
    if (_esc_config.enter_threshold < 0.03f) _esc_config.enter_threshold = 0.03f;
    if (_esc_config.enter_threshold > 0.36f) _esc_config.enter_threshold = 0.36f;
    _esc_config.exit_threshold = _esc_config.enter_threshold * 0.55f;
    _rtl.configure(profile, RTL_LED_COUNT, RTL_COMMAND_TIMEOUT_MS);
    _rtl_hw.setBrightness(_rtl_brightness);
    _esc_input.configureSignal(_esc_config);
    _esc_input.begin(_esc_enabled);
    _manual_rtl_active = false;
    _save();
    _sendRtlState();
}

void ConfigPortal::_commandRtl() {
    const String value = _server.arg("function");
    if (value == "pit_test" && _experience.frame(millis()).speed_mph > 1.0f) {
        _server.send(409, "application/json",
                     "{\"ok\":false,\"error\":\"vehicle_not_stationary\"}");
        return;
    }
    const bool clear = value == "clear" || value == "running";
    const bool valid = clear || value == "brake" || value == "left" ||
        value == "right" || value == "hazard" || value == "reverse_work" ||
        value == "dust_chase" || value == "pit_test";
    if (!valid) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_function\"}");
        return;
    }
    const uint32_t now = millis();
    _manual_rtl_active = !clear;
    _manual_rtl_at_ms = now;
    _manual_rtl = RtlLogic::stateFor(clear ? RtlFunction::RUNNING
                                           : rtlFunctionFromArg(value));
    _rtl.submit(_manual_rtl, now);
    _sendRtlState();
}

bool ConfigPortal::begin() {
    bool fs_ok = photoStore.begin();
    _load();
    _rtl_hw.setBrightness(_rtl_brightness);
    _rtl_hw.begin();
    _esc_input.begin(_esc_enabled);
    WiFi.mode(WIFI_AP);
    _loadNetworkIdentity();
    bool ok = WiFi.softAP(_ssid, _password,
                          CONFIG_AP_CHANNEL, false, CONFIG_AP_MAX_CLIENTS);
    IPAddress ip = WiFi.softAPIP();
    snprintf(_ip, sizeof(_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    _dns.start(53, "*", ip);
    _server.on("/", HTTP_GET, [this] { _sendPortal(); });
    _server.on("/api/settings", HTTP_GET, [this] { _sendSettings(); });
    _server.on("/api/settings", HTTP_POST, [this] { _saveSettings(); });
    _server.on("/api/v2/capabilities", HTTP_GET, [this] { _sendCapabilities(); });
    _server.on("/api/v3/capabilities", HTTP_GET, [this] { _sendCapabilities(); });
    _server.on("/api/v2/state", HTTP_GET, [this] { _sendExperienceState(); });
    _server.on("/api/v2/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v2/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v3/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v3/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v3/state", HTTP_GET, [this] { _sendExperienceState(); });
    _server.on("/api/v2/game/start", HTTP_POST, [this] { _startGame(); });
    _server.on("/api/v2/game/input", HTTP_POST, [this] { _inputGame(); });
    _server.on("/api/v2/game/action", HTTP_POST, [this] { _gameAction(); });
    _server.on("/api/v3/rtl/state", HTTP_GET, [this] { _sendRtlState(); });
    _server.on("/api/v3/rtl/config", HTTP_POST, [this] { _saveRtlConfig(); });
    _server.on("/api/v3/rtl/command", HTTP_POST, [this] { _commandRtl(); });
    _server.on("/api/photos", HTTP_GET, [this] { _sendPhotos(); });
    _server.on("/api/media/boot", HTTP_GET, [this] { _sendMediaAsset(true); });
    _server.on("/api/media/photo", HTTP_GET, [this] { _sendMediaAsset(false); });
    _server.on("/api/photos/delete", HTTP_POST, [this] { _deletePhoto(); });
    _server.on("/api/photos/commit", HTTP_POST, [this] { _commitPhotoUpload(); });
    _server.on("/api/photos/abort", HTTP_POST, [this] { _abortPhotoUpload(); });
    _server.on("/api/boot/delete", HTTP_POST, [this] { _deleteBoot(); });
    _server.on("/api/boot/commit", HTTP_POST, [this] { _commitBootUpload(); });
    _server.on("/api/boot/abort", HTTP_POST, [this] { _abortBootUpload(); });
    _server.on("/api/upload/photo-a", HTTP_POST, [this] { _finishUpload(); },
               [this] { _handleUpload(false, false); });
    _server.on("/api/upload/photo-b", HTTP_POST, [this] { _finishUpload(); },
               [this] { _handleUpload(false, true); });
    _server.on("/api/upload/boot-a", HTTP_POST, [this] { _finishUpload(); },
               [this] { _handleUpload(true, false); });
    _server.on("/api/upload/boot-b", HTTP_POST, [this] { _finishUpload(); },
               [this] { _handleUpload(true, true); });
    // Android, iOS and Windows captive-portal probes.
    _server.on("/generate_204", HTTP_ANY, [this] { _sendPortal(); });
    _server.on("/hotspot-detect.html", HTTP_ANY, [this] { _sendPortal(); });
    _server.on("/connecttest.txt", HTTP_ANY, [this] { _sendPortal(); });
    _server.onNotFound([this] {
        if (_server.uri().startsWith("/api/"))
            _server.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
        else
            _sendPortal();
    });
    _server.begin();
    if (!fs_ok) Serial.println("[photos] LittleFS unavailable; AP settings remain online");
    return ok;
}

void ConfigPortal::handle() {
    _dns.processNextRequest();
    _server.handleClient();
    const uint32_t now = millis();
    _esc_sample = _esc_input.sample();
    if (_manual_rtl_active &&
        static_cast<uint32_t>(now - _manual_rtl_at_ms) > RTL_COMMAND_TIMEOUT_MS)
        _manual_rtl_active = false;
    RtlCommandState output = _manual_rtl_active
        ? _manual_rtl : RtlLogic::stateFor(RtlFunction::RUNNING);
    if (!_manual_rtl_active && _esc_enabled && _esc_sample.valid)
        output = RtlLogic::mergeEsc(output, _esc_sample.state);
    _rtl.submit(output, now);
    _rtl_hw.update(_rtl.frame(now));
    if(_pair_active && millis()-_pair_started_ms>30000UL && !_abortPairUpload())
        _pair_started_ms=millis();
    if (_experience.consumeConfigDirty()) _save();
}
#endif
