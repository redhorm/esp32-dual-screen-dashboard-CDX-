#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "config_portal.h"
#include "settings_page.h"
#include "photo_store.h"

namespace {
const char* betaModeName(BetaMode mode) {
    switch (mode) {
        case BetaMode::SHOW: return "moment";
        case BetaMode::HYPE: return "callout";
        case BetaMode::GAME: return "game";
        default:             return "dashboard";
    }
}
const char* gameStatusName(RockGameStatus status) {
    switch (status) {
        case RockGameStatus::COUNTDOWN: return "countdown";
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
}

ConfigPortal::ConfigPortal(SocialBetaEngine& beta) : _server(80), _beta(beta) {}

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
    BetaConfig beta = _beta.config();
    String value = _prefs.getString("bCall", beta.callsign);
    value.toCharArray(beta.callsign, sizeof(beta.callsign));
    value = _prefs.getString("bTeam", beta.team);
    value.toCharArray(beta.team, sizeof(beta.team));
    value = _prefs.getString("bHandle", beta.handle);
    value.toCharArray(beta.handle, sizeof(beta.handle));
    value = _prefs.getString("bNumber", beta.number);
    value.toCharArray(beta.number, sizeof(beta.number));
    beta.accent = _prefs.getUShort("bAccent", beta.accent);
    beta.best_game_score = _prefs.getULong("bGameBest", beta.best_game_score);
    beta.game_wins = _prefs.getUShort("bGameWins", beta.game_wins);
    _beta.setConfig(beta);
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
    const BetaConfig& beta = _beta.config();
    _prefs.putString("bCall", beta.callsign);
    _prefs.putString("bTeam", beta.team);
    _prefs.putString("bHandle", beta.handle);
    _prefs.putString("bNumber", beta.number);
    _prefs.putUShort("bAccent", beta.accent);
    _prefs.putULong("bGameBest", beta.best_game_score);
    _prefs.putUShort("bGameWins", beta.game_wins);
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
    char json[320];
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
        "\"usedBytes\":%lu,\"totalBytes\":%lu}", slots, PHOTO_MAX_SLOTS,
        photoStore.hasBoot(false) ? "true" : "false",
        photoStore.hasBoot(true) ? "true" : "false",
        (unsigned long)photoStore.usedBytes(), (unsigned long)photoStore.totalBytes());
    _server.send(200, "application/json", json);
}

