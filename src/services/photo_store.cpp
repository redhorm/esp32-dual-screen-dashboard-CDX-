#ifndef NATIVE_BUILD
#include <LittleFS.h>
#include "photo_store.h"

PhotoStore photoStore;

bool PhotoStore::begin() {
    _degraded=false;
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex || !LittleFS.begin(true)) return false;
    _ready = true;
    bool healthy=false;
    if (_take()) { healthy=_scrubUnlocked(); _give(); }
    _degraded=!healthy;
    return healthy;
}

bool PhotoStore::_take() {
    return _mutex && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE;
}

void PhotoStore::_give() {
    if (_mutex) xSemaphoreGive(_mutex);
}

void PhotoStore::photoPath(char* out, size_t len, uint8_t slot, bool navigator) {
    snprintf(out, len, "/p%02u_%c.jpg", slot, navigator ? 'b' : 'a');
}

void PhotoStore::photoStagePath(char* out, size_t len, uint8_t slot, bool navigator) {
    snprintf(out, len, "/.p%02u_%c.new", slot, navigator ? 'b' : 'a');
}

const char* PhotoStore::bootPath(bool navigator) {
    return navigator ? "/boot_b.jpg" : "/boot_a.jpg";
}

const char* PhotoStore::bootStagePath(bool navigator) {
    return navigator ? "/.boot_b.new" : "/.boot_a.new";
}

uint32_t PhotoStore::_slotMaskUnlocked() {
    uint32_t mask = 0;
    char a[20], b[20];
    for (uint8_t i = 0; i < PHOTO_MAX_SLOTS; i++) {
        photoPath(a, sizeof(a), i, false);
        photoPath(b, sizeof(b), i, true);
        if (LittleFS.exists(a) && LittleFS.exists(b)) mask |= (1UL << i);
    }
    return mask;
}

bool PhotoStore::_validJpegUnlocked(const char* path) {
    if (!LittleFS.exists(path)) return false;
    File file = LittleFS.open(path, "r");
    if (!file || file.size() < PHOTO_MIN_JPEG_BYTES) { if (file) file.close(); return false; }
    uint8_t head[2] = {0,0}, tail[2] = {0,0};
    bool ok = file.read(head,2) == 2 && file.seek(file.size()-2) && file.read(tail,2) == 2;
    file.close();
    return ok && head[0] == 0xFF && head[1] == 0xD8 &&
           tail[0] == 0xFF && tail[1] == 0xD9;
}

bool PhotoStore::_recoverPairUnlocked(const char* target_a, const char* target_b,
                                      const char* backup_a, const char* backup_b) {
    bool ta=_validJpegUnlocked(target_a), tb=_validJpegUnlocked(target_b);
    bool ba=_validJpegUnlocked(backup_a), bb=_validJpegUnlocked(backup_b);
    if (ta && tb) {
        LittleFS.remove(backup_a);
        LittleFS.remove(backup_b);
        return !LittleFS.exists(backup_a)&&!LittleFS.exists(backup_b);
    }
    if (ba && bb) {
        LittleFS.remove(target_a);
        LittleFS.remove(target_b);
        LittleFS.rename(backup_a, target_a);
        LittleFS.rename(backup_b, target_b);
        return _validJpegUnlocked(target_a)&&_validJpegUnlocked(target_b)&&
               !LittleFS.exists(backup_a)&&!LittleFS.exists(backup_b);
    }
    // Power can fail between the two old->backup renames, or between the
    // two rollback renames. Preserve and complete those mixed valid pairs.
    if (ba && tb && !ta && !bb) {
        LittleFS.remove(target_a);
        LittleFS.rename(backup_a,target_a);
        return _validJpegUnlocked(target_a)&&_validJpegUnlocked(target_b)&&
               !LittleFS.exists(backup_a);
    }
    if (ta && bb && !tb && !ba) {
        LittleFS.remove(target_b);
        LittleFS.rename(backup_b,target_b);
        return _validJpegUnlocked(target_a)&&_validJpegUnlocked(target_b)&&
               !LittleFS.exists(backup_b);
    }
    LittleFS.remove(target_a); LittleFS.remove(target_b);
    LittleFS.remove(backup_a); LittleFS.remove(backup_b);
    return !LittleFS.exists(target_a)&&!LittleFS.exists(target_b)&&
           !LittleFS.exists(backup_a)&&!LittleFS.exists(backup_b);
}

