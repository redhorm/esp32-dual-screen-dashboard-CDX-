#ifndef NATIVE_BUILD
#include <LittleFS.h>
#include "photo_store.h"

PhotoStore photoStore;

bool PhotoStore::begin() {
    _mutex = xSemaphoreCreateMutex();
    return _mutex && LittleFS.begin(true);
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

const char* PhotoStore::bootPath(bool navigator) {
    return navigator ? "/boot_b.jpg" : "/boot_a.jpg";
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

uint32_t PhotoStore::slotMask() {
    if (!_take()) return 0;
    uint32_t mask = _slotMaskUnlocked();
    _give();
    return mask;
}

uint8_t PhotoStore::count() {
    return __builtin_popcount(slotMask());
}

bool PhotoStore::hasBoot(bool navigator) {
    if (!_take()) return false;
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
    if (!_take()) return false;
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
    if (!_take()) return false;
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
    if (_upload_locked || !_take()) return false;
    _upload_locked = true;
    _upload = LittleFS.open(path, "w");
    if (!_upload) { _upload_locked = false; _give(); return false; }
    return true;
}

bool PhotoStore::writeUpload(const uint8_t* data, size_t len) {
    return _upload_locked && _upload && _upload.write(data, len) == len;
}

void PhotoStore::endUpload(bool keep) {
    if (!_upload_locked) return;
    String path = _upload.name();
    _upload.close();
    if (!keep && path.length()) LittleFS.remove(path);
    _upload_locked = false;
    _give();
}

bool PhotoStore::removeSlot(uint8_t slot) {
    if (slot >= PHOTO_MAX_SLOTS || !_take()) return false;
    char path[20];
    photoPath(path, sizeof(path), slot, false); bool a = LittleFS.remove(path);
    photoPath(path, sizeof(path), slot, true);  bool b = LittleFS.remove(path);
    _give();
    return a || b;
}

size_t PhotoStore::usedBytes() {
    if (!_take()) return 0; size_t n = LittleFS.usedBytes(); _give(); return n;
}
size_t PhotoStore::totalBytes() {
    if (!_take()) return 0; size_t n = LittleFS.totalBytes(); _give(); return n;
}
#endif
