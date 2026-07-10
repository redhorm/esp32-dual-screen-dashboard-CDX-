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

// DISPLAY B  ST7789   240×240  SPI3_HOST (HSPI)
#define DISP_B_MOSI   13   // HSPI MOSI
#define DISP_B_SCLK   14   // HSPI SCLK
#define DISP_B_CS     15   // Chip Select
#define DISP_B_DC     16   // Data / Command
#define DISP_B_RST    17   // Reset  (or -1)
#define DISP_B_BL     18   // Backlight; set -1 to tie BL → 3.3 V

// ----------------------------------------------------------------
// ST7735  160×80  offset / inversion tuning
// Most 160×80 IPS blue-PCB modules: col=26 row=1 invert=false BGR
// If image is wrong color → flip DISP_A_INVERT
// If image is shifted     → adjust COL/ROW offsets
// ----------------------------------------------------------------
#define DISP_A_COL_OFFSET   26
#define DISP_A_ROW_OFFSET    1
#define DISP_A_INVERT     true    // IPS 0.96" modules need inversion ON
#define DISP_A_RGB_ORDER  false   // false = BGR (default for ST7735)
#define DISP_A_ROTATION      1    // 1/3 = landscape 160×80 (module native is portrait 80×160)

// ----------------------------------------------------------------
// ST7789  240×240
// ----------------------------------------------------------------
#define DISP_B_INVERT      true   // almost all ST7789 modules need this
#define DISP_B_RGB_ORDER  false
#define DISP_B_ROTATION      0    // 0-3; adjust if display is upside-down

// ----------------------------------------------------------------
// SPI clock speeds
// ----------------------------------------------------------------
#define DISP_A_SPI_HZ   40000000UL   // 40 MHz  (ST7735S max ~60 MHz)
#define DISP_B_SPI_HZ   80000000UL   // 80 MHz  (ST7789 supports 80 MHz)

// ----------------------------------------------------------------
// Frame-rate targets
// ----------------------------------------------------------------
#define DASH_FRAME_MS   33    // ~30 FPS  dashboard
#define NAV_FRAME_MS    80    // ~12 FPS  navigator

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
#define CONFIG_AP_SSID        "ULTRA4-DASH"
#define CONFIG_AP_PASSWORD    "ultra4setup"  // min 8 characters
#define CONFIG_AP_CHANNEL      6
#define CONFIG_AP_MAX_CLIENTS  2

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
// Boot sequence — dash: logo → sweep RPM → check; nav parte sfasato
#define BOOT_LOGO_MS         900
#define BOOT_SWEEP_MS       1300
#define BOOT_CHECK_MS        800
#define BOOT_NAV_DELAY_MS    800
#define BOOT_NAV_ACQ_MS     2400
#define BOOT_NAV_LOCK_MS     600
#define BOOT_TOTAL_MS       3800   // ≥ fine di entrambe le timeline

// Shift light / redline
#define REDLINE_RPM        6900.0f  // flash = rev limiter (7200)
#define SHIFT_FLASH_MS       110   // semiperiodo lampeggio bordo/barra
#define GEAR_SNAP_MS         100   // durata snap del numero marcia
#define GEAR_SNAP_SCALE      1.3f  // scala iniziale dello snap (130%)

// Waypoint alert
#define ALERT_DIST_MI        0.20f // distanza di attivazione dal WP danger
#define ALERT_PULSE_MS       250   // semiperiodo pulsazione riga/footer

// Finish line
#define FINISH_HOLD_MS      4000   // hold schermata risultato
#define SAT_LOCK_COUNT        12   // satelliti a lock avvenuto

// ----------------------------------------------------------------
// Wow pass — smoothing, micro-vita, transizioni, compass tape
// ----------------------------------------------------------------
// Easing dei valori visualizzati (tau in ms; attack/release separati)
#define SPEED_SMOOTH_MS      120.0f  // inseguimento velocità dash
#define RPM_ATTACK_MS         60.0f  // barra RPM: salita scattante...
#define RPM_RELEASE_MS       400.0f  // ...discesa lenta (VU meter)
#define NAV_VALUE_SMOOTH_MS  150.0f  // valori navigator (12 FPS)
#define SCROLL_MS            260     // slide scroll roadbook

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
#define VIB_MAX_PX             1     // tremolio barre (solo LIVE)
#define VIB_PERIOD_MS         45

// Transizioni di stato
#define LIVE_WIPE_MS         450     // wipe BOOT→LIVE
#define NAV_WIPE_LAG_MS      120     // il navigator segue la dash
#define FINISH_SHUTDOWN_MS   700     // spegnimento sequenziale strumenti
#define ALERT_IN_MS          180     // slide-in banner alert
#define ALERT_OUT_MS         250     // fade-out banner alert

// Compass tape (hero detail)
#define TAPE_PX_PER_DEG        0.8f  // scala nastro: 72 px ≈ ±45° visibili

// ----------------------------------------------------------------
// Frame pacing / profiling
// ----------------------------------------------------------------
#define NAV_FRAME_SLOW_MS    120   // navigator rallentato durante FX dash
#define PROFILE_LOG_MS      5000   // report frame time su seriale
