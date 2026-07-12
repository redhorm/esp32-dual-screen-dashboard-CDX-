#pragma once
#ifndef NATIVE_BUILD

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "core/ui_settings.h"
#include "core/experience.h"
#include "photo_store.h"

class ConfigPortal {
public:
    explicit ConfigPortal(ExperienceEngine& experience);
    bool begin();
    void handle();
    const UiSettings& settings() const { return _settings; }
    const char* ipString() const { return _ip; }

private:
    DNSServer   _dns;
    WebServer   _server;
    Preferences _prefs;
    UiSettings  _settings;
    ExperienceEngine& _experience;
    char        _ip[16] = "0.0.0.0";
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
    void _sendSettings();
    void _saveSettings();
    void _sendPortal();
    void _sendPhotos();
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
    void _startRift();
    void _stopRift();
    void _startGame();
    void _inputGame();
    void _gameAction();
};

#endif
