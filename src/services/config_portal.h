#pragma once
#ifndef NATIVE_BUILD

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "config.h"
#include "core/ui_settings.h"
#include "core/experience.h"
#include "core/diagnostics.h"
#include "core/rtl_logic.h"
#include "hal/ws2812_rtl.h"
#include "hal/esc_pwm_input.h"
#include "hal/copilot_display_profile.h"
#include "photo_store.h"

class ConfigPortal {
public:
    ConfigPortal(ExperienceEngine& experience, RtlLogic& rtl,
                 Ws2812Rtl& rtl_hw, EscPwmInput& esc_input,
                 DiagnosticsRegistry& diagnostics);
    bool begin();
    void handle();
    // Called by the dedicated high-priority control task.  No web, NVS or
    // filesystem work is performed here.
    void controlIoTick();
    EscPwmSample escDashboardInput();
    void fillDiagnosticsSlowHealth(DiagnosticsSlowHealth& health);
    UiSettings settings() const {
        taskENTER_CRITICAL(&_settings_mux);
        const UiSettings snapshot = _settings;
        taskEXIT_CRITICAL(&_settings_mux);
        return snapshot;
    }
    const char* ipString() const { return _ip; }
    const char* ssidString() const { return _ssid; }
    const char* passwordString() const { return _password; }
    bool setupComplete() const { return _setup_complete; }
    // The HTTP task exchanges bounded commands/snapshots with the display loop.
    bool takeShowCommand(bool& start);
    void publishShowState(bool active, uint32_t elapsed_ms);
    // Read before display initialisation so the selected panel/controller can
    // be configured safely.  Invalid or missing values are repaired to the
    // proven 240×135 landscape fallback.
    CopilotDisplayProfile loadCopilotDisplayProfile();
    CopilotDisplayProfile copilotDisplayProfile() const {
        return _copilot_display_profile;
    }

private:
    DNSServer   _dns;
    WebServer   _server;
    Preferences _prefs;
    UiSettings  _settings;
    mutable portMUX_TYPE _settings_mux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE _show_mux = portMUX_INITIALIZER_UNLOCKED;
    ExperienceEngine& _experience;
    RtlLogic&   _rtl;
    Ws2812Rtl&  _rtl_hw;
    EscPwmInput& _esc_input;
    DiagnosticsRegistry& _diagnostics;
    char        _ip[16] = "0.0.0.0";
    char        _ssid[24] = CONFIG_AP_SSID_PREFIX;
    char        _password[16] = {};
    bool        _setup_complete = true;
    bool        _show_active = false;
    uint32_t    _show_elapsed_ms = 0;
    bool        _show_command_pending = false;
    bool        _show_requested_start = false;
    CopilotDisplayProfile _copilot_display_profile =
        copilotDisplayDefaultProfile();
    bool        _copilot_profile_loaded = false;
    // The profile response must reach captive-portal browsers before a reboot.
    // Restarting directly inside the HTTP handler can drop the response on iOS.
    bool        _copilot_restart_pending = false;
    uint32_t    _copilot_restart_at_ms = 0;
    uint8_t     _rtl_brightness = RTL_LED_BRIGHTNESS;
    RtlProfile  _rtl_profile = RtlProfile::ULTRA4_4400;
    bool        _esc_enabled = ESC_PWM_INPUT_ENABLED != 0;
    EscSignalConfig _esc_config;
    uint16_t    _esc_deadband_us = 45;
    uint16_t    _esc_full_forward_us = 0;
    EscPwmSample _esc_sample;
    uint32_t _esc_sample_at_ms = 0;
    RtlFrame     _rtl_frame;
    portMUX_TYPE _io_mux = portMUX_INITIALIZER_UNLOCKED;
    bool        _io_reconfigure_pending = false;
    bool        _pwm_reconfigure_pending = false;
    RtlCommandState _manual_rtl;
    uint32_t    _manual_rtl_at_ms = 0;
    bool        _manual_rtl_active = false;
    bool        _upload_ok = false;
    bool        _upload_seen = false;
    bool        _upload_started = false;
    bool        _upload_navigator = false;
    PhotoUploadError _upload_error = PhotoUploadError::NONE;
    bool        _pair_active = false;
    bool        _pair_boot = false;
    bool        _pair_a_ready = false;
    bool        _pair_b_ready = false;
    uint8_t     _pair_slot = 0;
    uint32_t    _pair_started_ms = 0;
    char        _pair_tx[17] = "";
    char        _json_buffer[4096] = {};
    uint32_t    _restart_count = 0;
    uint32_t    _reset_reason_code = 0;
    char        _reset_reason[DIAGNOSTICS_RESET_TEXT_SIZE] = "unknown";
    uint32_t    _config_sequence = 0;
    uint8_t     _config_slot = 0;
    bool        _config_save_pending = false;
    uint32_t    _config_retry_at_ms = 0;


    void _load();
    bool _save();
    bool _saveLegacyMirror();
    void _loadVersionedConfig();
    bool _saveVersionedConfig();
    bool _saveVersionedConfigWithRtl(RtlProfile profile,
                                     uint8_t brightness,
                                     bool esc_enabled,
                                     const EscSignalConfig& esc_config,
                                     uint16_t deadband_us);
    void _loadNetworkIdentity();
    void _sendShow();
    void _commandShow();
    void _sendSettings();
    void _saveSettings();
    void _sendCopilotDisplay();
    void _saveCopilotDisplay();
    void _sendPortal();
    void _sendPhotos();
    void _sendMediaAsset(bool boot);
    void _deletePhoto();
    void _deleteBoot();
    void _commitPhotoUpload();
    void _abortPhotoUpload();
    void _commitBootUpload();
    void _abortBootUpload();
    void _handleUpload(bool boot, bool navigator);
    void _finishUpload();
    bool _preparePairUpload(bool boot, bool navigator, uint8_t slot);
    bool _pairRequestMatches(bool boot, uint8_t slot);
    bool _abortPairUpload();
    void _resetPairUpload();
    void _sendCapabilities();
    void _sendExperienceState();
    void _sendDiagnostics();
    void _sendProfile();
    void _saveProfile();
    void _sendRtlState();
    enum class ConfigDomain { Combined, Esc, Lights };
    void _saveRtlConfig(ConfigDomain domain = ConfigDomain::Combined);
    void _calibrateForward();
    void _commandRtl();
};

#endif