void ConfigPortal::_deletePhoto() {
    int slot = _server.arg("slot").toInt();
    bool ok = slot >= 0 && slot < PHOTO_MAX_SLOTS && photoStore.removeSlot(slot);
    _server.send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void ConfigPortal::_deleteBoot() {
    // Empty uploads atomically replace the two fixed boot files; removing is
    // handled by the same locked upload path with keep=false.
    bool any = false;
    if (photoStore.beginUpload(PhotoStore::bootPath(false))) { photoStore.endUpload(false); any = true; }
    if (photoStore.beginUpload(PhotoStore::bootPath(true)))  { photoStore.endUpload(false); any = true; }
    _server.send(any ? 200 : 500, "application/json", any ? "{\"ok\":true}" : "{\"ok\":false}");
}

void ConfigPortal::_handleUpload(bool boot, bool navigator) {
    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        char path[24];
        if (boot) snprintf(path, sizeof(path), "%s", PhotoStore::bootPath(navigator));
        else {
            int slot = _server.arg("slot").toInt();
            if (slot < 0 || slot >= PHOTO_MAX_SLOTS) { _upload_ok = false; return; }
            PhotoStore::photoPath(path, sizeof(path), slot, navigator);
        }
        _upload_ok = photoStore.beginUpload(path);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (_upload_ok) _upload_ok = photoStore.writeUpload(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        photoStore.endUpload(_upload_ok && upload.totalSize > 128);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        photoStore.endUpload(false); _upload_ok = false;
    }
}

void ConfigPortal::_finishUpload() {
    _server.send(_upload_ok ? 200 : 500, "application/json",
                 _upload_ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

void ConfigPortal::_sendCapabilities() {
    char json[320];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"protocol\":2,\"version\":\"%s\",\"features\":["
        "\"liveMirror\",\"moments\",\"spotter\",\"badges\",\"reelForge\",\"rockLineRush\"],"
        "\"maxClients\":%u,\"stateHz\":4,\"inputHz\":8}",
        SOCIAL_BETA_VERSION, CONFIG_AP_MAX_CLIENTS);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendBetaState() {
    uint32_t now = millis();
    BetaFrame f = _beta.frame(now);
    char json[1400];
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"version\":\"%s\",\"nowMs\":%lu,\"mode\":\"%s\","
        "\"identity\":{\"callsign\":\"%s\",\"team\":\"%s\",\"handle\":\"%s\","
        "\"number\":\"%s\",\"accent\":%u},"
        "\"vehicle\":{\"speedMph\":%.1f,\"rpm\":%.0f,\"gear\":%u,\"heading\":%.0f,"
        "\"batteryV\":%.1f,\"activeWp\":%u},"
        "\"moment\":{\"name\":\"%s\",\"ageMs\":%lu},"
        "\"callout\":{\"name\":\"%s\",\"text\":\"%s\",\"ageMs\":%lu},"
        "\"badges\":{\"mask\":%lu,\"latest\":\"%s\",\"ageMs\":%lu},"
        "\"game\":{\"status\":\"%s\",\"result\":\"%s\",\"x\":%d,\"safeX\":%d,"
        "\"speed\":%u,\"distanceToGate\":%u,\"progress\":%u,\"score\":%lu,"
        "\"combo\":%u,\"lives\":%u,\"gate\":%u,\"gateTotal\":%u,\"phaseMs\":%lu,"
        "\"bestScore\":%lu,\"wins\":%u}}",
        SOCIAL_BETA_VERSION, (unsigned long)now, betaModeName(f.mode),
        f.identity.callsign, f.identity.team, f.identity.handle, f.identity.number,
        f.identity.accent, f.speed_mph, f.rpm, f.gear, f.heading_deg, f.battery_v,
        f.active_wp, SocialBetaEngine::showName(f.show_program), (unsigned long)f.mode_ms,
        SocialBetaEngine::hypeName(f.hype), f.hype_text, (unsigned long)f.mode_ms,
        (unsigned long)f.badge_mask, SocialBetaEngine::badgeName(f.latest_badge),
        (unsigned long)f.badge_age_ms, gameStatusName(f.game.status),
        gateResultName(f.game.last_result), f.game.line_x, f.game.safe_x,
        f.game.speed, f.game.distance_to_gate, f.game.progress_permille,
        (unsigned long)f.game.score, f.game.combo, f.game.lives, f.game.gate,
        f.game.total_gates, (unsigned long)f.game.phase_ms,
        (unsigned long)f.identity.best_game_score, f.identity.game_wins);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendProfile() { _sendBetaState(); }

void ConfigPortal::_saveProfile() {
    BetaConfig config = _beta.config();
    copyDisplaySafe(config.callsign, sizeof(config.callsign), _server.arg("callsign"));
    copyDisplaySafe(config.team, sizeof(config.team), _server.arg("team"));
    copyDisplaySafe(config.handle, sizeof(config.handle), _server.arg("handle"), true);
    copyDisplaySafe(config.number, sizeof(config.number), _server.arg("number"));
    config.accent = accentFromName(_server.arg("accent"));
    if (!config.callsign[0] || !config.team[0] || !config.number[0]) {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid profile\"}");
        return;
    }
    _beta.setConfig(config);
    _save();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_setMoment() {
    String action = _server.arg("action");
    if (action == "stop") {
        _beta.stopShow();
        _server.send(200, "application/json", "{\"ok\":true}");
        return;
    }
    String type = _server.arg("type");
    ShowProgram program = ShowProgram::MUD_SPLASH;
    if (type == "water_ford") program = ShowProgram::WATER_FORD;
    else if (type == "night_raid") program = ShowProgram::NIGHT_RAID;
    else if (type == "winch_pull") program = ShowProgram::WINCH_PULL;
    else if (type == "send_it") program = ShowProgram::SEND_IT;
    _beta.startShow(program, millis());
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_setCallout() {
    String type = _server.arg("type");
    HypeReaction cue = HypeReaction::CUSTOM;
    if (type == "left") cue = HypeReaction::LEFT;
    else if (type == "right") cue = HypeReaction::RIGHT;
    else if (type == "stop") cue = HypeReaction::STOP;
    else if (type == "water") cue = HypeReaction::WATER;
    else if (type == "clear") cue = HypeReaction::CLEAR;
    else if (type == "winch") cue = HypeReaction::WINCH;
    else if (type == "champion") cue = HypeReaction::CHAMPION;
    char custom[25] = "";
    copyDisplaySafe(custom, sizeof(custom), _server.arg("text"));
    _beta.triggerHype(cue, custom, millis());
    _server.send(200, "application/json", "{\"ok\":true}");
}

void ConfigPortal::_startGame() {
    _beta.startGame(millis());
    _sendBetaState();
}

void ConfigPortal::_inputGame() {
    int steer = _server.arg("steer").toInt();
    bool throttle = _server.arg("throttle").toInt() > 0;
    _beta.controlGame(steer, throttle, millis());
    _sendBetaState();
}

void ConfigPortal::_gameAction() {
    String action = _server.arg("action");
    if (action == "restart") _beta.startGame(millis());
    else _beta.stopGame();
    _sendBetaState();
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
    _server.on("/api/v2/state", HTTP_GET, [this] { _sendBetaState(); });
    _server.on("/api/v2/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v2/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v2/moment", HTTP_POST, [this] { _setMoment(); });
    _server.on("/api/v2/callout", HTTP_POST, [this] { _setCallout(); });
    _server.on("/api/v2/game/start", HTTP_POST, [this] { _startGame(); });
    _server.on("/api/v2/game/input", HTTP_POST, [this] { _inputGame(); });
    _server.on("/api/v2/game/action", HTTP_POST, [this] { _gameAction(); });
    _server.on("/api/photos", HTTP_GET, [this] { _sendPhotos(); });
    _server.on("/api/photos/delete", HTTP_POST, [this] { _deletePhoto(); });
    _server.on("/api/boot/delete", HTTP_POST, [this] { _deleteBoot(); });
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
    return ok && fs_ok;
}

void ConfigPortal::handle() {
    _dns.processNextRequest();
    _server.handleClient();
    if (_beta.consumeConfigDirty()) _save();
}
#endif
