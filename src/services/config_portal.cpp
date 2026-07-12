#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "config_portal.h"
#include "settings_page.h"
#include "photo_store.h"

namespace {
const char* experienceModeName(ExperienceMode mode) {
    switch (mode) {
        case ExperienceMode::RIFT: return "rift";
        case ExperienceMode::GAME: return "game";
        default:             return "dashboard";
    }
}
const char* gameStatusName(RockGameStatus status) {
    switch (status) {
        case RockGameStatus::COUNTDOWN: return "countdown";
        case RockGameStatus::ATTRACT:   return "attract";
        case RockGameStatus::PLAYING:   return "playing";
        case RockGameStatus::CRASHED:   return "crashed";
        case RockGameStatus::FINISHED:  return "finished";
        default:                        return "idle";
    }
}
const char* gateResultName(GateResult result) {
    switch (result) {
        case GateResult::PERFECT: return "perfect";
        case GateResult::CLEAN:   return "clean";
        case GateResult::HIT:     return "hit";
        default:                  return "none";
    }
}
void copyDisplaySafe(char* out, size_t len, const String& source, bool allow_at = false) {
    if (!len) return;
    size_t n = 0;
    for (size_t i = 0; i < source.length() && n + 1 < len; i++) {
        char c = source[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        bool safe = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == ' ' || c == '.' || c == '_' || c == '-' ||
                    (allow_at && c == '@');
        if (safe) out[n++] = c;
    }
    while (n && out[n - 1] == ' ') n--;
    out[n] = '\0';
}
uint16_t accentFromName(const String& value) {
    if (value == "amber") return 0xFD03;
    if (value == "cyan")  return 0x67BF;
    if (value == "sand")  return 0xDE73;
    return 0xCFE4;
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

ConfigPortal::ConfigPortal(ExperienceEngine& experience)
    : _server(80), _experience(experience) {}

void ConfigPortal::_load() {
    _prefs.begin("ultra4-ui", true);
    _settings.driver_theme = sanitizeTheme(_prefs.getUChar("dashTheme", 0));
    _settings.navigator_theme = sanitizeTheme(_prefs.getUChar("navTheme", 0));
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
    RigProfile profile = _experience.config();
    String value = _prefs.getString("bCall", profile.callsign);
    value.toCharArray(profile.callsign, sizeof(profile.callsign));
    value = _prefs.getString("bTeam", profile.team);
    value.toCharArray(profile.team, sizeof(profile.team));
    value = _prefs.getString("bHandle", profile.handle);
    value.toCharArray(profile.handle, sizeof(profile.handle));
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
    const RigProfile& profile = _experience.config();
    _prefs.putString("bCall", profile.callsign);
    _prefs.putString("bTeam", profile.team);
    _prefs.putString("bHandle", profile.handle);
    _prefs.putString("bNumber", profile.number);
    _prefs.putUShort("bAccent", profile.accent);
    _prefs.putULong("bGameBest", profile.best_game_score);
    _prefs.putUShort("bGameWins", profile.game_wins);
    _prefs.end();
}

void ConfigPortal::_sendPortal() {
    _server.send_P(200, "text/html; charset=utf-8", SETTINGS_PAGE);
}

void ConfigPortal::_sendSettings() {
    char json[420];
    snprintf(json, sizeof(json),
        "{\"driverTheme\":%u,\"navigatorTheme\":%u,\"useKmh\":%s,"
        "\"useCelsius\":%s,\"driverBrightness\":%u,\"navigatorBrightness\":%u,"
        "\"bootImageEnabled\":%s,\"bootHoldMs\":%u,\"photoModeEnabled\":%s,"
        "\"photoTarget\":%u,\"photoIntervalS\":%u,\"photoTransition\":%u,"
        "\"photoRandom\":%s,\"photoOverlay\":%s}",
        static_cast<unsigned>(_settings.driver_theme),
        static_cast<unsigned>(_settings.navigator_theme),
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
    if (!_server.hasArg("driverTheme") || !_server.hasArg("navigatorTheme") ||
        !_server.hasArg("units") || !_server.hasArg("temperature") ||
        !_server.hasArg("driverBrightness") || !_server.hasArg("navigatorBrightness")) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing fields\"}");
        return;
    }

    _settings.driver_theme = sanitizeTheme(_server.arg("driverTheme").toInt());
    _settings.navigator_theme = sanitizeTheme(_server.arg("navigatorTheme").toInt());
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
    char json[420];
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
        "{\"slots\":%s,\"maxSlots\":%u,\"bootDriver\":%s,\"bootNavigator\":%s,"
        "\"usedBytes\":%lu,\"totalBytes\":%lu,\"freeBytes\":%lu,\"fsReady\":%s}",
        slots, PHOTO_MAX_SLOTS,
        photoStore.hasBoot(false) ? "true" : "false",
        photoStore.hasBoot(true) ? "true" : "false",
        (unsigned long)photoStore.usedBytes(), (unsigned long)photoStore.totalBytes(),
        (unsigned long)photoStore.freeBytes(), photoStore.ready() ? "true" : "false");
    _server.send(200, "application/json", json);
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
    char json[320];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"protocol\":2,\"version\":\"%s\",\"features\":["
        "\"riftRun\",\"neonTrailRush\",\"photoStorageV2\"],"
        "\"maxClients\":%u,\"stateHz\":4,\"inputHz\":8}",
        EXPERIENCE_VERSION, CONFIG_AP_MAX_CLIENTS);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendExperienceState() {
    uint32_t now = millis();
    ExperienceFrame f = _experience.frame(now);
    char json[1700];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"version\":\"%s\",\"nowMs\":%lu,\"mode\":\"%s\","
        "\"identity\":{\"callsign\":\"%s\",\"team\":\"%s\",\"handle\":\"%s\","
        "\"number\":\"%s\",\"accent\":%u},"
        "\"vehicle\":{\"speedMph\":%.1f,\"rpm\":%.0f,\"gear\":%u,\"heading\":%.0f,"
        "\"batteryV\":%.1f,\"activeWp\":%u},"
        "\"rift\":{\"phase\":\"%s\",\"runId\":%lu,\"seed\":%lu,"
        "\"startAtMs\":%lu,\"elapsedMs\":%lu,\"progress\":%u,"
        "\"layout\":%u,\"durationMs\":%lu},"
        "\"game\":{\"status\":\"%s\",\"result\":\"%s\",\"x\":%d,\"safeX\":%d,"
        "\"speed\":%u,\"distanceToGate\":%u,\"progress\":%u,\"score\":%lu,"
        "\"combo\":%u,\"lives\":%u,\"gate\":%u,\"gateTotal\":%u,\"phaseMs\":%lu,"
        "\"steer\":%d,\"throttle\":%u,\"worldDistance\":%u,"
        "\"resultAgeMs\":%u,\"elapsedMs\":%lu,"
        "\"bestScore\":%lu,\"wins\":%u}}",
        EXPERIENCE_VERSION, (unsigned long)now, experienceModeName(f.mode),
        f.identity.callsign, f.identity.team, f.identity.handle, f.identity.number,
        f.identity.accent, f.speed_mph, f.rpm, f.gear, f.heading_deg, f.battery_v,
        f.active_wp, ExperienceEngine::riftPhaseName(f.rift.phase),
        (unsigned long)f.rift.run_id, (unsigned long)f.rift.seed,
        (unsigned long)f.rift.start_at_ms, (unsigned long)f.rift.elapsed_ms,
        f.rift.progress_permille, f.rift.layout, (unsigned long)RIFT_DURATION_MS,
        gameStatusName(f.game.status),
        gateResultName(f.game.last_result), f.game.line_x, f.game.safe_x,
        f.game.speed, f.game.distance_to_gate, f.game.progress_permille,
        (unsigned long)f.game.score, f.game.combo, f.game.lives, f.game.gate,
        f.game.total_gates, (unsigned long)f.game.phase_ms, f.game.steering,
        f.game.throttle, f.game.world_distance, f.game.result_age_ms,
        (unsigned long)f.game.elapsed_ms,
        (unsigned long)f.identity.best_game_score, f.identity.game_wins);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendProfile() { _sendExperienceState(); }

void ConfigPortal::_saveProfile() {
    RigProfile config = _experience.config();
    copyDisplaySafe(config.callsign, sizeof(config.callsign), _server.arg("callsign"));
    copyDisplaySafe(config.team, sizeof(config.team), _server.arg("team"));
    copyDisplaySafe(config.handle, sizeof(config.handle), _server.arg("handle"), true);
    copyDisplaySafe(config.number, sizeof(config.number), _server.arg("number"));
    config.accent = accentFromName(_server.arg("accent"));
    if (!config.callsign[0] || !config.team[0] || !config.number[0]) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid profile\"}");
        return;
    }
    _experience.setConfig(config);
    _save();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_startRift() {
    uint8_t layout=(uint8_t)_server.arg("layout").toInt();
    _experience.startRift(millis(),layout);
    _sendExperienceState();
}

