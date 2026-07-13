#pragma once

#include <string.h>
#include "ui_settings.h"
#include "formatter.h"

// Tiny, allocation-free display localization.  Strings deliberately stay in
// ASCII because the built-in TFT fonts do not guarantee accented glyphs.
inline const char* uiText(UiLanguage language, const char* italian,
                          const char* english) {
    return language == UiLanguage::ENGLISH ? english : italian;
}

// The roadbook data is shared with the simulator.  Translate its few fixed
// demo labels at render time so the core route and saved files remain stable.
inline const char* localizedRoadbookText(UiLanguage language,
                                         const char* text) {
    if (!text || language == UiLanguage::ENGLISH) return text ? text : "";
    struct Pair { const char* english; const char* italian; };
    static constexpr Pair pairs[] = {
        {"START", "PARTENZA"}, {"LINE", "LINEA"},
        {"CAUTION !!", "ATTENZIONE"}, {"WATER XING", "GUADO"},
        {"2FT DEEP", "60CM FONDO"}, {"FAST", "VELOCE"},
        {"L HAIRPIN", "TORNANTE S"}, {"SLOW !!", "PIANO !!"},
        {"STAY RIGHT", "TIENI DESTRA"}, {"FINISH", "ARRIVO"},
    };
    for (const Pair& pair : pairs)
        if (strcmp(text, pair.english) == 0) return pair.italian;
    return text;
}

// Italian compass notation uses Ovest (O) instead of West (W).  Keeping the
// replacement the same length avoids allocations and remains safe for the
// small fixed display buffers.
inline void localizedHeadingFull(UiLanguage language, char* out, float degrees) {
    Fmt::headingFull(out, degrees);
    if (language != UiLanguage::ITALIAN) return;
    for (char* p = out; *p; ++p)
        if (*p == 'W') *p = 'O';
}
