#pragma once
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <FS.h>
#include <LovyanGFX.hpp>
#include "core/ui_settings.h"

constexpr uint8_t PHOTO_MAX_SLOTS = 24;
constexpr size_t PHOTO_MIN_JPEG_BYTES = 128;
constexpr size_t PHOTO_DRIVER_MAX_BYTES = 64 * 1024;
constexpr size_t PHOTO_NAV_MAX_BYTES = 192 * 1024;
constexpr size_t PHOTO_STORAGE_RESERVE_BYTES = 32 * 1024;
constexpr const char* PHOTO_FS_PARTITION_LABEL = "littlefs";
constexpr const char* PHOTO_FS_LEGACY_PARTITION_LABEL = "spiffs";
constexpr const char* PHOTO_FS_BASE_PATH = "/littlefs";
constexpr uint8_t PHOTO_FS_MAX_OPEN_FILES = 10;

enum class PhotoUploadError : uint8_t {
    NONE,
    INVALID_SLOT,
    INVALID_TRANSACTION,
    BUSY,
    FILESYSTEM,
    NO_SPACE,
    TOO_LARGE,
    INVALID_IMAGE,
    WRITE_FAILED,
    COMMIT_FAILED,
    INTERRUPTED,
};

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
    bool endUpload(bool keep);
    bool commitSlotUpload(uint8_t slot);
    bool abortSlotUpload(uint8_t slot);
    bool commitBootUpload();
    bool abortBootUpload();
    bool removeSlot(uint8_t slot);
    bool removeBoot();
    bool ready() const { return _ready && !_degraded; }
    bool mounted() const { return _ready; }
    bool degraded() const { return _degraded; }
    bool formattedOnBegin() const { return _formatted_on_begin; }
    bool recoveredOnBegin() const { return _recovered_on_begin; }
    bool uploadActive() const { return _upload_locked; }
    const char* partitionLabel() const { return _partition_label; }
    const char* storageStateName() const;
    size_t freeBytes();
    size_t usedBytes();
    size_t totalBytes();
    PhotoUploadError uploadError() const { return _upload_error; }
    static const char* uploadErrorName(PhotoUploadError error);

    static void photoPath(char* out, size_t len, uint8_t slot, bool navigator);
    static void photoStagePath(char* out, size_t len, uint8_t slot, bool navigator);
    static const char* bootPath(bool navigator);
    static const char* bootStagePath(bool navigator);

private:
    SemaphoreHandle_t _mutex = nullptr;
    File _upload;
    bool _upload_locked = false;
    bool _ready = false;
    bool _degraded = false;
    bool _formatted_on_begin = false;
    bool _recovered_on_begin = false;
    char _partition_label[12] = {};
    String _upload_target;
    size_t _upload_bytes = 0;
    size_t _upload_limit = PHOTO_NAV_MAX_BYTES;
    size_t _upload_free_at_start = 0;
    PhotoUploadError _upload_error = PhotoUploadError::NONE;

    bool _take();
    void _give();
    uint32_t _slotMaskUnlocked();
    bool _validJpegUnlocked(const char* path);
    bool _validUploadTarget(const char* path) const;
    bool _commitPairUnlocked(const char* stage_a, const char* stage_b,
                             const char* target_a, const char* target_b,
                             const char* backup_a, const char* backup_b);
    bool _recoverPairUnlocked(const char* target_a, const char* target_b,
                              const char* backup_a, const char* backup_b);
    bool _scrubUnlocked();
    int _firstSlot(uint32_t mask);
    int _nextSlot(uint32_t mask, int current, bool random, uint32_t seed);
    bool _drawFile(lgfx::LGFX_Sprite& sprite, const char* path,
                   int width, int height);
};

extern PhotoStore photoStore;

#endif
