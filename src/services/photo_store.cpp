#ifndef NATIVE_BUILD
#include <LittleFS.h>
#include <esp_partition.h>
#include "photo_store.h"

PhotoStore photoStore;

namespace {
const esp_partition_t* findPhotoPartition(const char* label) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                    label);
}

bool mountPhotoPartition(const char* label) {
    return LittleFS.begin(false, PHOTO_FS_BASE_PATH,
                          PHOTO_FS_MAX_OPEN_FILES, label);
}
}

bool PhotoStore::begin() {
    _ready = false;
    _degraded = false;
    _formatted_on_begin = false;
    _recovered_on_begin = false;
    _partition_label[0] = '\0';
    _upload_error = PhotoUploadError::NONE;

    if (!_mutex) _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        _upload_error = PhotoUploadError::FILESYSTEM;
        Serial.println("[photos] fs init failed: mutex unavailable");
        return false;
    }

    const bool canonical_exists = findPhotoPartition(PHOTO_FS_PARTITION_LABEL) != nullptr;
    const bool legacy_exists = findPhotoPartition(PHOTO_FS_LEGACY_PARTITION_LABEL) != nullptr;
    const char* mounted_label = nullptr;

    // Arduino-ESP32 2.x defaults LittleFS.begin() to partition label "spiffs".
    // This project deliberately uses the clearer "littlefs" label, so always
    // pass it explicitly.  The fallback keeps installations made with an older
    // default partition table readable instead of formatting them.
    if (canonical_exists && mountPhotoPartition(PHOTO_FS_PARTITION_LABEL)) {
        mounted_label = PHOTO_FS_PARTITION_LABEL;
    } else if (legacy_exists &&
               mountPhotoPartition(PHOTO_FS_LEGACY_PARTITION_LABEL)) {
        mounted_label = PHOTO_FS_LEGACY_PARTITION_LABEL;
        Serial.println("[photos] fs mounted legacy partition label 'spiffs'");
    }

    if (!mounted_label) {
        const char* recovery_label = canonical_exists
            ? PHOTO_FS_PARTITION_LABEL
            : (legacy_exists ? PHOTO_FS_LEGACY_PARTITION_LABEL : nullptr);
        if (!recovery_label) {
            _upload_error = PhotoUploadError::FILESYSTEM;
            Serial.println("[photos] fs init failed: LittleFS partition not found");
            return false;
        }

        // Retry once before formatting.  Formatting is only attempted on a
        // partition proven to exist, which avoids erasing unrelated storage
        // when a partition-table label is wrong.
        if (mountPhotoPartition(recovery_label)) {
            mounted_label = recovery_label;
        } else {
            Serial.printf("[photos] fs mount failed on '%s'; formatting partition\n",
                          recovery_label);
            // The failed begin above selected recovery_label inside LittleFS,
            // so format() targets that exact partition.
            if (LittleFS.format() && mountPhotoPartition(recovery_label)) {
                mounted_label = recovery_label;
                _formatted_on_begin = true;
            }
        }
    }

    if (!mounted_label) {
        _upload_error = PhotoUploadError::FILESYSTEM;
        Serial.println("[photos] fs recovery failed: storage remains unavailable");
        return false;
    }

    snprintf(_partition_label, sizeof(_partition_label), "%s", mounted_label);
    _ready = true;
    bool healthy = false;
    if (_take()) { healthy=_scrubUnlocked(); _give(); }
    _degraded = !healthy;
    if (!healthy) _upload_error = PhotoUploadError::FILESYSTEM;

    const size_t total = LittleFS.totalBytes();
    const size_t used = LittleFS.usedBytes();
    Serial.printf("[photos] fs %s label='%s' total=%lu used=%lu formatted=%u recovered=%u\n",
                  ready() ? "ready" : "degraded", _partition_label,
                  static_cast<unsigned long>(total),
                  static_cast<unsigned long>(used),
                  _formatted_on_begin ? 1U : 0U,
                  _recovered_on_begin ? 1U : 0U);
    return ready();
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
    if (!file || file.size() < PHOTO_MIN_JPEG_BYTES) {
        if (file) file.close();
        return false;
    }
    const size_t file_size = file.size();
    uint8_t head[2] = {0,0}, tail[2] = {0,0};
    bool ok = file.read(head,2) == 2 && head[0] == 0xFF && head[1] == 0xD8 &&
              file.seek(file_size-2) && file.read(tail,2) == 2 &&
              tail[0] == 0xFF && tail[1] == 0xD9 && file.seek(2);

    // Validate the JPEG segment structure and require a real frame header.
    // Checking only SOI/EOI allowed arbitrary/corrupt payloads to be committed.
    bool found_frame = false;
    bool found_scan = false;
    while (ok && file.position() + 4 < file_size) {
        int prefix = file.read();
        if (prefix < 0) { ok = false; break; }
        if (prefix != 0xFF) continue;

        int marker = file.read();
        while (marker == 0xFF) marker = file.read();
        if (marker < 0) { ok = false; break; }
        if (marker == 0xD9) break;
        if (marker == 0xDA) { found_scan = true; break; }
        if (marker == 0x00 || marker == 0xD8 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) continue;

        const int hi = file.read();
        const int lo = file.read();
        if (hi < 0 || lo < 0) { ok = false; break; }
        const size_t segment_len = (static_cast<size_t>(hi) << 8) |
                                   static_cast<size_t>(lo);
        if (segment_len < 2 || file.position() + segment_len - 2 > file_size) {
            ok = false;
            break;
        }

        const bool is_frame = marker >= 0xC0 && marker <= 0xCF &&
                              marker != 0xC4 && marker != 0xC8 &&
                              marker != 0xCC;
        if (is_frame) {
            if (segment_len < 11) { ok = false; break; }
            const int precision = file.read();
            const int height_hi = file.read();
            const int height_lo = file.read();
            const int width_hi = file.read();
            const int width_lo = file.read();
            const int components = file.read();
            if (precision < 0 || height_hi < 0 || height_lo < 0 ||
                width_hi < 0 || width_lo < 0 || components < 1 || components > 4 ||
                segment_len != static_cast<size_t>(8 + 3 * components)) {
                ok = false;
                break;
            }
            const uint16_t height = static_cast<uint16_t>((height_hi << 8) | height_lo);
            const uint16_t width = static_cast<uint16_t>((width_hi << 8) | width_lo);
            if (!width || !height || width > 8192 || height > 8192) {
                ok = false;
                break;
            }
            found_frame = true;
        }
        if (!file.seek(file.position() + segment_len - 2 - (is_frame ? 6 : 0))) {
            ok = false;
            break;
        }
    }
    file.close();
    return ok && found_frame && found_scan;
}