bool PhotoStore::_commitPairUnlocked(const char* stage_a, const char* stage_b,
                                     const char* target_a, const char* target_b,
                                     const char* backup_a, const char* backup_b) {
    if (!_validJpegUnlocked(stage_a) || !_validJpegUnlocked(stage_b)) {
        _upload_error = PhotoUploadError::INVALID_IMAGE;
        return false;
    }

    if (LittleFS.exists(backup_a) || LittleFS.exists(backup_b)) {
        bool recovered=_recoverPairUnlocked(target_a,target_b,backup_a,backup_b);
        bool ta=_validJpegUnlocked(target_a),tb=_validJpegUnlocked(target_b);
        if (!recovered || LittleFS.exists(backup_a) || LittleFS.exists(backup_b) || ta!=tb) {
            if(!recovered)_degraded=true;
            _upload_error = PhotoUploadError::COMMIT_FAILED;
            return false;
        }
    }
    bool had_pair = _validJpegUnlocked(target_a) && _validJpegUnlocked(target_b);
    if (had_pair) {
        if (!LittleFS.rename(target_a, backup_a)) {
            _upload_error = PhotoUploadError::COMMIT_FAILED;
            return false;
        }
        if (!LittleFS.rename(target_b, backup_b)) {
            if(!_recoverPairUnlocked(target_a,target_b,backup_a,backup_b))_degraded=true;
            _upload_error = PhotoUploadError::COMMIT_FAILED;
            return false;
        }
    } else {
        LittleFS.remove(target_a);
        LittleFS.remove(target_b);
    }

    if (!LittleFS.rename(stage_a, target_a)) {
        if (had_pair&&!_recoverPairUnlocked(target_a,target_b,backup_a,backup_b))_degraded=true;
        _upload_error = PhotoUploadError::COMMIT_FAILED;
        return false;
    }
    if (!LittleFS.rename(stage_b, target_b)) {
        LittleFS.remove(target_a);
        if (had_pair&&!_recoverPairUnlocked(target_a,target_b,backup_a,backup_b))_degraded=true;
        _upload_error = PhotoUploadError::COMMIT_FAILED;
        return false;
    }
    LittleFS.remove(backup_a);
    LittleFS.remove(backup_b);
    return true;
}

bool PhotoStore::_scrubUnlocked() {
    bool healthy=true;
    LittleFS.remove("/.upload.tmp");
    const char* boot_a = bootPath(false);
    const char* boot_b = bootPath(true);
    LittleFS.remove(bootStagePath(false));
    LittleFS.remove(bootStagePath(true));
    bool recovered=_recoverPairUnlocked(boot_a, boot_b, "/.boot_a.bak", "/.boot_b.bak");
    if(!recovered)recovered=_recoverPairUnlocked(boot_a,boot_b,"/.boot_a.bak","/.boot_b.bak");
    healthy=healthy&&recovered;
    char a[20], b[20], sa[20], sb[20], ba[20], bb[20];
    for (uint8_t i=0;i<PHOTO_MAX_SLOTS;i++) {
        photoPath(a,sizeof(a),i,false); photoPath(b,sizeof(b),i,true);
        photoStagePath(sa,sizeof(sa),i,false); photoStagePath(sb,sizeof(sb),i,true);
        snprintf(ba,sizeof(ba),"/.p%02u_a.bak",i);
        snprintf(bb,sizeof(bb),"/.p%02u_b.bak",i);
        LittleFS.remove(sa); LittleFS.remove(sb);
        recovered=_recoverPairUnlocked(a,b,ba,bb);
        if(!recovered)recovered=_recoverPairUnlocked(a,b,ba,bb);
        healthy=healthy&&recovered;
    }
    return healthy;
}

uint32_t PhotoStore::slotMask() {
    if (!_ready || !_take()) return 0;
    uint32_t mask = _slotMaskUnlocked();
    _give();
    return mask;
}

uint8_t PhotoStore::count() {
    return __builtin_popcount(slotMask());
}

bool PhotoStore::hasBoot(bool navigator) {
    if (!_ready || !_take()) return false;
    bool found = LittleFS.exists(bootPath(navigator));
    _give();
    return found;
}

