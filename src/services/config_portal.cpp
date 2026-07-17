#ifndef NATIVE_BUILD
#include <Arduino.h>
#include <stdarg.h>
#include <WiFi.h>
#include <esp_system.h>
#include "config.h"
#include "core/config_integrity.h"
#include "config_portal.h"
#include "settings_page.h"
#include "photo_store.h"

namespace {
constexpr uint16_t PERSISTED_CONFIG_SCHEMA = 5;
constexpr const char* CONFIG_SLOT_A_KEY = "cfg5A";
constexpr const char* CONFIG_SLOT_B_KEY = "cfg5B";
constexpr const char* GHOST_KEY = "ghost5";

#pragma pack(push, 1)
struct PersistedConfigV5 {
    uint8_t driver_theme = 0;
    uint8_t navigator_theme = 0;
    uint8_t language = 0;
    uint8_t flags = 0;
    uint8_t driver_brightness = 220;
    uint8_t navigator_brightness = 220;
    uint8_t photo_target = 3;
    uint8_t photo_transition = 1;
    uint16_t boot_hold_ms = 1800;
    uint16_t photo_interval_s = 12;
    uint8_t rtl_profile = 1;
    uint8_t rtl_brightness = RTL_LED_BRIGHTNESS;
    uint8_t esc_enabled = 0;
    uint8_t esc_type = 0;
    uint16_t esc_neutral_us = ESC_PWM_NEUTRAL_US;
    uint16_t esc_deadband_us = 45;
    uint16_t esc_reverse_confirm_ms = ESC_REVERSE_CONFIRM_MS;
    uint32_t best_score = 0;
    uint16_t wins = 0;
    uint16_t accent = 0xFC40;
    char number[7] = {};
    char vehicle[25] = {};
    char driver[25] = {};
};
#pragma pack(pop)

struct StoredConfigV5 {
    ConfigEnvelope envelope;
    PersistedConfigV5 payload;
};

enum PersistedFlag : uint8_t {
    PERSIST_KMH = 1u << 0,
    PERSIST_CELSIUS = 1u << 1,
    PERSIST_BOOT_IMAGE = 1u << 2,
    PERSIST_PHOTO_MODE = 1u << 3,
    PERSIST_PHOTO_RANDOM = 1u << 4,
    PERSIST_PHOTO_OVERLAY = 1u << 5,
    PERSIST_SETUP_COMPLETE = 1u << 6,
};

static_assert(sizeof(PersistedConfigV5) < 128,
              "Versioned settings record must remain small");

const char* experienceModeName(ExperienceMode mode) {
    return mode == ExperienceMode::GAME ? "game" : "dashboard";
}
const char* gameStatusName(TrailStatus status) {
    switch (status) {
        case TrailStatus::COUNTDOWN: return "countdown";
        case TrailStatus::ATTRACT:   return "attract";
        case TrailStatus::RUNNING:   return "running";
        case TrailStatus::PAUSED:    return "paused";
        case TrailStatus::FINISHED:  return "finished";
        case TrailStatus::FAILED:    return "failed";
        default:                     return "idle";
    }
}
const char* legacyGameStatusName(TrailStatus status) {
    if (status == TrailStatus::RUNNING) return "playing";
    if (status == TrailStatus::FAILED) return "crashed";
    return gameStatusName(status);
}
const char* gameModeName(TrailMode mode) {
    switch (mode) {
        case TrailMode::GATE_RUSH:  return "gate_rush";
        case TrailMode::HILL_CLIMB: return "hill_climb";
        default:                    return "time_attack";
    }
}
const char* gameEventName(TrailEvent event) {
    switch (event) {
        case TrailEvent::CHECKPOINT:         return "checkpoint";
        case TrailEvent::GATE_CLEAR:         return "gate_clear";
        case TrailEvent::GATE_MISSED:        return "gate_missed";
        case TrailEvent::COLLISION_SMALL:    return "collision_small";
        case TrailEvent::COLLISION_LARGE:    return "collision_large";
        case TrailEvent::TERRAIN_CHANGED:    return "terrain_changed";
        case TrailEvent::JUMP:               return "jump";
        case TrailEvent::LANDING:            return "landing";
        case TrailEvent::CONTROLLER_TIMEOUT: return "controller_timeout";
        case TrailEvent::OVERHEAT:           return "overheat";
        case TrailEvent::FINISH:             return "finish";
        default:                             return "none";
    }
}
const char* terrainName(TrailTerrain terrain) {
    switch (terrain) {
        case TrailTerrain::ROCK:  return "rock";
        case TrailTerrain::MUD:   return "mud";
        case TrailTerrain::SAND:  return "sand";
        case TrailTerrain::WATER: return "water";
        case TrailTerrain::JUMP:  return "jump";
        default:                  return "dirt";
    }
}
const char* obstacleKindName(TrailObstacleKind kind) {
    switch (kind) {
        case TrailObstacleKind::BOULDER:   return "boulder";
        case TrailObstacleKind::LOG:       return "log";
        case TrailObstacleKind::RUT:       return "rut";
        case TrailObstacleKind::GATE:      return "gate";
        case TrailObstacleKind::JUMP_RAMP: return "jump_ramp";
        default:                           return "none";
    }
}
const char* audioEventName(AudioEventType type) {
    switch (type) {
        case AudioEventType::ENGINE_START:    return "engine_start";
        case AudioEventType::ENGINE_RPM:      return "engine_rpm";
        case AudioEventType::GEAR_CHANGE:     return "gear_change";
        case AudioEventType::SKID:            return "skid";
        case AudioEventType::COLLISION_SMALL: return "collision_small";
        case AudioEventType::COLLISION_LARGE: return "collision_large";
        case AudioEventType::MUD:             return "mud";
        case AudioEventType::WATER:           return "water";
        case AudioEventType::CHECKPOINT:      return "checkpoint";
        case AudioEventType::COUNTDOWN:       return "countdown";
        case AudioEventType::FINISH:          return "finish";
        case AudioEventType::WARNING:         return "warning";
        case AudioEventType::PAUSE:           return "pause";
        default:                              return "none";
    }
}
const char* inputResultName(TrailInputResult result) {
    switch (result) {
        case TrailInputResult::ACCEPTED:       return "accepted";
        case TrailInputResult::WRONG_SESSION: return "wrong_session";
        case TrailInputResult::STALE_SEQUENCE:return "stale_sequence";
        default:                               return "inactive";
    }
}
const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:  return "power_on";
        case ESP_RST_EXT:      return "external_reset";
        case ESP_RST_SW:       return "software_reset";
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_INT_WDT:  return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT:      return "watchdog";
        case ESP_RST_DEEPSLEEP:return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO:     return "sdio";
        default:               return "unknown";
    }
}
bool appendFormat(char* output, size_t capacity, size_t& position,
                  const char* format, ...) {
    if (position >= capacity) return false;
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(output + position, capacity - position,
                                  format, args);
    va_end(args);
    if (written < 0) {
        position = capacity;
        return false;
    }
    const size_t count = static_cast<size_t>(written);
    if (count >= capacity - position) {
        output[capacity - 1] = '\0';
        position = capacity;
        return false;
    }
    position += count;
    return true;
}
void applyEscDeadband(EscSignalConfig& config, uint16_t deadband_us) {
    EscSignalProcessor::configureDeadbandUs(config, deadband_us);
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
        case PhotoUploadError::GAME_ACTIVE:   return 409;
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
bool parseUnsignedArg(WebServer& server, const char* name,
                      uint32_t minimum, uint32_t maximum,
                      uint32_t& output) {
    if (!server.hasArg(name)) return false;
    const String value = server.arg(name);
    if (!value.length() || value.length() > 10u) return false;
    for (size_t i = 0; i < value.length(); ++i)
        if (value[i] < '0' || value[i] > '9') return false;
    char* end = nullptr;
    const unsigned long parsed = strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed < minimum ||
        parsed > maximum)
        return false;
    output = static_cast<uint32_t>(parsed);
    return true;
}
bool parseSignedArg(WebServer& server, const char* name,
                    int32_t minimum, int32_t maximum, int32_t& output) {
    if (!server.hasArg(name)) return false;
    const String value = server.arg(name);
    if (!value.length() || value.length() > 11u) return false;
    size_t first = value[0] == '-' ? 1u : 0u;
    if (first == value.length()) return false;
    for (size_t i = first; i < value.length(); ++i)
        if (value[i] < '0' || value[i] > '9') return false;
    char* end = nullptr;
    const long parsed = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed < minimum ||
        parsed > maximum)
        return false;
    output = static_cast<int32_t>(parsed);
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
                           Ws2812Rtl& rtl_hw, EscPwmInput& esc_input,
                           DiagnosticsRegistry& diagnostics)
    : _server(80), _experience(experience), _rtl(rtl), _rtl_hw(rtl_hw),
      _esc_input(esc_input), _diagnostics(diagnostics) {}

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

