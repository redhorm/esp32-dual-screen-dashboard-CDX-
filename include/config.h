#pragma once
// ================================================================
// config.h  — single point of modification for all hardware tuning
// ================================================================

// ----------------------------------------------------------------
// PIN TABLE
// ----------------------------------------------------------------
//  Signal          GPIO   Notes
//  ──────────────  ─────  ─────────────────────────────────────────
// DISPLAY A  ST7735S  160×80   SPI2_HOST (FSPI)
#define DISP_A_MOSI   11   // FSPI MOSI
#define DISP_A_SCLK   12   // FSPI SCLK
#define DISP_A_CS     10   // Chip Select
#define DISP_A_DC      9   // Data / Command
#define DISP_A_RST     8   // Reset  (or -1 to omit)
#define DISP_A_BL     46   // Backlight PWM; set -1 to tie BL → 3.3 V

// DISPLAY B  copilot ST7789V 240×135 landscape.
// Default profile is ST7789V native 135×240, landscape 240×135.
// Module silk: GND VCC SCL SDA RES DC CS BLK.
#define DISP_B_MOSI   13   // HSPI MOSI
#define DISP_B_SCLK   14   // HSPI SCLK
#define DISP_B_CS     15   // Chip Select
#define DISP_B_DC     16   // Data / Command
#define DISP_B_RST    17   // Reset  (or -1)
#define DISP_B_BL     18   // Backlight; set -1 to tie BL → 3.3 V

// REAR LIGHT  WS2812/NeoPixel — separate regulated 5 V supply required.
// GPIO is data only; never power the strip from an ESP32 pin.
#define RTL_WS2812_PIN          4
#define RTL_LED_COUNT          16
#define RTL_LED_BRIGHTNESS     96
#define RTL_COMMAND_TIMEOUT_MS 1500UL

// ESC receiver signal capture. Enabled by default; install the
// conditioned input first. A vehicle/receiver signal above 3.6 V must never
// reach the ESP32.
#define ESC_PWM_SIGNAL_PIN      5
#define ESC_PWM_INPUT_ENABLED   1
#define ESC_PWM_MIN_US        800U
#define ESC_PWM_MAX_US       2200U
#define ESC_PWM_TIMEOUT_US 100000UL
#define ESC_PWM_REVERSE_US    1000U
#define ESC_PWM_NEUTRAL_US    1500U
#define ESC_PWM_FORWARD_US    2000U
#define ESC_PWM_ENTER         0.12f
#define ESC_PWM_EXIT          0.06f
#define ESC_PWM_SMOOTH_MS       55U
#define ESC_PWM_MAX_SLEW         8.0f
#define ESC_NEUTRAL_DWELL_MS   180U
#define ESC_REVERSE_CONFIRM_MS 220U
#define ESC_REVERSE_ARM_MS    1600U

// ----------------------------------------------------------------
// ST7735  160×80  offset / inversion tuning
// Existing 160×80 IPS blue-PCB module: col=26 row=1 invert=true BGR
// If image is wrong color → flip DISP_A_INVERT
// If image is shifted     → adjust COL/ROW offsets
// ----------------------------------------------------------------
#define DISP_A_COL_OFFSET   26
#define DISP_A_ROW_OFFSET    1
#define DISP_A_INVERT     true    // IPS 0.96" modules need inversion ON
#define DISP_A_RGB_ORDER  false   // false = BGR (default for ST7735)
#define DISP_A_ROTATION      1    // 1/3 = landscape 160×80 (module native is portrait 80×160)

// ----------------------------------------------------------------
// Copilot geometry and offsets have a single source of truth:
// hal/copilot_display_profile.h (ST7789V 135×240, landscape 240×135).
// The constants below describe media sizes used by the supplied panel.
// ----------------------------------------------------------------
#define COPILOT_SCREEN_WIDTH       240
#define COPILOT_SCREEN_HEIGHT      135

// ----------------------------------------------------------------
// SPI clock speeds
// ----------------------------------------------------------------
#define DISP_A_SPI_HZ   40000000UL   // 40 MHz  (ST7735S max ~60 MHz)
#define DISP_B_SPI_HZ   80000000UL   // 80 MHz  (ST7789 supports 80 MHz)

// ----------------------------------------------------------------
// Frame-rate targets
// ----------------------------------------------------------------
#define DASH_FRAME_MS   33    // ~30 FPS, valore collaudato sul display pilota
#define NAV_FRAME_MS    66    // ~15 FPS; SHOW synchronizes both panels at DASH_FRAME_MS