void ConfigPortal::_stopRift() {
    _experience.stopRift();
    _sendExperienceState();
}

void ConfigPortal::_startGame() {
    _experience.startGame(millis());
    _sendExperienceState();
}

void ConfigPortal::_inputGame() {
    int steer = _server.arg("steer").toInt();
    bool throttle = _server.arg("throttle").toInt() > 0;
    _experience.controlGame(steer, throttle, millis());
    _sendExperienceState();
}

void ConfigPortal::_gameAction() {
    String action = _server.arg("action");
    if(action=="enter")_experience.enterGameAttract(millis());
    else if(action=="restart")_experience.startGame(millis());
    else _experience.stopGame();
    _sendExperienceState();
}

bool ConfigPortal::begin() {
    bool fs_ok = photoStore.begin();
    _load();
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASSWORD,
                          CONFIG_AP_CHANNEL, false, CONFIG_AP_MAX_CLIENTS);
    IPAddress ip = WiFi.softAPIP();
    snprintf(_ip, sizeof(_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    _dns.start(53, "*", ip);
    _server.on("/", HTTP_GET, [this] { _sendPortal(); });
    _server.on("/api/settings", HTTP_GET, [this] { _sendSettings(); });
    _server.on("/api/settings", HTTP_POST, [this] { _saveSettings(); });
    _server.on("/api/v2/capabilities", HTTP_GET, [this] { _sendCapabilities(); });
    _server.on("/api/v2/state", HTTP_GET, [this] { _sendExperienceState(); });
    _server.on("/api/v2/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v2/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v2/rift/start", HTTP_POST, [this] { _startRift(); });
    _server.on("/api/v2/rift/stop", HTTP_POST, [this] { _stopRift(); });
    _server.on("/api/v2/game/start", HTTP_POST, [this] { _startGame(); });
    _server.on("/api/v2/game/input", HTTP_POST, [this] { _inputGame(); });
    _server.on("/api/v2/game/action", HTTP_POST, [this] { _gameAction(); });
    _server.on("/api/photos", HTTP_GET, [this] { _sendPhotos(); });
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
    if(_pair_active && millis()-_pair_started_ms>30000UL && !_abortPairUpload())
        _pair_started_ms=millis();
    if (_experience.consumeConfigDirty()) _save();
}
#endif
