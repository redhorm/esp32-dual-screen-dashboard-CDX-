#pragma once
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <FS.h>
#include <LovyanGFX.hpp>
#include "core/ui_settings.h"

constexpr uint8_t PHOTO_MAX_SLOTS = 24;

struct PhotoFrameState {
    int8_t   slot = -1;
    uint32_t shown_at_ms = 0;
    bool     loaded = false;
};

class PhotoStore {
public:
    bool begin();
    uint32_t slotMask();
    uint8_t count();
    bool hasBoot(bool navigator);
    bool drawBoot(lgfx::LGFX_Sprite& sprite, bool navigator);
    bool drawFrame(lgfx::LGFX_Sprite& sprite, bool navigator,
                   PhotoFrameState& state, const UiSettings& settings,
                   uint32_t now_ms);

    bool beginUpload(const char* path);
    bool writeUpload(const uint8_t* data, size_t len);
    void endUpload(bool keep);
    bool removeSlot(uint8_t slot);
    size_t usedBytes();
    size_t totalBytes();

    static void photoPath(char* out, size_t len, uint8_t slot, bool navigator);
    static const char* bootPath(bool navigator);

private:
    SemaphoreHandle_t _mutex = nullptr;
    File _upload;
    bool _upload_locked = false;

    bool _take();
    void _give();
    uint32_t _slotMaskUnlocked();
    int _firstSlot(uint32_t mask);
    int _nextSlot(uint32_t mask, int current, bool random, uint32_t seed);
    bool _drawFile(lgfx::LGFX_Sprite& sprite, const char* path,
                   int width, int height);
};

extern PhotoStore photoStore;

#endif