void ConfigPortal::_loadVersionedConfig() {
    StoredConfigV5 slot_a{};
    StoredConfigV5 slot_b{};
    size_t length_a = 0;
    size_t length_b = 0;
    size_t bytes_a = 0;
    size_t bytes_b = 0;
    if (_prefs.begin("ultra4-ui", true)) {
        length_a = _prefs.getBytesLength(CONFIG_SLOT_A_KEY);
        length_b = _prefs.getBytesLength(CONFIG_SLOT_B_KEY);
        if (length_a == sizeof(slot_a))
            bytes_a = _prefs.getBytes(CONFIG_SLOT_A_KEY, &slot_a, sizeof(slot_a));
        if (length_b == sizeof(slot_b))
            bytes_b = _prefs.getBytes(CONFIG_SLOT_B_KEY, &slot_b, sizeof(slot_b));
        _prefs.end();
    }

    ConfigRecordView view_a;
    view_a.envelope = bytes_a == sizeof(slot_a) ? &slot_a.envelope : nullptr;
    view_a.payload = bytes_a == sizeof(slot_a) ? &slot_a.payload : nullptr;
    view_a.available_payload_bytes = bytes_a == sizeof(slot_a)
        ? sizeof(slot_a.payload) : 0u;
    ConfigRecordView view_b;
    view_b.envelope = bytes_b == sizeof(slot_b) ? &slot_b.envelope : nullptr;
    view_b.payload = bytes_b == sizeof(slot_b) ? &slot_b.payload : nullptr;
    view_b.available_payload_bytes = bytes_b == sizeof(slot_b)
        ? sizeof(slot_b.payload) : 0u;
    const ConfigSelection selection = selectNewestConfigRecord(
        view_a, view_b, PERSISTED_CONFIG_SCHEMA, sizeof(PersistedConfigV5));
    const StoredConfigV5* selected = nullptr;
    if (selection.slot == ConfigSlot::A) {
        selected = &slot_a;
        _config_slot = 1;
    } else if (selection.slot == ConfigSlot::B) {
        selected = &slot_b;
        _config_slot = 2;
    }

    if (!selected) {
        const bool record_present = length_a != 0 || length_b != 0;
        if (record_present) {
            // Keep identity, themes and media from the legacy keys but force
            // physical input back to its safe, disabled default.
            _esc_enabled = false;
            _manual_rtl_active = false;
            _diagnostics.setLastError("settings_crc_invalid");
        }
        _config_sequence = 0;
        _config_save_pending = !_saveVersionedConfig();
        _config_retry_at_ms = millis() + 5000u;
        return;
    }

    PersistedConfigV5 data = selected->payload;
    data.number[sizeof(data.number) - 1] = '\0';
    data.vehicle[sizeof(data.vehicle) - 1] = '\0';
    data.driver[sizeof(data.driver) - 1] = '\0';
    _config_sequence = selected->envelope.sequence;
    _settings.driver_theme = sanitizeTheme(data.driver_theme);
    _settings.navigator_theme = sanitizeTheme(data.navigator_theme);
    _settings.language = sanitizeLanguage(data.language);
    _settings.use_kmh = (data.flags & PERSIST_KMH) != 0;
    _settings.use_celsius = (data.flags & PERSIST_CELSIUS) != 0;
    _settings.boot_image_enabled = (data.flags & PERSIST_BOOT_IMAGE) != 0;
    _settings.photo_mode_enabled = (data.flags & PERSIST_PHOTO_MODE) != 0;
    _settings.photo_random = (data.flags & PERSIST_PHOTO_RANDOM) != 0;
    _settings.photo_overlay = (data.flags & PERSIST_PHOTO_OVERLAY) != 0;
    _setup_complete = (data.flags & PERSIST_SETUP_COMPLETE) != 0;
    _settings.driver_brightness = sanitizeBrightness(data.driver_brightness);
    _settings.navigator_brightness = sanitizeBrightness(data.navigator_brightness);
    _settings.photo_target = data.photo_target & 0x03u;
    if (!_settings.photo_target) _settings.photo_target = 3;
    _settings.photo_transition = data.photo_transition ? 1 : 0;
    _settings.boot_hold_ms = clampU16(data.boot_hold_ms, 500, 3500);
    _settings.photo_interval_s = clampU16(data.photo_interval_s, 3, 300);
    _rtl_profile = data.rtl_profile == 0
        ? RtlProfile::RTL_S : RtlProfile::ULTRA4_4400;
    _rtl_brightness = data.rtl_brightness < 8 ? 8 : data.rtl_brightness;
    _esc_enabled = data.esc_enabled != 0;
    _esc_config.type = data.esc_type <=
        static_cast<uint8_t>(EscType::FORWARD_BRAKE_ONLY)
        ? static_cast<EscType>(data.esc_type)
        : EscType::FORWARD_BRAKE_REVERSE;
    _esc_config.neutral_us = clampU16(data.esc_neutral_us, 1300, 1700);
    _esc_deadband_us = clampU16(data.esc_deadband_us, 10, 180);
    _esc_config.reverse_confirm_ms = clampU16(
        data.esc_reverse_confirm_ms, 100, 2000);
    applyEscDeadband(_esc_config, _esc_deadband_us);
    _rtl.configure(_rtl_profile, RTL_LED_COUNT, RTL_COMMAND_TIMEOUT_MS);
    _esc_input.configureSignal(_esc_config);

    RigProfile profile = _experience.config();
    if (data.number[0]) snprintf(profile.number, sizeof(profile.number), "%s", data.number);
    if (data.vehicle[0])
        snprintf(profile.vehicle_name, sizeof(profile.vehicle_name), "%s", data.vehicle);
    if (data.driver[0]) snprintf(profile.driver, sizeof(profile.driver), "%s", data.driver);
    profile.accent = data.accent;
    profile.best_game_score = data.best_score;
    profile.game_wins = data.wins;
    _experience.setConfig(profile);
}

bool ConfigPortal::_saveVersionedConfig() {
    return _saveVersionedConfigWithRtl(
        _rtl_profile, _rtl_brightness, _esc_enabled,
        _esc_config, _esc_deadband_us);
}

bool ConfigPortal::_saveVersionedConfigWithRtl(
        RtlProfile profile_value, uint8_t brightness_value,
        bool esc_enabled_value, const EscSignalConfig& esc_config_value,
        uint16_t deadband_us_value) {
    StoredConfigV5 record{};
    PersistedConfigV5& data = record.payload;
    data.driver_theme = static_cast<uint8_t>(_settings.driver_theme);
    data.navigator_theme = static_cast<uint8_t>(_settings.navigator_theme);
    data.language = static_cast<uint8_t>(_settings.language);
    if (_settings.use_kmh) data.flags |= PERSIST_KMH;
    if (_settings.use_celsius) data.flags |= PERSIST_CELSIUS;
    if (_settings.boot_image_enabled) data.flags |= PERSIST_BOOT_IMAGE;
    if (_settings.photo_mode_enabled) data.flags |= PERSIST_PHOTO_MODE;
    if (_settings.photo_random) data.flags |= PERSIST_PHOTO_RANDOM;
    if (_settings.photo_overlay) data.flags |= PERSIST_PHOTO_OVERLAY;
    if (_setup_complete) data.flags |= PERSIST_SETUP_COMPLETE;
    data.driver_brightness = _settings.driver_brightness;
    data.navigator_brightness = _settings.navigator_brightness;
    data.photo_target = _settings.photo_target;
    data.photo_transition = _settings.photo_transition;
    data.boot_hold_ms = _settings.boot_hold_ms;
    data.photo_interval_s = _settings.photo_interval_s;
    data.rtl_profile = profile_value == RtlProfile::ULTRA4_4400 ? 1 : 0;
    data.rtl_brightness = brightness_value;
    data.esc_enabled = esc_enabled_value ? 1 : 0;
    data.esc_type = static_cast<uint8_t>(esc_config_value.type);
    data.esc_neutral_us = esc_config_value.neutral_us;
    data.esc_deadband_us = deadband_us_value;
    data.esc_reverse_confirm_ms = esc_config_value.reverse_confirm_ms;
    const RigProfile& profile = _experience.config();
    data.best_score = profile.best_game_score;
    data.wins = profile.game_wins;
    data.accent = profile.accent;
    snprintf(data.number, sizeof(data.number), "%s", profile.number);
    snprintf(data.vehicle, sizeof(data.vehicle), "%s", profile.vehicle_name);
    snprintf(data.driver, sizeof(data.driver), "%s", profile.driver);

    const uint32_t sequence = _config_sequence + 1u;
    record.envelope = makeConfigEnvelope(PERSISTED_CONFIG_SCHEMA,
        sizeof(PersistedConfigV5), sequence, &record.payload);
    const bool write_a = _config_slot != 1;
    const char* key = write_a ? CONFIG_SLOT_A_KEY : CONFIG_SLOT_B_KEY;
    if (!_prefs.begin("ultra4-ui", false)) return false;
    const size_t written = _prefs.putBytes(key, &record, sizeof(record));
    StoredConfigV5 verify{};
    const size_t read = written == sizeof(record)
        ? _prefs.getBytes(key, &verify, sizeof(verify)) : 0u;
    _prefs.end();
    const bool valid = read == sizeof(verify) &&
        memcmp(&verify, &record, sizeof(record)) == 0 &&
        validateConfigRecord(verify.envelope, &verify.payload,
            sizeof(verify.payload), PERSISTED_CONFIG_SCHEMA,
            sizeof(PersistedConfigV5)) == ConfigValidation::VALID;
    if (!valid) {
        _diagnostics.setLastError("settings_commit_failed");
        return false;
    }
    _config_sequence = sequence;
    _config_slot = write_a ? 1 : 2;
    return true;
}