bool PhotoStore::_validUploadTarget(const char* path) const {
    if (!path || !path[0]) return false;
    if (strcmp(path, bootStagePath(false)) == 0 ||
        strcmp(path, bootStagePath(true)) == 0) return true;
    char allowed[20];
    for (uint8_t slot = 0; slot < PHOTO_MAX_SLOTS; ++slot) {
        photoStagePath(allowed, sizeof(allowed), slot, false);
        if (strcmp(path, allowed) == 0) return true;
        photoStagePath(allowed, sizeof(allowed), slot, true);
        if (strcmp(path, allowed) == 0) return true;
    }
    return false;
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

    // A successful rename is not enough: re-open both final files before
    // acknowledging the transaction so a refresh/reboot can only expose a
    // complete, decodable pair.
    if (!_validJpegUnlocked(target_a) || !_validJpegUnlocked(target_b)) {
        LittleFS.remove(target_a);
        LittleFS.remove(target_b);
        if (had_pair &&
            !_recoverPairUnlocked(target_a,target_b,backup_a,backup_b)) {
            _degraded = true;
        }
        _upload_error = PhotoUploadError::COMMIT_FAILED;
        return false;
    }
    LittleFS.remove(backup_a);
    LittleFS.remove(backup_b);
    return true;
}

bool PhotoStore::_scrubUnlocked() {
    bool healthy = true;
    bool recovery_needed = LittleFS.exists("/.upload.tmp");
    LittleFS.remove("/.upload.tmp");
    const char* boot_a = bootPath(false);
    const char* boot_b = bootPath(true);
    const bool boot_a_exists = LittleFS.exists(boot_a);
    const bool boot_b_exists = LittleFS.exists(boot_b);
    const bool boot_a_valid = _validJpegUnlocked(boot_a);
    const bool boot_b_valid = _validJpegUnlocked(boot_b);
    recovery_needed = recovery_needed || LittleFS.exists(bootStagePath(false)) ||
                      LittleFS.exists(bootStagePath(true)) ||
                      LittleFS.exists("/.boot_a.bak") ||
                      LittleFS.exists("/.boot_b.bak") ||
                      boot_a_valid != boot_b_valid ||
                      (boot_a_exists && !boot_a_valid) ||
                      (boot_b_exists && !boot_b_valid);
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
        const bool a_exists = LittleFS.exists(a);
        const bool b_exists = LittleFS.exists(b);
        const bool a_valid = _validJpegUnlocked(a);
        const bool b_valid = _validJpegUnlocked(b);
        recovery_needed = recovery_needed || LittleFS.exists(sa) || LittleFS.exists(sb) ||
                          LittleFS.exists(ba) || LittleFS.exists(bb) ||
                          a_valid != b_valid || (a_exists && !a_valid) ||
                          (b_exists && !b_valid);
        LittleFS.remove(sa); LittleFS.remove(sb);
        recovered=_recoverPairUnlocked(a,b,ba,bb);
        if(!recovered)recovered=_recoverPairUnlocked(a,b,ba,bb);
        healthy=healthy&&recovered;
    }
    _recovered_on_begin = recovery_needed;
    if (recovery_needed)
        Serial.printf("[photos] fs recovery pass %s\n", healthy ? "completed" : "failed");
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
    if (!_validUploadTarget(path)) {
        _upload_error = PhotoUploadError::INVALID_TRANSACTION;
        Serial.println("[photos] upload rejected: invalid internal target");
        return false;
    }
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    size_t total=LittleFS.totalBytes(), used=LittleFS.usedBytes();
    if (total <= used || total-used < PHOTO_STORAGE_RESERVE_BYTES) {
        _upload_error=PhotoUploadError::NO_SPACE;
        Serial.printf("[photos] upload rejected: no space total=%lu used=%lu\n",
                      static_cast<unsigned long>(total),
                      static_cast<unsigned long>(used));
        _give();
        return false;
    }
    _upload_locked = true;
    _upload_target = path;
    _upload_bytes = 0;
    _upload_free_at_start = total - used;
    _upload_limit = _upload_target.indexOf("_a.") >= 0
        ? PHOTO_DRIVER_MAX_BYTES : PHOTO_NAV_MAX_BYTES;
    LittleFS.remove("/.upload.tmp");
    _upload = LittleFS.open("/.upload.tmp", "w");
    if (!_upload) {
        _upload_error=PhotoUploadError::FILESYSTEM;
        _upload_locked=false;
        _upload_free_at_start=0;
        Serial.println("[photos] upload failed: temporary file could not be opened");
        _give();
        return false;
    }
    Serial.printf("[photos] upload begin role=%s limit=%lu\n",
                  _upload_limit == PHOTO_DRIVER_MAX_BYTES ? "driver" : "navigator",
                  static_cast<unsigned long>(_upload_limit));
    _give();
    return true;
}