bool PhotoStore::_drawFile(lgfx::LGFX_Sprite& sprite, const char* path,
                           int width, int height) {
    if (!LittleFS.exists(path)) return false;
    sprite.fillScreen(0x0000);
    return sprite.drawJpgFile(LittleFS, path, 0, 0, width, height, 0, 0, 1.0f);
}

bool PhotoStore::drawBoot(lgfx::LGFX_Sprite& sprite, bool navigator) {
    if (!_ready || !_take()) return false;
    bool ok = _drawFile(sprite, bootPath(navigator),
                        navigator ? 240 : 160, navigator ? 240 : 80);
    _give();
    return ok;
}

int PhotoStore::_firstSlot(uint32_t mask) {
    for (int i = 0; i < PHOTO_MAX_SLOTS; i++) if (mask & (1UL << i)) return i;
    return -1;
}

int PhotoStore::_nextSlot(uint32_t mask, int current, bool random, uint32_t seed) {
    int n = __builtin_popcount(mask);
    if (n <= 1) return _firstSlot(mask);
    if (random) {
        int pick = (int)((seed * 1664525UL + 1013904223UL) % (uint32_t)n);
        for (int i = 0; i < PHOTO_MAX_SLOTS; i++) {
            if (!(mask & (1UL << i))) continue;
            if (pick-- == 0) return i == current ? _nextSlot(mask, current, false, seed) : i;
        }
    }
    for (int step = 1; step <= PHOTO_MAX_SLOTS; step++) {
        int i = (current + step) % PHOTO_MAX_SLOTS;
        if (mask & (1UL << i)) return i;
    }
    return _firstSlot(mask);
}

bool PhotoStore::drawFrame(lgfx::LGFX_Sprite& sprite, bool navigator,
                           PhotoFrameState& state, const UiSettings& settings,
                           uint32_t now_ms) {
    if (!_ready || !_take()) return false;
    uint32_t mask = _slotMaskUnlocked();
    if (!mask) { _give(); return false; }

    if (state.slot < 0 || !(mask & (1UL << state.slot))) {
        state.slot = _firstSlot(mask);
        state.shown_at_ms = now_ms;
        state.loaded = false;
    }
    uint32_t interval_ms = (uint32_t)settings.photo_interval_s * 1000UL;
    if (now_ms - state.shown_at_ms >= interval_ms) {
        state.slot = _nextSlot(mask, state.slot, settings.photo_random, now_ms);
        state.shown_at_ms = now_ms;
        state.loaded = false;
    }

    uint32_t age = now_ms - state.shown_at_ms;
    bool dissolving = settings.photo_transition == 1 && age < 650;
    if (!state.loaded || dissolving) {
        char path[20];
        photoPath(path, sizeof(path), state.slot, navigator);
        state.loaded = _drawFile(sprite, path,
                                 navigator ? 240 : 160, navigator ? 240 : 80);
    }
    _give();
    if (!state.loaded) return false;

    // Memory-free dissolve: a deterministic black dither mask thins out over
    // 650 ms, avoiding a second 240x240 framebuffer.
    if (dissolving) {
        int threshold = 16 - (int)(age * 16 / 650);
        int step = navigator ? 6 : 4;
        int w = navigator ? 240 : 160, h = navigator ? 240 : 80;
        for (int y = 0; y < h; y += step)
            for (int x = 0; x < w; x += step)
                if (((x * 3 + y * 5) / step) % 16 < threshold)
                    sprite.fillRect(x, y, step, step, 0x0000);
    }

    if (settings.photo_overlay) {
        int h = navigator ? 20 : 12;
        int y = (navigator ? 240 : 80) - h;
        sprite.fillRect(0, y, navigator ? 240 : 160, h, 0x0000);
        sprite.setFont(&lgfx::fonts::Font0);
        sprite.setTextColor(0xFFFF, 0x0000);
        char label[28];
        snprintf(label, sizeof(label), "ULTRA4  %02u/%02u  %lus",
                 (unsigned)(__builtin_popcount(mask & ((1UL << (state.slot + 1)) - 1))),
                 (unsigned)__builtin_popcount(mask),
                 (unsigned long)(interval_ms - (now_ms - state.shown_at_ms)) / 1000UL);
        sprite.drawString(label, navigator ? 7 : 4, y + (navigator ? 6 : 2));
    }
    return true;
}