void ConfigPortal::_loadGhost() {
    TrailGhostRecording ghost{};
    if (!_prefs.begin("ultra4-ui", true)) return;
    const size_t bytes = _prefs.getBytesLength(GHOST_KEY) == sizeof(ghost)
        ? _prefs.getBytes(GHOST_KEY, &ghost, sizeof(ghost)) : 0u;
    _prefs.end();
    if (bytes && !_experience.loadBestGhost(ghost)) {
        _diagnostics.setLastError("ghost_crc_invalid");
    }
}

bool ConfigPortal::_saveGhost() {
    const TrailGhostRecording& ghost = _experience.bestGhost();
    if (!_prefs.begin("ultra4-ui", false)) return false;
    bool ok = false;
    if (TrailAssaultGame::ghostValid(ghost))
        ok = _prefs.putBytes(GHOST_KEY, &ghost, sizeof(ghost)) == sizeof(ghost);
    else
        ok = _prefs.getBytesLength(GHOST_KEY) == 0u || _prefs.remove(GHOST_KEY);
    _prefs.end();
    if (!ok) _diagnostics.setLastError("ghost_commit_failed");
    return ok;
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
    _rtl_profile = _prefs.getUChar("rtlProfile", 1) == 0
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
    applyEscDeadband(_esc_config, _esc_deadband_us);
    _esc_config.reverse_confirm_ms = clampU16(
        _prefs.getUShort("escRevMs", ESC_REVERSE_CONFIRM_MS), 100, 2000);
    _esc_input.configureSignal(_esc_config);
    _rtl.configure(_rtl_profile, RTL_LED_COUNT, RTL_COMMAND_TIMEOUT_MS);
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

    // Existing per-key beta settings are the migration source.  A valid V5
    // A/B record overrides them; otherwise they are preserved and wrapped in
    // the new CRC-protected format.
    _loadVersionedConfig();
    _loadGhost();

    const esp_reset_reason_t reason = esp_reset_reason();
    _reset_reason_code = static_cast<uint32_t>(reason);
    snprintf(_reset_reason, sizeof(_reset_reason), "%s", resetReasonName(reason));
    _prefs.begin("ultra4-ui", false);
    const uint32_t previous_restarts = _prefs.getULong("restartCnt", 0);
    _restart_count = previous_restarts == UINT32_MAX
        ? UINT32_MAX : previous_restarts + 1u;
    _prefs.putULong("restartCnt", _restart_count);
    _prefs.end();
}

bool ConfigPortal::_save() {
    // Commit the authoritative record first.  If it cannot be verified, do
    // not leak a partially staged physical configuration into legacy keys.
    const bool versioned_ok = _saveVersionedConfig();
    if (!versioned_ok) {
        _diagnostics.setLastError("settings_commit_failed");
        return false;
    }
    (void)_saveLegacyMirror();
    return true;
}

bool ConfigPortal::_saveLegacyMirror() {
    bool legacy_ok = _prefs.begin("ultra4-ui", false);
    if (legacy_ok) {
        legacy_ok = _prefs.putUChar("dashTheme", static_cast<uint8_t>(_settings.driver_theme)) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("navTheme", static_cast<uint8_t>(_settings.navigator_theme)) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("uiLang", static_cast<uint8_t>(_settings.language)) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("kmh", _settings.use_kmh) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("celsius", _settings.use_celsius) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("dashBright", _settings.driver_brightness) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("navBright", _settings.navigator_brightness) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("bootImg", _settings.boot_image_enabled) == 1u && legacy_ok;
        legacy_ok = _prefs.putUShort("bootMs", _settings.boot_hold_ms) == 2u && legacy_ok;
        legacy_ok = _prefs.putBool("photoMode", _settings.photo_mode_enabled) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("photoTarget", _settings.photo_target) == 1u && legacy_ok;
        legacy_ok = _prefs.putUShort("photoSec", _settings.photo_interval_s) == 2u && legacy_ok;
        legacy_ok = _prefs.putUChar("photoFx", _settings.photo_transition) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("photoRnd", _settings.photo_random) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("photoOvl", _settings.photo_overlay) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("setupV4", _setup_complete) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("rtlProfile", _rtl_profile == RtlProfile::ULTRA4_4400 ? 1 : 0) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("rtlBright", _rtl_brightness) == 1u && legacy_ok;
        legacy_ok = _prefs.putBool("escInput", _esc_enabled) == 1u && legacy_ok;
        legacy_ok = _prefs.putUChar("escType", static_cast<uint8_t>(_esc_config.type)) == 1u && legacy_ok;
        legacy_ok = _prefs.putUShort("escNeutral", _esc_config.neutral_us) == 2u && legacy_ok;
        legacy_ok = _prefs.putUShort("escDeadUs", _esc_deadband_us) == 2u && legacy_ok;
        legacy_ok = _prefs.putUShort("escRevMs", _esc_config.reverse_confirm_ms) == 2u && legacy_ok;
        const RigProfile& profile = _experience.config();
        legacy_ok = _prefs.putString("bVehicle", profile.vehicle_name) > 0u && legacy_ok;
        legacy_ok = _prefs.putString("bDriver", profile.driver) > 0u && legacy_ok;
        legacy_ok = _prefs.putString("bNumber", profile.number) > 0u && legacy_ok;
        legacy_ok = _prefs.putUShort("bAccent", profile.accent) == 2u && legacy_ok;
        legacy_ok = _prefs.putULong("bGameBest", profile.best_game_score) == 4u && legacy_ok;
        legacy_ok = _prefs.putUShort("bGameWins", profile.game_wins) == 2u && legacy_ok;
        _prefs.end();
    }
    if (!legacy_ok)
        _diagnostics.setLastError("legacy_settings_mirror_failed");
    // The verified A/B+CRC record is authoritative.  Legacy per-key entries
    // are a downgrade/migration mirror only.
    return legacy_ok;
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

    const UiSettings previous_settings = _settings;
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
    const bool saved = _save();
    if (!saved) _settings = previous_settings;
    _config_save_pending = !saved;
    _config_retry_at_ms = millis() + 5000u;
    _server.send(saved ? 200 : 500, "application/json",
                 saved ? "{\"ok\":true}"
                       : "{\"ok\":false,\"error\":\"settings_commit_failed\"}");
}