bool PhotoStore::writeUpload(const uint8_t* data, size_t len) {
    if (!_upload_locked || !_upload) {
        _upload_error = PhotoUploadError::INVALID_TRANSACTION;
        return false;
    }
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    if (len > _upload_limit - _upload_bytes) {
        _upload_error=PhotoUploadError::TOO_LARGE;
        Serial.printf("[photos] upload stopped: image exceeds %lu bytes\n",
                      static_cast<unsigned long>(_upload_limit));
        _give();
        return false;
    }
    if (_upload_bytes + len + PHOTO_STORAGE_RESERVE_BYTES > _upload_free_at_start) {
        _upload_error=PhotoUploadError::NO_SPACE;
        Serial.println("[photos] upload stopped: storage reserve would be crossed");
        _give();
        return false;
    }
    if (_upload.write(data,len) != len) {
        _upload_error=PhotoUploadError::WRITE_FAILED;
        Serial.println("[photos] upload stopped: LittleFS write failed");
        _give();
        return false;
    }
    _upload_bytes += len;
    _give();
    return true;
}

bool PhotoStore::endUpload(bool keep) {
    if (!_upload_locked) return false;
    if (!_take()) { _upload_error=PhotoUploadError::BUSY; return false; }
    const size_t received_bytes = _upload_bytes;
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
    if (!keep && _upload_error == PhotoUploadError::NONE)
        _upload_error = PhotoUploadError::INTERRUPTED;
    Serial.printf("[photos] upload %s bytes=%lu status=%s\n",
                  committed ? "staged" : "discarded",
                  static_cast<unsigned long>(received_bytes),
                  uploadErrorName(_upload_error));
    _upload_locked = false;
    _upload_target = "";
    _upload_bytes = 0;
    _upload_free_at_start = 0;
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
    Serial.printf("[photos] gallery commit slot=%u status=%s\n",
                  static_cast<unsigned>(slot),
                  ok ? "ok" : uploadErrorName(_upload_error));
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
    Serial.printf("[photos] boot commit status=%s\n",
                  ok ? "ok" : uploadErrorName(_upload_error));
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

const char* PhotoStore::storageStateName() const {
    if (!_ready) return "unavailable";
    if (_degraded) return "degraded";
    return "ready";
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
