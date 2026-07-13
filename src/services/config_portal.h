#pragma once
#ifndef NATIVE_BUILD

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "core/ui_settings.h"
#include "core/experience.h"
#include "core/rtl_logic.h"
#include "hal/ws2812_rtl.h"
#include "hal/esc_pwm_input.h"
#include "photo_store.h"

class ConfigPortal {
public:
    ConfigPortal(ExperienceEngine& experience, RtlLogic& rtl,
                 Ws2812Rtl& rtl_hw, EscPwmInput& esc_input);
    bool begin();
    void handle();
    const UiSettings& settings() const { return _settings; }
    const char* ipString() const { return _ip; }
    const char* ssidString() const { return _ssid; }
    const char* passwordString() const { return _password; }
    bool setupComplete() const { return _setup_complete; }

private:
    DNSServer   _dns;
    WebServer   _server;
    Preferences _prefs;
    UiSettings  _settings;
    ExperienceEngine& _experience;
    RtlLogic&   _rtl;
    Ws2812Rtl&  _rtl_hw;
    EscPwmInput& _esc_input;
    char        _ip[16] = "0.0.0.0";
    char        _ssid[24] = CONFIG_AP_SSID_PREFIX;
    char        _password[16] = {};
    bool        _setup_complete = false;
    uint8_t     _rtl_brightness = RTL_LED_BRIGHTNESS;
    bool        _esc_enabled = ESC_PWM_INPUT_ENABLED != 0;
    EscSignalConfig _esc_config;
    uint16_t    _esc_deadband_us = 45;
    EscPwmSample _esc_sample;
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

    void _load();
    void _save();
    void _loadNetworkIdentity();
    void _sendSettings();
    void _saveSettings();
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
    void _sendProfile();
    void _saveProfile();
    void _startGame();
    void _inputGame();
    void _gameAction();
    void _sendRtlState();
    void _saveRtlConfig();
    void _commandRtl();
};

#endif