void ConfigPortal::_sendPhotos() {
    char json[1280];
    uint32_t mask = photoStore.slotMask();
    const PhotoStorageBreakdown storage = photoStore.storageBreakdown();
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
        "\"systemAssetBytes\":%lu,\"userPhotoBytes\":%lu,"
        "\"gameAssetBytes\":%lu,\"transactionBytes\":%lu,"
        "\"filesystemOverheadBytes\":%lu,\"catalogueGeneration\":%lu,"
        "\"assetLocation\":\"firmware\","
        "\"storageState\":\"%s\",\"partitionLabel\":\"%s\","
        "\"formattedOnBegin\":%s,\"recoveredOnBegin\":%s,\"degraded\":%s,"
        "\"uploadActive\":%s,\"lastError\":\"%s\","
        "\"driverMaxBytes\":%lu,\"navigatorMaxBytes\":%lu,\"reserveBytes\":%lu}",
        slots, PHOTO_MAX_SLOTS,
        photoStore.hasBoot(false) ? "true" : "false",
        photoStore.hasBoot(true) ? "true" : "false",
        (unsigned long)storage.used_bytes, (unsigned long)storage.total_bytes,
        (unsigned long)storage.free_bytes, photoStore.ready() ? "true" : "false",
        (unsigned long)storage.system_asset_bytes,
        (unsigned long)storage.user_photo_bytes,
        (unsigned long)storage.game_asset_bytes,
        (unsigned long)storage.transaction_bytes,
        (unsigned long)storage.filesystem_overhead_bytes,
        (unsigned long)storage.catalogue_generation,
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
    if (_rejectMediaWhileGameActive()) return;
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

    File file;
    if (!photoStore.acquireReadLease(path, file)) {
        _server.send(404, "application/json",
                     "{\"ok\":false,\"error\":\"not_found\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.sendHeader("X-Content-Type-Options", "nosniff");
    _server.streamFile(file, "image/jpeg");
    photoStore.releaseReadLease(file);
}

bool ConfigPortal::_rejectMediaWhileGameActive() {
    return false;
}

void ConfigPortal::_deletePhoto() {
    if (_rejectMediaWhileGameActive()) return;
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
    if (_rejectMediaWhileGameActive()) return;
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
    if (_rejectMediaWhileGameActive()) return;
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
    if (_rejectMediaWhileGameActive()) return;
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
        if (_upload_error == PhotoUploadError::NONE)
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
    char json[640];
    const bool protocol_v4 = _server.uri().startsWith("/api/v4/");
    snprintf(json, sizeof(json),
        "{\"ok\":true,\"protocol\":%u,\"version\":\"%s\",\"features\":["
        "\"trailAssault\",\"seededTracks\",\"fixedStep60Hz\","
        "\"ghostReplay\",\"analogController\",\"webAudioEvents\","
        "\"diagnostics\",\"rtlController\",\"escStateMachine\","
        "\"realismDash\",\"stockOffroadDash\",\"bilingualUi\","
        "\"photoStorageV4\",\"mediaPreview\","
        "\"uniqueApCredentials\",\"rtlOutputWatchdog\"],"
        "\"maxClients\":%u,\"stateHz\":%u,\"inputHz\":%u,"
        "\"physicsHz\":60,\"inputTimeoutMs\":%u}",
        protocol_v4 ? 4u : 3u, EXPERIENCE_VERSION, CONFIG_AP_MAX_CLIENTS,
        protocol_v4 ? 10u : 5u, protocol_v4 ? 20u : 10u,
        TrailAssaultGame::INPUT_TIMEOUT_MS);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void ConfigPortal::_sendExperienceState() {
    // Preserve the v2/v3 wire contract for installed beta clients.  Protocol
    // v4 deliberately exposes the richer game state at top level, whereas
    // legacy clients require mode=dashboard|game plus a nested game object.
    const uint32_t now = millis();
    const ExperienceFrame f = _experience.frame(now);
    const TrailGameSnapshot& game = f.game;
    const int32_t line_x = game.lateral_permille / 12;
    const uint16_t nearest_m = game.visible_objects
        ? static_cast<uint16_t>(game.objects[0].distance_dm / 10u) : 0u;
    const uint8_t lives = game.status == TrailStatus::FAILED ? 0u
        : static_cast<uint8_t>(3u - (game.damage_permille > 666u ? 2u
                                  : game.damage_permille > 333u ? 1u : 0u));

    size_t position = 0;
    _json_buffer[0] = '\0';
    appendFormat(_json_buffer, sizeof(_json_buffer), position,
        "{\"ok\":true,\"version\":\"%s\",\"protocol\":3,"
        "\"nowMs\":%lu,\"mode\":\"%s\","
        "\"identity\":{\"number\":\"%s\",\"vehicle\":\"%s\","
        "\"vehicleName\":\"%s\",\"driver\":\"%s\",\"accent\":%u},"
        "\"vehicle\":{\"speedMph\":%.1f,\"rpm\":%.0f,\"gear\":%u,"
        "\"heading\":%.0f,\"batteryV\":%.1f,\"activeWp\":%u},"
        "\"game\":{\"status\":\"%s\",\"event\":\"%s\","
        "\"x\":%ld,\"lineX\":%ld,\"speed\":%u.%u,"
        "\"nearestDistance\":%u,\"progress\":%u,\"score\":%lu,"
        "\"combo\":0,\"lives\":%u,\"obstacle\":%u,"
        "\"totalObstacles\":%u,\"phaseMs\":%lu,\"steer\":%d,"
        "\"throttle\":%u,\"brake\":%u,\"worldDistance\":%lu,"
        "\"resultAgeMs\":%u,\"elapsedMs\":%lu,"
        "\"visibleObstacles\":%u,\"courseClear\":%s,"
        "\"bestScore\":%lu,\"wins\":%u,\"obstacles\":[",
        EXPERIENCE_VERSION, static_cast<unsigned long>(now),
        experienceModeName(f.mode), f.identity.number, f.identity.vehicle_name,
        f.identity.vehicle_name, f.identity.driver, f.identity.accent,
        f.speed_mph, f.rpm, f.gear, f.heading_deg, f.battery_v, f.active_wp,
        legacyGameStatusName(game.status), gameEventName(game.last_event),
        static_cast<long>(line_x), static_cast<long>(line_x),
        game.speed_kph10 / 10u, game.speed_kph10 % 10u, nearest_m,
        game.progress_permille, static_cast<unsigned long>(game.score), lives,
        game.checkpoint, game.total_checkpoints,
        static_cast<unsigned long>(game.phase_ms), game.controller.steering,
        game.controller.throttle, game.controller.brake,
        static_cast<unsigned long>(game.distance_cm / 100u), game.event_age_ms,
        static_cast<unsigned long>(game.elapsed_ms), game.visible_objects,
        game.course_clear ? "true" : "false",
        static_cast<unsigned long>(f.identity.best_game_score),
        f.identity.game_wins);
    for (uint8_t index = 0; index < game.visible_objects; ++index) {
        const TrailObjectView& object = game.objects[index];
        appendFormat(_json_buffer, sizeof(_json_buffer), position,
            "%s{\"lineX\":%d,\"distance\":%u,\"radius\":%u,"
            "\"kind\":%u,\"kindName\":\"%s\"}", index ? "," : "",
            object.lateral, object.distance_dm / 10u, object.radius,
            static_cast<unsigned>(object.kind), obstacleKindName(object.kind));
    }
    appendFormat(_json_buffer, sizeof(_json_buffer), position, "]}}");
    if (position >= sizeof(_json_buffer)) {
        _diagnostics.setLastError("legacy_json_overflow");
        _server.send(500, "application/json",
                     "{\"ok\":false,\"error\":\"response_too_large\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _json_buffer);
}

void ConfigPortal::_sendGameStateV4() {
    const uint32_t now = millis();
    const ExperienceFrame f = _experience.frame(now);
    const TrailGameSnapshot& game = f.game;
    const TrailGhostRecording& best = _experience.bestGhost();
    const uint16_t after = static_cast<uint16_t>(
        _server.hasArg("audioAfter") ? _server.arg("audioAfter").toInt() : 0);
    AudioEvent audio[AudioEventBus::CAPACITY];
    const uint8_t audio_count = _experience.readAudioEvents(
        after, audio, AudioEventBus::CAPACITY);
    const DiagnosticsSnapshot& diagnostic = _diagnostics.snapshot();
    const uint32_t rpm = clampU16(
        850 + game.speed_kph10 * 5u + game.controller.throttle * 14u,
        750, 7800);

    size_t position = 0;
    _json_buffer[0] = '\0';
    appendFormat(_json_buffer, sizeof(_json_buffer), position,
        "{\"ok\":true,\"version\":\"%s\",\"protocol\":4,"
        "\"serverMs\":%lu,\"nowMs\":%lu,\"experienceMode\":\"%s\","
        "\"status\":\"%s\",\"mode\":\"%s\",\"seed\":%lu,"
        "\"ackSeq\":%u,\"controllerAlive\":%s,\"inputRttMs\":%lu,"
        "\"rpm\":%lu,\"speed\":%u.%u,\"steer\":%d,"
        "\"throttle\":%u,\"brake\":%u,\"score\":%lu,"
        "\"checkpoint\":%u,\"totalCheckpoints\":%u,"
        "\"gate\":%u,\"totalGates\":%u,\"timeMs\":%lu,"
        "\"remainingMs\":%lu,\"bestMs\":%lu,\"damage\":%u,"
        "\"tempC\":%u.%u,\"grip\":%u,\"progress\":%u,"
        "\"deltaMs\":%ld,\"terrain\":\"%s\",\"event\":\"%s\","
        "\"eventAgeMs\":%u,\"courseClear\":%s,\"phaseMs\":%lu,"
        "\"distanceCm\":%lu,\"courseLengthCm\":%lu,"
        "\"worldDistance\":%lu,\"lineX\":%ld,\"x\":%ld,"
        "\"audioLatest\":%u,"
        "\"identity\":{\"number\":\"%s\",\"vehicle\":\"%s\","
        "\"vehicleName\":\"%s\",\"driver\":\"%s\",\"accent\":%u},"
        "\"vehicle\":{\"speedMph\":%.1f,\"rpm\":%.0f,\"gear\":%u,"
        "\"heading\":%.0f,\"batteryV\":%.1f,\"activeWp\":%u},"
        "\"bestScore\":%lu,\"wins\":%u,\"audio\":[",
        EXPERIENCE_VERSION, static_cast<unsigned long>(now),
        static_cast<unsigned long>(now), experienceModeName(f.mode),
        gameStatusName(game.status), gameModeName(game.mode),
        static_cast<unsigned long>(game.seed), game.controller.last_sequence,
        game.controller.timed_out ? "false" : "true",
        static_cast<unsigned long>(diagnostic.input.rtt_last_ms),
        static_cast<unsigned long>(rpm), game.speed_kph10 / 10u,
        game.speed_kph10 % 10u, game.controller.steering,
        game.controller.throttle, game.controller.brake,
        static_cast<unsigned long>(game.score), game.checkpoint,
        game.total_checkpoints, game.gate, game.total_gates,
        static_cast<unsigned long>(game.elapsed_ms),
        static_cast<unsigned long>(game.remaining_ms),
        static_cast<unsigned long>(TrailAssaultGame::ghostValid(best)
            ? best.finish_ms : 0u), game.damage_permille,
        game.temperature_c10 / 10u, game.temperature_c10 % 10u,
        game.grip_permille, game.progress_permille,
        static_cast<long>(game.ghost.time_delta_ms), terrainName(game.terrain),
        gameEventName(game.last_event), game.event_age_ms,
        game.course_clear ? "true" : "false",
        static_cast<unsigned long>(game.phase_ms),
        static_cast<unsigned long>(game.distance_cm),
        static_cast<unsigned long>(game.course_length_cm),
        static_cast<unsigned long>(game.distance_cm / 100u),
        static_cast<long>(game.lateral_permille / 12),
        static_cast<long>(game.lateral_permille / 12),
        _experience.latestAudioSequence(), f.identity.number,
        f.identity.vehicle_name, f.identity.vehicle_name, f.identity.driver,
        f.identity.accent, f.speed_mph, f.rpm, f.gear, f.heading_deg,
        f.battery_v, f.active_wp,
        static_cast<unsigned long>(f.identity.best_game_score),
        f.identity.game_wins);
    for (uint8_t index = 0; index < audio_count; ++index) {
        appendFormat(_json_buffer, sizeof(_json_buffer), position,
            "%s{\"seq\":%u,\"type\":\"%s\",\"value\":%d,"
            "\"intensity\":%u,\"tick\":%lu}", index ? "," : "",
            audio[index].sequence, audioEventName(audio[index].type),
            audio[index].value, audio[index].intensity,
            static_cast<unsigned long>(audio[index].tick));
    }
    appendFormat(_json_buffer, sizeof(_json_buffer), position, "],\"objects\":[");
    for (uint8_t index = 0; index < game.visible_objects; ++index) {
        const TrailObjectView& object = game.objects[index];
        appendFormat(_json_buffer, sizeof(_json_buffer), position,
            "%s{\"distanceDm\":%u,\"lateral\":%d,\"radius\":%u,"
            "\"kind\":\"%s\"}", index ? "," : "", object.distance_dm,
            object.lateral, object.radius, obstacleKindName(object.kind));
    }
    appendFormat(_json_buffer, sizeof(_json_buffer), position, "],\"notes\":[");
    for (uint8_t index = 0; index < game.roadbook_notes; ++index) {
        const TrailRoadbookNote& note = game.notes[index];
        appendFormat(_json_buffer, sizeof(_json_buffer), position,
            "%s{\"distanceM\":%u,\"turn\":%d,\"grade\":%d,"
            "\"terrain\":\"%s\",\"flags\":%u}", index ? "," : "",
            note.distance_m, note.turn_grade, note.grade,
            terrainName(note.terrain), note.flags);
    }
    appendFormat(_json_buffer, sizeof(_json_buffer), position, "]}");
    if (position >= sizeof(_json_buffer)) {
        _diagnostics.setLastError("game_json_overflow");
        _server.send(500, "application/json",
                     "{\"ok\":false,\"error\":\"response_too_large\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _json_buffer);
}

void ConfigPortal::_sendProfile() { _sendExperienceState(); }

void ConfigPortal::_saveProfile() {
    const RigProfile previous_config = _experience.config();
    const bool previous_setup_complete = _setup_complete;
    RigProfile config = previous_config;
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
    const bool saved = _save();
    if (!saved) {
        _experience.setConfig(previous_config);
        _setup_complete = previous_setup_complete;
    }
    _config_save_pending = !saved;
    _config_retry_at_ms = millis() + 5000u;
    _server.send(saved ? 200 : 500, "application/json",
                 saved ? "{\"ok\":true}"
                       : "{\"ok\":false,\"error\":\"profile_commit_failed\"}");
}

void ConfigPortal::_startGame() {
    _experience.startGame(millis(), 1u);
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
    else if (action == "start" || action == "restart") _experience.startGame(now, 1u);
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

void ConfigPortal::_configureGameV4() {
    TrailGameConfig config = _experience.gameConfig();
    const String mode = _server.arg("mode");
    if (mode == "gate_rush" || mode == "GATE_RUSH")
        config.mode = TrailMode::GATE_RUSH;
    else if (mode == "hill_climb" || mode == "HILL_CLIMB")
        config.mode = TrailMode::HILL_CLIMB;
    else if (mode.length() && mode != "time_attack" && mode != "TIME_ATTACK") {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_mode\"}");
        return;
    } else config.mode = TrailMode::TIME_ATTACK;

    if (_server.hasArg("seed")) {
        uint32_t value = 0;
        if (!parseUnsignedArg(_server, "seed", 0u, UINT32_MAX, value)) {
            _server.send(400, "application/json",
                         "{\"ok\":false,\"error\":\"invalid_seed\"}");
            return;
        }
        config.seed = value;
    }
    _experience.configureGame(config);
    _experience.enterGameAttract(millis());
    _sendGameStateV4();
}

void ConfigPortal::_inputGameV4() {
    if (!_server.hasArg("session") || !_server.hasArg("seq") ||
        !_server.hasArg("steer") || !_server.hasArg("throttle") ||
        !_server.hasArg("brake")) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"missing_input\"}");
        return;
    }
    const uint32_t now = millis();
    uint32_t session = 0, sequence_value = 0, client_ms = now;
    uint32_t throttle = 0, brake = 0;
    int32_t steering = 0;
    if (!parseUnsignedArg(_server, "session", 1u, UINT32_MAX, session) ||
        !parseUnsignedArg(_server, "seq", 0u, UINT16_MAX, sequence_value) ||
        !parseSignedArg(_server, "steer", -100, 100, steering) ||
        !parseUnsignedArg(_server, "throttle", 0u, 100u, throttle) ||
        !parseUnsignedArg(_server, "brake", 0u, 100u, brake) ||
        (_server.hasArg("clientMs") &&
         !parseUnsignedArg(_server, "clientMs", 0u, UINT32_MAX, client_ms))) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_input\"}");
        return;
    }
    const uint16_t sequence = static_cast<uint16_t>(sequence_value);
    const TrailInputResult result = _experience.controlGame(
        steering, static_cast<int>(throttle), static_cast<int>(brake),
        session, sequence, client_ms, now);

    const uint32_t offset = now - client_ms;
    if (!_input_offset_known ||
        static_cast<int32_t>(offset - _input_min_offset_ms) < 0) {
        _input_min_offset_ms = offset;
        _input_offset_known = true;
    }
    const uint32_t latency = offset - _input_min_offset_ms;
    const bool accepted = result == TrailInputResult::ACCEPTED;
    _recordInputDiagnostic(sequence, latency, accepted);
    char json[180];
    snprintf(json, sizeof(json),
        "{\"ok\":%s,\"accepted\":%s,\"seq\":%u,\"serverMs\":%lu,"
        "\"result\":\"%s\",\"inputRttMs\":%lu}",
        accepted ? "true" : "false", accepted ? "true" : "false",
        sequence, static_cast<unsigned long>(now), inputResultName(result),
        static_cast<unsigned long>(latency));
    _server.send(accepted ? 200 : 409, "application/json", json);
}

void ConfigPortal::_neutralGameV4() {
    const TrailGameSnapshot game = _experience.frame(millis()).game;
    uint32_t session = game.controller.session_id;
    uint32_t sequence_value = static_cast<uint16_t>(
        game.controller.last_sequence + 1u);
    if ((_server.hasArg("session") &&
         !parseUnsignedArg(_server, "session", 1u, UINT32_MAX, session)) ||
        (_server.hasArg("seq") &&
         !parseUnsignedArg(_server, "seq", 0u, UINT16_MAX, sequence_value))) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_input\"}");
        return;
    }
    const uint16_t sequence = static_cast<uint16_t>(sequence_value);
    const uint32_t now = millis();
    const TrailInputResult result = _experience.neutralGame(session, sequence, now);
    const bool accepted = result == TrailInputResult::ACCEPTED;
    _recordInputDiagnostic(sequence, 0, accepted);
    char json[130];
    snprintf(json, sizeof(json),
        "{\"ok\":%s,\"seq\":%u,\"serverMs\":%lu,\"result\":\"%s\"}",
        accepted ? "true" : "false", sequence,
        static_cast<unsigned long>(now), inputResultName(result));
    _server.send(accepted ? 200 : 409, "application/json", json);
}

void ConfigPortal::_gameActionV4() {
    const String action = _server.arg("action");
    const uint32_t now = millis();
    if (action == "enter") {
        _experience.enterGameAttract(now);
    } else if (action == "start" || action == "restart") {
        uint32_t session = esp_random();
        if (_server.hasArg("session") &&
            !parseUnsignedArg(_server, "session", 1u, UINT32_MAX, session)) {
            _server.send(400, "application/json",
                         "{\"ok\":false,\"error\":\"invalid_session\"}");
            return;
        }
        if (!session) session = 1u;
        _input_offset_known = false;
        _has_input_sequence = false;
        _controller_was_alive = false;
        _experience.startGame(now, session);
    } else if (action == "pause") {
        _experience.pauseGame(now);
    } else if (action == "resume") {
        _experience.resumeGame(now);
    } else if (action == "exit" || action == "quit") {
        const TrailGameSnapshot game = _experience.frame(now).game;
        _experience.neutralGame(game.controller.session_id,
                                game.controller.last_sequence + 1u, now);
        _experience.stopGame();
    } else {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_action\"}");
        return;
    }
    _sendGameStateV4();
}