bool PhotoStore::beginUpload(const char* path) {
    _upload_error = PhotoUploadError::NONE;
    if (!ready()) { _upload_error=PhotoUploadError::FILESYSTEM; return false; }
    if (_upload_locked) { _upload_error=PhotoUploadError::BUSY; return false; }
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    size_t total=LittleFS.totalBytes(), used=LittleFS.usedBytes();
    if (total <= used || total-used < PHOTO_STORAGE_RESERVE_BYTES) {
        _upload_error=PhotoUploadError::NO_SPACE; _give(); return false;
    }
    _upload_locked = true;
    _upload_target = path;
    _upload_bytes = 0;
    _upload_limit = _upload_target.indexOf("_a.") >= 0
        ? PHOTO_DRIVER_MAX_BYTES : PHOTO_NAV_MAX_BYTES;
    LittleFS.remove("/.upload.tmp");
    _upload = LittleFS.open("/.upload.tmp", "w");
    if (!_upload) { _upload_error=PhotoUploadError::FILESYSTEM; _upload_locked=false; _give(); return false; }
    _give();
    return true;
}

bool PhotoStore::writeUpload(const uint8_t* data, size_t len) {
    if (!_upload_locked || !_upload) return false;
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    if (_upload_bytes + len > _upload_limit) {
        _upload_error=PhotoUploadError::TOO_LARGE; _give(); return false;
    }
    size_t total=LittleFS.totalBytes(), used=LittleFS.usedBytes();
    if (total <= used || len + PHOTO_STORAGE_RESERVE_BYTES > total-used) {
        _upload_error=PhotoUploadError::NO_SPACE; _give(); return false;
    }
    if (_upload.write(data,len) != len) {
        _upload_error=PhotoUploadError::WRITE_FAILED; _give(); return false;
    }
    _upload_bytes += len;
    _give();
    return true;
}

bool PhotoStore::endUpload(bool keep) {
    if (!_upload_locked) return false;
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    if (_upload) { _upload.flush(); _upload.close(); }
    bool valid = keep && _upload_error == PhotoUploadError::NONE &&
                 _upload_bytes >= PHOTO_MIN_JPEG_BYTES &&
                 _validJpegUnlocked("/.upload.tmp");
    if (keep && !valid && _upload_error == PhotoUploadError::NONE)
        _upload_error=PhotoUploadError::INVALID_IMAGE;
    bool committed=false;
    if (valid) {
        LittleFS.remove(_upload_target.c_str());
        committed=LittleFS.rename("/.upload.tmp",_upload_target.c_str());
        if (!committed) _upload_error=PhotoUploadError::COMMIT_FAILED;
    }
    if (!committed) LittleFS.remove("/.upload.tmp");
    _upload_locked = false;
    _upload_target = "";
    _upload_bytes = 0;
    _give();
    return committed;
}

