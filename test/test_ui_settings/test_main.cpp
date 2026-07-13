#include <unity.h>
#include <type_traits>
#include "core/ui_settings.h"
#include "core/ui_i18n.h"

void setUp() {}
void tearDown() {}

static void test_theme_ids_remain_backward_compatible() {
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(UiTheme::CLASSIC_RALLY));
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(UiTheme::REALISM));
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(UiTheme::STOCK_OFFROAD));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiTheme::STOCK_OFFROAD),
                          static_cast<int>(sanitizeTheme(2)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiTheme::CLASSIC_RALLY),
                          static_cast<int>(sanitizeTheme(255)));
    TEST_ASSERT_EQUAL_STRING("Stock Offroad",
                             themeName(UiTheme::STOCK_OFFROAD));
}

static void test_language_sanitization_and_names() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiLanguage::ITALIAN),
                          static_cast<int>(sanitizeLanguage(0)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiLanguage::ENGLISH),
                          static_cast<int>(sanitizeLanguage(1)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiLanguage::ITALIAN),
                          static_cast<int>(sanitizeLanguage(8)));
    TEST_ASSERT_EQUAL_STRING("Italiano", languageName(UiLanguage::ITALIAN));
    TEST_ASSERT_EQUAL_STRING("English", languageName(UiLanguage::ENGLISH));
}

static void test_display_translation_is_ascii_and_deterministic() {
    TEST_ASSERT_EQUAL_STRING("PRONTO",
        uiText(UiLanguage::ITALIAN, "PRONTO", "READY"));
    TEST_ASSERT_EQUAL_STRING("READY",
        uiText(UiLanguage::ENGLISH, "PRONTO", "READY"));
    TEST_ASSERT_EQUAL_STRING("GUADO",
        localizedRoadbookText(UiLanguage::ITALIAN, "WATER XING"));
    TEST_ASSERT_EQUAL_STRING("WATER XING",
        localizedRoadbookText(UiLanguage::ENGLISH, "WATER XING"));
    TEST_ASSERT_EQUAL_STRING("R 45",
        localizedRoadbookText(UiLanguage::ITALIAN, "R 45"));

    struct Translation { const char* english; const char* italian; };
    const Translation fixed_labels[] = {
        {"START", "PARTENZA"}, {"LINE", "LINEA"},
        {"CAUTION !!", "ATTENZIONE"}, {"WATER XING", "GUADO"},
        {"2FT DEEP", "60CM FONDO"}, {"FAST", "VELOCE"},
        {"L HAIRPIN", "TORNANTE S"}, {"SLOW !!", "PIANO !!"},
        {"STAY RIGHT", "TIENI DESTRA"}, {"FINISH", "ARRIVO"},
    };
    for (const Translation& label : fixed_labels) {
        TEST_ASSERT_EQUAL_STRING(label.italian,
            localizedRoadbookText(UiLanguage::ITALIAN, label.english));
        TEST_ASSERT_EQUAL_STRING(label.english,
            localizedRoadbookText(UiLanguage::ENGLISH, label.english));
    }

    char heading[8];
    localizedHeadingFull(UiLanguage::ITALIAN, heading, 315.0f);
    TEST_ASSERT_EQUAL_STRING("NO 315", heading);
    localizedHeadingFull(UiLanguage::ENGLISH, heading, 315.0f);
    TEST_ASSERT_EQUAL_STRING("NW 315", heading);
}

static void test_ui_settings_stays_shared_frame_safe() {
    TEST_ASSERT_TRUE(std::is_trivially_copyable<UiSettings>::value);
    UiSettings settings;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UiLanguage::ITALIAN),
                          static_cast<int>(settings.language));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_theme_ids_remain_backward_compatible);
    RUN_TEST(test_language_sanitization_and_names);
    RUN_TEST(test_display_translation_is_ascii_and_deterministic);
    RUN_TEST(test_ui_settings_stays_shared_frame_safe);
    return UNITY_END();
}