// ----------------------------------------------------------------
// Unit preferences
// ----------------------------------------------------------------
#define USE_KMH     false   // false = MPH, true = km/h
#define USE_CELSIUS true    // false = °F,  true = °C (stile telemetria RC)

// ----------------------------------------------------------------
// Wi-Fi AP configuration portal
// Connect to this network and open http://192.168.4.1.  The captive
// DNS also redirects common portal probes to the settings page.
// ----------------------------------------------------------------
#define CONFIG_AP_SSID_PREFIX          "stormDASH"
#define CONFIG_AP_CHANNEL               6
#define CONFIG_AP_MAX_CLIENTS           2
#define CONFIG_AP_REVEAL_PIN             0  // Hold BOOT while running to reveal

// ----------------------------------------------------------------
// Simulator limits
// ----------------------------------------------------------------
#define SIM_MAX_SPEED_MPH  110.0f
#define SIM_MAX_RPM       7200.0f
#define SIM_IDLE_RPM       800.0f
#define SIM_NUM_GEARS        5
#define SIM_SHIFT_COOLDOWN_MS 450   // tempo minimo tra due cambiate

// ----------------------------------------------------------------
// Demo driver (autopilot roadbook-aware)
// DEMO_MODE 1 = guida lo stage frenando prima dei waypoint;
// DEMO_MODE 0 = vecchio ciclo throttle/brake fisso a 8 s
// ----------------------------------------------------------------
#define DEMO_MODE               1
#define DRV_CRUISE_MPH        70.0f   // velocità sul dritto
#define DRV_TGT_HAIRPIN_MPH   12.0f   // target ingresso curva per tipo WP
#define DRV_TGT_90_MPH        20.0f
#define DRV_TGT_WATER_MPH     15.0f
#define DRV_TGT_45_MPH        45.0f
#define DRV_BRAKE_DECEL       12.0f   // mph/s usati per pianificare la staccata
#define DRV_BRAKE_MARGIN       1.4f   // frena con questo margine di sicurezza
#define DRV_CORNER_HOLD_MI    0.03f   // tiene la velocità-curva così vicino al WP
#define DRV_SPEED_DEADBAND     2.0f   // mph di tolleranza sul target
#define DRV_BRAKE_GAIN_MPH    12.0f   // mph oltre target → freno pieno
#define DRV_THROTTLE_GAIN_MPH 25.0f   // mph sotto target → gas pieno
#define DRV_MAX_THROTTLE      0.95f

// ----------------------------------------------------------------
// Animation & FX (tutte le durate in ms)
// ----------------------------------------------------------------
// Boot animation; both screens have one rendering owner
#define BOOT_TOTAL_MS       1600   // animation time after initialization; no forced blocking delay

// Shift light / redline
#define REDLINE_RPM        6900.0f  // flash = rev limiter (7200)
#define GEAR_SNAP_MS         100   // durata snap del numero marcia
#define GEAR_SNAP_SCALE      1.3f  // scala iniziale dello snap (130%)

// Waypoint alert
#define ALERT_DIST_MI        0.20f // distanza di attivazione dal WP danger

// Finish line
#define FINISH_HOLD_MS      4000   // hold schermata risultato

// ----------------------------------------------------------------
// Simulation dynamics
// ----------------------------------------------------------------
// Micro-vita (rumore deterministico, seedabile)
#define SIM_NOISE_SEED    0xC0FFEEu
#define SIM_RPM_JITTER        40.0f  // ±rpm a regime
#define SIM_RPM_JITTER_MS    120     // periodo del noise RPM
#define SIM_BAT_SAG_V          0.30f // flessione tensione a pieno carico
#define SIM_BAT_SAG_UP_MS    400.0f  // tau insorgenza sag
#define SIM_BAT_SAG_DN_MS   4000.0f  // tau recupero (lento)
#define SIM_TEMP_HEAT_RATE     0.45f // 1/s: scalda in fretta...
#define SIM_TEMP_COOL_RATE     0.05f // ...raffredda piano
#define SIM_ESC_HEAT_RATE      0.70f // ESC: massa termica piccola, spike rapidi

// ----------------------------------------------------------------
// Frame pacing / profiling
// ----------------------------------------------------------------
#define PROFILE_LOG_MS      5000   // report frame time su seriale