bool PhotoStore::commitSlotUpload(uint8_t slot) {
    _upload_error = PhotoUploadError::NONE;
    if (!ready()) { _upload_error=PhotoUploadError::FILESYSTEM; return false; }
    if (slot >= PHOTO_MAX_SLOTS) { _upload_error=PhotoUploadError::INVALID_SLOT; return false; }
    if (_upload_locked || !_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    char a[20], b[20], sa[20], sb[20], ba[20], bb[20];
    photoPath(a,sizeof(a),slot,false); photoPath(b,sizeof(b),slot,true);
    photoStagePath(sa,sizeof(sa),slot,false); photoStagePath(sb,sizeof(sb),slot,true);
    snprintf(ba,sizeof(ba),"/.p%02u_a.bak",slot);
    snprintf(bb,sizeof(bb),"/.p%02u_b.bak",slot);
    bool ok=_commitPairUnlocked(sa,sb,a,b,ba,bb);
    _give();
    return ok;
}

bool PhotoStore::abortSlotUpload(uint8_t slot) {
    if (!_ready || slot >= PHOTO_MAX_SLOTS || _upload_locked || !_take()) return false;
    char path[20], target_a[20], target_b[20], backup_a[20], backup_b[20];
    photoStagePath(path,sizeof(path),slot,false); bool a=!LittleFS.exists(path)||LittleFS.remove(path);
    photoStagePath(path,sizeof(path),slot,true);  bool b=!LittleFS.exists(path)||LittleFS.remove(path);
    photoPath(target_a,sizeof(target_a),slot,false);photoPath(target_b,sizeof(target_b),slot,true);
    snprintf(backup_a,sizeof(backup_a),"/.p%02u_a.bak",(unsigned)slot);
    snprintf(backup_b,sizeof(backup_b),"/.p%02u_b.bak",(unsigned)slot);
    bool recovered=_recoverPairUnlocked(target_a,target_b,backup_a,backup_b);
    if(!recovered)_degraded=true;
    _give();
    return a&&b&&recovered;
}

bool PhotoStore::commitBootUpload() {
    _upload_error = PhotoUploadError::NONE;
    if (!ready()) { _upload_error=PhotoUploadError::FILESYSTEM; return false; }
    if (_upload_locked || !_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    bool ok=_commitPairUnlocked(bootStagePath(false),bootStagePath(true),
                                bootPath(false),bootPath(true),
                                "/.boot_a.bak","/.boot_b.bak");
    _give();
    return ok;
}

bool PhotoStore::abortBootUpload() {
    if (!_ready || _upload_locked || !_take()) return false;
    bool a=!LittleFS.exists(bootStagePath(false))||LittleFS.remove(bootStagePath(false));
    bool b=!LittleFS.exists(bootStagePath(true))||LittleFS.remove(bootStagePath(true));
    bool recovered=_recoverPairUnlocked(bootPath(false),bootPath(true),
                                        "/.boot_a.bak","/.boot_b.bak");
    if(!recovered)_degraded=true;
    _give();
    return a&&b&&recovered;
}

bool PhotoStore::removeSlot(uint8_t slot) {
    if (!_ready || slot >= PHOTO_MAX_SLOTS || !_take()) return false;
    char path[20], backup[20];
    photoPath(path, sizeof(path), slot, false); bool a = LittleFS.remove(path);
    photoPath(path, sizeof(path), slot, true);  bool b = LittleFS.remove(path);
    photoStagePath(path,sizeof(path),slot,false); LittleFS.remove(path);
    photoStagePath(path,sizeof(path),slot,true); LittleFS.remove(path);
    snprintf(backup,sizeof(backup),"/.p%02u_a.bak",slot); LittleFS.remove(backup);
    snprintf(backup,sizeof(backup),"/.p%02u_b.bak",slot); LittleFS.remove(backup);
    _give();
    return a || b;
}

bool PhotoStore::removeBoot() {
    if (!_ready || !_take()) return false;
    bool a=!LittleFS.exists(bootPath(false)) || LittleFS.remove(bootPath(false));
    bool b=!LittleFS.exists(bootPath(true)) || LittleFS.remove(bootPath(true));
    LittleFS.remove(bootStagePath(false)); LittleFS.remove(bootStagePath(true));
    LittleFS.remove("/.boot_a.bak"); LittleFS.remove("/.boot_b.bak");
    _give(); return a && b;
}

size_t PhotoStore::freeBytes() {
    if (!_ready || !_take()) return 0;
    size_t total=LittleFS.totalBytes(), used=LittleFS.usedBytes();
    _give(); return total>used?total-used:0;
}

size_t PhotoStore::usedBytes() {
    if (!_ready || !_take()) return 0; size_t n = LittleFS.usedBytes(); _give(); return n;
}
size_t PhotoStore::totalBytes() {
    if (!_ready || !_take()) return 0; size_t n = LittleFS.totalBytes(); _give(); return n;
}

const char* PhotoStore::uploadErrorName(PhotoUploadError error) {
    switch(error) {
        case PhotoUploadError::INVALID_SLOT:  return "invalid_slot";
        case PhotoUploadError::INVALID_TRANSACTION: return "invalid_transaction";
        case PhotoUploadError::BUSY:          return "upload_busy";
        case PhotoUploadError::FILESYSTEM:    return "filesystem_unavailable";
        case PhotoUploadError::NO_SPACE:      return "no_space";
        case PhotoUploadError::TOO_LARGE:     return "image_too_large";
        case PhotoUploadError::INVALID_IMAGE: return "invalid_image";
        case PhotoUploadError::WRITE_FAILED:  return "write_failed";
        case PhotoUploadError::COMMIT_FAILED: return "commit_failed";
        case PhotoUploadError::INTERRUPTED:   return "interrupted";
        default:                              return "none";
    }
}
#endif
