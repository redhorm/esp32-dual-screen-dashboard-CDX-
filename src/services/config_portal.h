#pragma once
#ifndef NATIVE_BUILD

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "core/ui_settings.h"

class ConfigPortal {
public:
    ConfigPortal();
    bool begin();
    void handle();
    const UiSettings& settings() const { return _settings; }
    const char* ipString() const { return _ip; }

private:
    DNSServer   _dns;
    WebServer   _server;
    Preferences _prefs;
    UiSettings  _settings;
    char        _ip[16] = "0.0.0.0";
    bool        _upload_ok = false;

    void _load();
    void _save();
    void _sendSettings();
    void _saveSettings();
    void _sendPortal();
    void _sendPhotos();
    void _deletePhoto();
    void _deleteBoot();
    void _handleUpload(bool boot, bool navigator);
    void _finishUpload();
};

#endif