void ConfigPortal::_recordInputDiagnostic(uint16_t sequence,
                                          uint32_t latency_ms,
                                          bool accepted) {
    if (!accepted) {
        _diagnostics.recordInput(_diagnostic_input_sequence, latency_ms, false);
        return;
    }
    uint16_t distance = 1u;
    if (_has_input_sequence) {
        distance = static_cast<uint16_t>(sequence - _last_input_sequence);
        if (!distance || distance >= 0x8000u) {
            _diagnostics.recordInput(_diagnostic_input_sequence,
                                     latency_ms, false);
            return;
        }
    }
    _diagnostic_input_sequence += distance;
    _last_input_sequence = sequence;
    _has_input_sequence = true;
    _diagnostics.recordInput(_diagnostic_input_sequence, latency_ms, true);
}

void ConfigPortal::_sendDiagnostics() {
    const DiagnosticsSnapshot& data = _diagnostics.snapshot();
    size_t position = 0;
    _json_buffer[0] = '\0';
    appendFormat(_json_buffer, sizeof(_json_buffer), position,
        "{\"ok\":true,\"firmware\":\"%s\",\"protocol\":%u,"
        "\"uptimeMs\":%llu,\"displayA\":{\"ok\":%s,"
        "\"heartbeatAgeMs\":%lu,\"stackFreeBytes\":%lu,\"fps\":%lu,"
        "\"frames\":%llu,\"frameAvgUs\":%lu,\"frameMaxUs\":%lu,"
        "\"renderAvgUs\":%lu,\"renderMaxUs\":%lu,"
        "\"pushAvgUs\":%lu,\"pushMaxUs\":%lu},"
        "\"displayB\":{\"ok\":%s,\"heartbeatAgeMs\":%lu,"
        "\"stackFreeBytes\":%lu,\"fps\":%lu,\"frames\":%llu,"
        "\"frameAvgUs\":%lu,\"frameMaxUs\":%lu,"
        "\"renderAvgUs\":%lu,\"renderMaxUs\":%lu,"
        "\"pushAvgUs\":%lu,\"pushMaxUs\":%lu},"
        "\"memory\":{\"heapFreeBytes\":%lu,\"heapMinBytes\":%lu,"
        "\"heapLargestBytes\":%lu,\"psramPresent\":%s,"
        "\"psramFreeBytes\":%lu,\"psramTotalBytes\":%lu},"
        "\"storage\":{\"ready\":%s,\"totalBytes\":%lu,"
        "\"usedBytes\":%lu,\"freeBytes\":%lu,\"systemAssetBytes\":%lu,"
        "\"userPhotoBytes\":%lu,\"gameAssetBytes\":%lu,"
        "\"transactionBytes\":%lu},"
        "\"wifi\":{\"online\":%s,\"clients\":%u},"
        "\"input\":{\"packets\":%llu,\"accepted\":%llu,"
        "\"lost\":%llu,\"stale\":%llu,\"timeouts\":%llu,"
        "\"latencyMs\":%lu,\"latencyAvgMs\":%lu,\"latencyMaxMs\":%lu,"
        "\"controllerAlive\":%s},"
        "\"rtl\":{\"ok\":%s,\"controlAlive\":%s,"
        "\"controlAgeMs\":%lu,\"controlStackFreeBytes\":%lu},"
        "\"esc\":{\"enabled\":%s,"
        "\"valid\":%s,\"pulseUs\":%u},\"lastError\":\"%s\","
        "\"restartCount\":%lu,\"resetReason\":\"%s\","
        "\"resetReasonCode\":%lu}",
        data.firmware, data.protocol_version,
        static_cast<unsigned long long>(data.uptime_ms),
        data.driver.ok ? "true" : "false",
        static_cast<unsigned long>(data.driver.heartbeat_age_ms),
        static_cast<unsigned long>(data.driver.stack_free_bytes),
        static_cast<unsigned long>(data.driver.fps),
        static_cast<unsigned long long>(data.driver.total_frames),
        static_cast<unsigned long>(data.driver.frame_avg_us),
        static_cast<unsigned long>(data.driver.frame_max_us),
        static_cast<unsigned long>(data.driver.render_avg_us),
        static_cast<unsigned long>(data.driver.render_max_us),
        static_cast<unsigned long>(data.driver.push_avg_us),
        static_cast<unsigned long>(data.driver.push_max_us),
        data.navigator.ok ? "true" : "false",
        static_cast<unsigned long>(data.navigator.heartbeat_age_ms),
        static_cast<unsigned long>(data.navigator.stack_free_bytes),
        static_cast<unsigned long>(data.navigator.fps),
        static_cast<unsigned long long>(data.navigator.total_frames),
        static_cast<unsigned long>(data.navigator.frame_avg_us),
        static_cast<unsigned long>(data.navigator.frame_max_us),
        static_cast<unsigned long>(data.navigator.render_avg_us),
        static_cast<unsigned long>(data.navigator.render_max_us),
        static_cast<unsigned long>(data.navigator.push_avg_us),
        static_cast<unsigned long>(data.navigator.push_max_us),
        static_cast<unsigned long>(data.memory.heap_free_bytes),
        static_cast<unsigned long>(data.memory.heap_min_free_bytes),
        static_cast<unsigned long>(data.memory.heap_largest_block_bytes),
        data.memory.psram_present ? "true" : "false",
        static_cast<unsigned long>(data.memory.psram_free_bytes),
        static_cast<unsigned long>(data.memory.psram_total_bytes),
        data.filesystem.ready ? "true" : "false",
        static_cast<unsigned long>(data.filesystem.total_bytes),
        static_cast<unsigned long>(data.filesystem.used_bytes),
        static_cast<unsigned long>(data.filesystem.free_bytes),
        static_cast<unsigned long>(data.filesystem.system_bytes),
        static_cast<unsigned long>(data.filesystem.user_bytes),
        static_cast<unsigned long>(data.filesystem.game_bytes),
        static_cast<unsigned long>(data.filesystem.transaction_bytes),
        data.network.ap_online ? "true" : "false", data.network.clients,
        static_cast<unsigned long long>(data.input.packets_received),
        static_cast<unsigned long long>(data.input.packets_accepted),
        static_cast<unsigned long long>(data.input.sequence_gaps),
        static_cast<unsigned long long>(data.input.stale_packets),
        static_cast<unsigned long long>(data.input.timeouts),
        static_cast<unsigned long>(data.input.rtt_last_ms),
        static_cast<unsigned long>(data.input.rtt_avg_ms),
        static_cast<unsigned long>(data.input.rtt_max_ms),
        data.input.controller_alive ? "true" : "false",
        data.output.rtl_ok ? "true" : "false",
        data.output.control_alive ? "true" : "false",
        static_cast<unsigned long>(data.output.control_age_ms),
        static_cast<unsigned long>(data.output.control_stack_free_bytes),
        data.output.esc_enabled ? "true" : "false",
        data.output.esc_valid ? "true" : "false", data.output.esc_pulse_us,
        data.last_error, static_cast<unsigned long>(data.reboot_count),
        data.reset_reason, static_cast<unsigned long>(data.reset_reason_code));
    if (position >= sizeof(_json_buffer)) {
        _diagnostics.setLastError("diagnostic_json_overflow");
        _server.send(500, "application/json",
                     "{\"ok\":false,\"error\":\"response_too_large\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _json_buffer);
}

void ConfigPortal::_sendRtlState() {
    RtlFrame frame;
    EscPwmSample esc;
    EscSignalConfig esc_config;
    uint8_t brightness;
    bool esc_enabled;
    taskENTER_CRITICAL(&_io_mux);
    frame = _rtl_frame;
    esc = _esc_sample;
    esc_config = _esc_config;
    brightness = _rtl_brightness;
    esc_enabled = _esc_enabled;
    taskEXIT_CRITICAL(&_io_mux);
    String json;
    json.reserve(1100);
    json += F("{\"ok\":true,\"profile\":\"");
    json += RtlLogic::profileName(frame.profile);
    json += F("\",\"function\":\"");
    json += RtlLogic::functionName(frame.primary);
    json += F("\",\"brightness\":");
    json += static_cast<unsigned>(brightness);
    json += F(",\"hardwareReady\":");
    json += _rtl_hw.ready() ? F("true") : F("false");
    json += F(",\"timedOut\":");
    json += frame.command_timed_out ? F("true") : F("false");
    json += F(",\"escEnabled\":");
    json += esc_enabled ? F("true") : F("false");
    json += F(",\"escValid\":");
    json += esc.valid ? F("true") : F("false");
    json += F(",\"escPulse\":");
    json += esc.filtered_us;
    json += F(",\"escType\":\"");
    json += EscSignalProcessor::typeName(esc_config.type);
    json += F("\",\"escState\":\"");
    json += EscSignalProcessor::stateName(esc.state);
    json += F("\",\"escNormalized\":");
    json += static_cast<int>(esc.normalized * 100.0f);
    json += F(",\"escNeutralUs\":"); json += esc_config.neutral_us;
    json += F(",\"escDeadbandUs\":"); json += _esc_deadband_us;
    json += F(",\"escReverseConfirmMs\":"); json += esc_config.reverse_confirm_ms;
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
    const String profile_value = _server.arg("profile");
    const String esc_type_value = _server.arg("escType");
    const bool profile_valid = profile_value == "rtl_s" ||
                               profile_value == "ultra4_4400";
    const bool type_valid = esc_type_value == "forward_brake_reverse" ||
                            esc_type_value == "crawler_forward_reverse" ||
                            esc_type_value == "forward_brake_only";
    uint32_t brightness_value = 0;
    uint32_t enabled_value = 0;
    uint32_t neutral_value = 0;
    uint32_t deadband_value = 0;
    uint32_t reverse_ms_value = 0;
    if (!profile_valid || !type_valid ||
        !parseUnsignedArg(_server, "brightness", 8, 255, brightness_value) ||
        !parseUnsignedArg(_server, "escEnabled", 0, 1, enabled_value) ||
        !parseUnsignedArg(_server, "escNeutralUs", 1300, 1700,
                          neutral_value) ||
        !parseUnsignedArg(_server, "escDeadbandUs", 10, 180,
                          deadband_value) ||
        !parseUnsignedArg(_server, "escReverseConfirmMs", 100, 2000,
                          reverse_ms_value)) {
        _server.send(400, "application/json",
                     "{\"ok\":false,\"error\":\"invalid_rtl_config\"}");
        return;
    }

    const RtlProfile profile = profile_value == "rtl_s"
        ? RtlProfile::RTL_S : RtlProfile::ULTRA4_4400;
    EscSignalConfig esc_config;
    taskENTER_CRITICAL(&_io_mux);
    esc_config = _esc_config;
    taskEXIT_CRITICAL(&_io_mux);
    const bool esc_enabled = enabled_value != 0u;
    esc_config.type = EscSignalProcessor::typeFromName(esc_type_value.c_str());
    esc_config.neutral_us = static_cast<uint16_t>(neutral_value);
    const uint16_t deadband_us = static_cast<uint16_t>(deadband_value);
    esc_config.reverse_confirm_ms = static_cast<uint16_t>(reverse_ms_value);
    applyEscDeadband(esc_config, deadband_us);

    // Persist the complete candidate while the control task continues using
    // the old live configuration.  Only a byte-for-byte verified A/B record
    // is allowed to become physical state.
    const bool saved = _saveVersionedConfigWithRtl(
        profile, static_cast<uint8_t>(brightness_value), esc_enabled,
        esc_config, deadband_us);
    if (!saved) {
        _server.send(500, "application/json",
                     "{\"ok\":false,\"error\":\"rtl_commit_failed\"}");
        return;
    }
    taskENTER_CRITICAL(&_io_mux);
    _rtl_profile = profile;
    _rtl_brightness = static_cast<uint8_t>(brightness_value);
    _esc_enabled = esc_enabled;
    _esc_config = esc_config;
    _esc_deadband_us = deadband_us;
    _manual_rtl_active = false;
    _io_reconfigure_pending = true;
    taskEXIT_CRITICAL(&_io_mux);
    _config_save_pending = false;
    (void)_saveLegacyMirror();
    _sendRtlState();
}

void ConfigPortal::_commandRtl() {
    const String value = _server.arg("function");
    if (value == "pit_test") {
        EscPwmSample esc;
        bool esc_enabled;
        taskENTER_CRITICAL(&_io_mux);
        esc = _esc_sample;
        esc_enabled = _esc_enabled;
        taskEXIT_CRITICAL(&_io_mux);
        const bool esc_neutral = esc.valid &&
            esc.state == EscDriveState::NEUTRAL &&
            esc.state_age_ms >= ESC_NEUTRAL_DWELL_MS;
        if (!esc_enabled) {
            _server.send(409, "application/json",
                         "{\"ok\":false,\"error\":\"esc_required_for_pit\"}");
            return;
        }
        if (!esc_neutral) {
            _server.send(409, "application/json",
                         "{\"ok\":false,\"error\":\"esc_not_neutral\"}");
            return;
        }
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
    taskENTER_CRITICAL(&_io_mux);
    _manual_rtl_active = !clear;
    _manual_rtl_at_ms = now;
    _manual_rtl = RtlLogic::stateFor(clear ? RtlFunction::RUNNING
                                           : rtlFunctionFromArg(value));
    taskEXIT_CRITICAL(&_io_mux);
    _sendRtlState();
}

bool ConfigPortal::begin() {
    bool fs_ok = photoStore.begin();
    _load();
    _rtl_hw.setBrightness(_rtl_brightness);
    _rtl_hw.begin();
    _esc_input.begin(_esc_enabled);
    controlIoTick();
    WiFi.mode(WIFI_AP);
    // The dashboard is an offline appliance: avoid Wi-Fi power save latency
    // and declare the captive-gateway address explicitly for iOS/Android.
    WiFi.setSleep(false);
    const IPAddress ap_ip(192, 168, 4, 1);
    const IPAddress ap_mask(255, 255, 255, 0);
    if (!WiFi.softAPConfig(ap_ip, ap_ip, ap_mask))
        Serial.println("[portal] AP static IP setup failed; using SDK default");
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
    _server.on("/api/v4/capabilities", HTTP_GET, [this] { _sendCapabilities(); });
    _server.on("/api/v2/state", HTTP_GET, [this] { _sendExperienceState(); });
    _server.on("/api/v2/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v2/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v3/profile", HTTP_GET, [this] { _sendProfile(); });
    _server.on("/api/v3/profile", HTTP_POST, [this] { _saveProfile(); });
    _server.on("/api/v3/state", HTTP_GET, [this] { _sendExperienceState(); });
    _server.on("/api/v2/game/start", HTTP_POST, [this] { _startGame(); });
    _server.on("/api/v2/game/input", HTTP_POST, [this] { _inputGame(); });
    _server.on("/api/v2/game/action", HTTP_POST, [this] { _gameAction(); });
    _server.on("/api/v4/game/state", HTTP_GET, [this] { _sendGameStateV4(); });
    _server.on("/api/v4/game/config", HTTP_POST, [this] { _configureGameV4(); });
    _server.on("/api/v4/game/action", HTTP_POST, [this] { _gameActionV4(); });
    _server.on("/api/v4/game/input", HTTP_POST, [this] { _inputGameV4(); });
    _server.on("/api/v4/game/neutral", HTTP_POST, [this] { _neutralGameV4(); });
    _server.on("/api/v4/diagnostics", HTTP_GET, [this] { _sendDiagnostics(); });
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

void ConfigPortal::controlIoTick() {
    const uint32_t now = millis();
    bool reconfigure;
    bool esc_enabled;
    uint8_t brightness;
    RtlProfile profile;
    EscSignalConfig esc_config;
    bool manual_active;
    uint32_t manual_at;
    RtlCommandState manual;

    taskENTER_CRITICAL(&_io_mux);
    reconfigure = _io_reconfigure_pending;
    _io_reconfigure_pending = false;
    esc_enabled = _esc_enabled;
    brightness = _rtl_brightness;
    profile = _rtl_profile;
    esc_config = _esc_config;
    manual_active = _manual_rtl_active;
    manual_at = _manual_rtl_at_ms;
    manual = _manual_rtl;
    taskEXIT_CRITICAL(&_io_mux);

    if (reconfigure) {
        _rtl.configure(profile, RTL_LED_COUNT, RTL_COMMAND_TIMEOUT_MS);
        _rtl_hw.setBrightness(brightness);
        _esc_input.configureSignal(esc_config);
        _esc_input.begin(esc_enabled);
    }

    if (manual_active &&
        static_cast<uint32_t>(now - manual_at) > RTL_COMMAND_TIMEOUT_MS) {
        manual_active = false;
        taskENTER_CRITICAL(&_io_mux);
        if (_manual_rtl_active && _manual_rtl_at_ms == manual_at)
            _manual_rtl_active = false;
        taskEXIT_CRITICAL(&_io_mux);
    }

    const EscPwmSample esc = _esc_input.sample();
    // PIT test is a stationary-only diagnostic.  Abort it immediately if the
    // physical ESC leaves neutral or its signal disappears after activation.
    if (manual_active && manual.pit_test && esc_enabled &&
        (!esc.valid || esc.state != EscDriveState::NEUTRAL)) {
        manual_active = false;
        taskENTER_CRITICAL(&_io_mux);
        if (_manual_rtl_active && _manual_rtl.pit_test)
            _manual_rtl_active = false;
        taskEXIT_CRITICAL(&_io_mux);
    }
    RtlCommandState output = manual_active
        ? manual : RtlLogic::stateFor(RtlFunction::RUNNING);
    // The real drive signal always owns brake/reverse, while web commands may
    // still add turn, hazard or dust indications.  A manual animation must
    // never hide actual braking for up to the command timeout.
    if (esc_enabled && esc.valid)
        output = RtlLogic::mergeEsc(output, esc.state);
    _rtl.submit(output, now);
    const RtlFrame frame = _rtl.frame(now);
    _rtl_hw.update(frame);

    taskENTER_CRITICAL(&_io_mux);
    _esc_sample = esc;
    _rtl_frame = frame;
    taskEXIT_CRITICAL(&_io_mux);
}

void ConfigPortal::fillDiagnosticsSlowHealth(DiagnosticsSlowHealth& health) {
    const PhotoStorageBreakdown storage = photoStore.storageBreakdown();
    health.filesystem.ready = photoStore.ready();
    health.filesystem.total_bytes = static_cast<uint32_t>(storage.total_bytes);
    health.filesystem.used_bytes = static_cast<uint32_t>(storage.used_bytes);
    health.filesystem.free_bytes = static_cast<uint32_t>(storage.free_bytes);
    health.filesystem.system_bytes = static_cast<uint32_t>(storage.system_asset_bytes);
    health.filesystem.user_bytes = static_cast<uint32_t>(storage.user_photo_bytes);
    health.filesystem.game_bytes = static_cast<uint32_t>(storage.game_asset_bytes);
    health.filesystem.transaction_bytes =
        static_cast<uint32_t>(storage.transaction_bytes);
    const wifi_mode_t wifi_mode = WiFi.getMode();
    health.network.ap_online = wifi_mode == WIFI_AP || wifi_mode == WIFI_AP_STA;
    health.network.clients = static_cast<uint8_t>(WiFi.softAPgetStationNum());
    health.reboot_count = _restart_count;
    health.reset_reason_code = _reset_reason_code;
    health.reset_reason = _reset_reason;

    taskENTER_CRITICAL(&_io_mux);
    health.output.rtl_ok = _rtl_hw.ready();
    health.output.esc_enabled = _esc_enabled;
    health.output.esc_valid = _esc_sample.valid;
    health.output.esc_pulse_us = _esc_sample.filtered_us;
    taskEXIT_CRITICAL(&_io_mux);
    if (health.last_error == nullptr) {
        if (!photoStore.mounted()) health.last_error = "littlefs_unavailable";
        else if (photoStore.degraded()) health.last_error = "media_degraded";
        else if (!health.output.rtl_ok) health.last_error = "rtl_unavailable";
    }
}

void ConfigPortal::handle() {
    _dns.processNextRequest();
    _server.handleClient();
    const uint32_t now = millis();
    if (photoStore.abortStaleUpload(now)) {
        _upload_started = false;
        _upload_ok = false;
        _upload_error = PhotoUploadError::INTERRUPTED;
    }
    if (_pair_active && now - _pair_started_ms > 30000UL &&
        !_abortPairUpload())
        _pair_started_ms = now;
    if (_experience.consumeConfigDirty()) {
        _config_save_pending = true;
        _config_retry_at_ms = now;
    }
    if (_experience.consumeGhostDirty()) {
        _ghost_save_pending = true;
        _ghost_retry_at_ms = now;
    }
    if (_config_save_pending &&
        static_cast<int32_t>(now - _config_retry_at_ms) >= 0) {
        _config_save_pending = !_save();
        _config_retry_at_ms = now + 5000u;
    }
    if (_ghost_save_pending &&
        static_cast<int32_t>(now - _ghost_retry_at_ms) >= 0) {
        _ghost_save_pending = !_saveGhost();
        _ghost_retry_at_ms = now + 5000u;
    }
    const TrailGameSnapshot game = _experience.frame(now).game;
    if (game.status == TrailStatus::RUNNING) {
        const bool alive = !game.controller.timed_out;
        if (_controller_was_alive && !alive) _diagnostics.recordTimeout();
        else _diagnostics.setControllerAlive(alive);
        _controller_was_alive = alive;
    } else {
        _controller_was_alive = false;
        _diagnostics.setControllerAlive(false);
    }
}
#endif
