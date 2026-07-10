#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "config_portal.h"
#include "settings_page.h"
#include "photo_store.h"

ConfigPortal::ConfigPortal() : _server(80) {}

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
    _server.onNotFound([this] { _sendPortal(); });
    _server.begin();
    return ok && fs_ok;
}

void ConfigPortal::handle() {
    _dns.processNextRequest();
    _server.handleClient();
}
#endif
