# ESP32-S3 Dual Display — RC Ultra4 Dashboard + Waypoint Navigator

Due display SPI indipendenti pilotati in parallelo (DMA su due host SPI separati):

| Display | Pannello | Risoluzione | Ruolo | Host SPI | Target FPS |
|---|---|---|---|---|---|
| A | ST7735S 0.96" | 160×80 | **Dashboard** stile cluster Jeep off-road | `SPI2_HOST` (FSPI) | 30+ |
| B | ST7789 1.3" | 240×240 | **Waypoint Navigator** stile roadbook Ultra4 "WAY MAP" | `SPI3_HOST` (HSPI) | ~12 |

Un simulatore physics-based (C++ puro, testabile su host) genera velocità, RPM, marce,
batteria, temperatura e un odometro che fa avanzare i waypoint del roadbook demo (9 WP, ~12 mi, in loop).

## Demo loop & FX

La state machine globale (`core/race.{h,cpp}`) orchestra: **BOOT → LIVE → FINISH → restart**,
tutta a `millis()`, durate e soglie in `config.h` (sezione *Animation & FX*):

- **Boot cinematico** (~4 s): dash → wordmark ULTRA4 in fade, sweep RPM 0→max→0,
  self-check strumenti; navigator sfasato di 0.8 s → "GPS ACQUIRING" con anello
  satelliti che si accende, flash "SAT LOCK", poi tabella live.
- **Shift light**: oltre `REDLINE_RPM` bordo dash e zona rossa RPM lampeggiano
  rosso/bianco; a ogni cambio marcia il numero fa uno snap 130%→100% (`GEAR_SNAP_MS`).
- **Waypoint alert**: a `ALERT_DIST_MI` (0.2 mi) da un WP pericoloso la riga rossa
  del roadbook pulsa, il countdown nel footer diventa rosso e sulla dash appare il
  banner warning ("L HAIRPIN SLOW !!") — i due schermi si parlano.
- **Demo driver** (`DEMO_MODE 1`, `core/driver.cpp`): guida roadbook-aware con
  staccate calcolate sulla distanza di frenata, punte a ~70 mph sul dritto,
  hairpin a 12 mph. `DEMO_MODE 0` = vecchio ciclo throttle/brake fisso.
- **Finish**: navigator con scacchiera animata + tempo stage (mm:ss.d), dash con
  max speed / max RPM della run. Hold `FINISH_HOLD_MS` (4 s), poi riparte il giro.
  Loop infinito senza fermi immagine.

Frame time reale (render e push separati, avg/max per display) e heap libero
loggati su seriale ogni `PROFILE_LOG_MS` (5 s). Durante gli FX critici della dash
il navigator scala a `NAV_FRAME_SLOW_MS` per non rubare CPU. Frecce e dot del
roadbook sono anti-aliased (`drawWideLine`/`fillSmoothCircle`).

## Wow pass

- **Font smooth VLW** — Barlow Condensed Bold (OFL) subsettato in 3 taglie
  (44/20/12 px, ~32 KB flash totali) embedded come header C e renderizzato
  anti-aliased. Rigenerazione: `python3 tools/make_vlw.py` (richiede Pillow).
  Ogni scena ha la propria istanza `UiFonts`: il cursore del DataWrapper VLW è
  stato mutabile, condividerlo tra i due task (core diversi) sarebbe una race.
- **Easing ovunque** (`core/easing.h`): velocità con inseguimento smooth, barra
  RPM stile VU meter (attack `RPM_ATTACK_MS`, release `RPM_RELEASE_MS`), scroll
  del roadbook a slide (`SCROLL_MS`, clip sul corpo tabella), countdown footer
  interpolato per il refresh a 12 FPS.
- **Micro-vita** (`core/noise.h`, deterministico e seedabile): jitter RPM ±40 a
  regime, sag batteria transiente sotto pieno carico con recupero lento, inerzia
  termica asimmetrica (scalda in fretta, raffredda piano), micro-tremolio ±1 px
  delle barre con ampiezza ∝ RPM (solo LIVE).
- **Transizioni**: wipe orizzontale BOOT→LIVE coordinato tra i display (il nav
  segue di `NAV_WIPE_LAG_MS`), spegnimento sequenziale strumenti LIVE→FINISH,
  banner alert con slide-in ease-out e fade-out (mai un cut secco).
- **Hero detail**: nastro bussola tape-style aeronautico in cima alla dash
  (`core/compass.h`), tick ogni 15°, cardinali ogni 45°, marker centrale lime —
  in moto perpetuo con l'heading simulato.

## Portale Wi-Fi AP

Il controller crea automaticamente un access point locale, senza bisogno di
router o Internet:

| Parametro | Valore predefinito |
|---|---|
| SSID | `ULTRA4-DASH` |
| Password | `ultra4setup` |
| Indirizzo | `http://192.168.4.1` |

La rete e la password si cambiano in `include/config.h`. Il DNS captive reindirizza
al portale i normali probe di iOS, Android e Windows. Le modifiche sono applicate
in tempo reale e salvate in NVS (`Preferences`), quindi sopravvivono al riavvio.

Dal portale si regolano indipendentemente:

- tema grafico del display pilota e del display copilota;
- luminosità dei due pannelli;
- MPH/mi oppure km/h/km e °F oppure °C;
- immagine di boot e durata;
- modalità cornice digitale, display target, intervallo, ordine, transizione e overlay.

### Temi cockpit

La grafica originale resta invariata come **Classic Rally**. Sono disponibili tre
nuovi cockpit completi, ognuno con boot, live e finish dedicati su entrambi gli
schermi:

1. **Baja Amber** — strumentazione desert-race, tachimetro a segmenti e pannelli ambra;
2. **Night Ops** — griglia NVG, ciano ad alta leggibilità e accenti lime;
3. **Trail Topo** — palette expedition olive/sabbia, bussola e dettagli topografici.

Pilota e copilota possono usare temi diversi nello stesso momento.

## Foto di boot e cornice digitale

Il selettore usa `accept="image/*"` e funziona dal browser di iPhone/iPad,
Android, Windows, macOS e Linux. Il browser ridimensiona e converte localmente
la foto in JPEG per i due formati reali (160×80 e 240×240) prima dell'upload:
nessun originale ad alta risoluzione viene inviato o conservato sull'ESP32.

- una foto può essere usata come immagine di avvio su entrambi i display;
- la galleria contiene fino a 24 foto, con rimozione singola dal portale;
- il crop può essere `Riempi` o `Adatta` al momento dell'importazione;
- lo slideshow può occupare il display pilota, il copilota o entrambi;
- intervallo 3–120 s, ordine sequenziale/casuale, taglio o dissolvenza;
- l'overlay opzionale mostra indice foto e countdown.

Le immagini ottimizzate vivono nella partizione LittleFS da circa 4,9 MB. La
dissolvenza usa un dithering progressivo senza allocare un secondo framebuffer
240×240, preservando RAM e DMA dei display.

## Pinout (ESP32-S3 DevKitC-1)

Tutto il pinout e il tuning vivono in **`include/config.h`** — unico punto di modifica.

### Display A — ST7735S 160×80 (Dashboard)

| Segnale modulo | GPIO | Note |
|---|---|---|
| SDA / MOSI | **11** | FSPI MOSI |
| SCL / SCLK | **12** | FSPI SCLK |
| CS         | **10** | |
| DC / A0    | **9**  | |
| RST        | **8**  | `-1` in config.h se legato a 3.3 V |
| BLK / BL   | **46** | PWM; `-1` se legato a 3.3 V |
| VCC        | 3.3 V  | |
| GND        | GND    | |

### Display B — ST7789 240×240 (Navigator)

| Segnale modulo | GPIO | Note |
|---|---|---|
| SDA / MOSI | **13** | HSPI MOSI |
| SCL / SCLK | **14** | HSPI SCLK |
| CS         | **15** | molti moduli 240×240 non hanno CS: lascia il pin libero e metti `-1` |
| DC         | **16** | |
| RST        | **17** | |
| BLK / BL   | **18** | |
| VCC        | 3.3 V  | |
| GND        | GND    | |

Cavi corti (<10 cm) per i clock a 40/80 MHz; se vedi glitch abbassa
`DISP_A_SPI_HZ` / `DISP_B_SPI_HZ` in config.h.

## Tuning ST7735 160×80 (leggi prima di dare la colpa al wiring!)

I moduli 0.96" 160×80 usano una finestra 80×160 dentro una GRAM 132×162 e
richiedono quasi sempre offset e spesso inversione colore. In `config.h`:

| Sintomo | Rimedio |
|---|---|
| Immagine spostata / bordo di pixel random | ritocca `DISP_A_COL_OFFSET` (tipico **26**, a volte 24) e `DISP_A_ROW_OFFSET` (tipico **1**, a volte 0) |
| Colori invertiti (sfondo bianco invece che nero) | inverti `DISP_A_INVERT` |
| Rosso e blu scambiati | inverti `DISP_A_RGB_ORDER` |
| Immagine capovolta | `DISP_A_ROTATION` 1 ↔ 3 (landscape), `DISP_B_ROTATION` 0 ↔ 2 |

Per l'ST7789: quasi tutti i moduli 240×240 vogliono `DISP_B_INVERT = true` (già il default).

## Build / Flash / Monitor

```bash
pio run                          # build firmware
pio run -t upload                # flash (USB-C nativo, auto-reset 1200bps)
pio device monitor               # monitor 115200 (stampa FPS e stato sim ogni 1s)
pio test -e native               # unit test host-compiled (g++, nessun hardware)
```

Senza PlatformIO i test girano anche con g++ puro:

```bash
g++ -std=c++17 -DNATIVE_BUILD -I src -I include -I <unity> \
    src/core/*.cpp test/test_core/test_main.cpp -o test_core && ./test_core
```

## Architettura

```
include/config.h        ← pin, offset, velocità SPI, unità (MPH/km/h, °F/°C)
src/
  core/                 ← C++ puro, ZERO dipendenze Arduino (testabile su host)
    state.h             ← VehicleState condiviso tra i due display
    simulator.{h,cpp}   ← fisica: throttle/brake → velocità/RPM/marce/batteria/temp
    roadbook.{h,cpp}    ← stage demo 9 waypoint + avanzamento su odometro
    driver.{h,cpp}      ← autopilot roadbook-aware (demo mode)
    race.{h,cpp}        ← state machine BOOT/LIVE/FINISH, stats run, alert
    anim.h              ← helper animazioni (pulse, ease, lerp565)
    easing.h            ← curve easing, Follow (attack/release), SlideAnim
    noise.h             ← value noise deterministico (jitter, vibrazione)
    compass.h           ← geometria nastro bussola tape-style
    formatter.{h,cpp}   ← formattazione valori in buffer (no heap)
  hal/
    display_a.h         ← LGFX device ST7735S su SPI2_HOST + DMA
    display_b.h         ← LGFX device ST7789  su SPI3_HOST + DMA
  app/
    scene_dashboard.*   ← render 160×80 (sprite full-frame)
    scene_navigator.*   ← render 240×240 (sprite full-frame)
    scene_*_themes.cpp  ← Baja Amber, Night Ops, Trail Topo
  services/
    config_portal.*     ← AP, captive DNS, REST e persistenza impostazioni
    photo_store.*       ← LittleFS, upload, boot image e slideshow
    settings_page.h     ← web app responsive embedded nel firmware
  main.cpp              ← loop core 1 = sim + dashboard 30 FPS
                          task core 0 = navigator 12 FPS
test/test_core/         ← unit test Unity (roadbook, simulatore, formatter)
```

- **Frame rate disaccoppiati**: `DASH_FRAME_MS` (33 ms) e `NAV_FRAME_MS` (80 ms) in config.h.
- **Niente delay()**: il loop si autoregola con `millis()`, il task navigator con `vTaskDelayUntil()`.
- **Due host SPI + DMA**: le push dei due sprite avvengono in parallelo su bus fisicamente separati; gli sprite sono forzati in RAM interna (`setPsram(false)`) per restare DMA-friendly.

### Pitfall noto (già gestito): `build_src_filter`

In `platformio.ini` il filtro elenca esplicitamente `+<core/*.cpp>` ecc.:
con il solo `+<*>` alcune versioni di PIO non linkano i .cpp nelle sottocartelle
**silenziosamente** (il firmware compila ma i simboli mancano o usano stub).
Se aggiungi una cartella sotto `src/`, aggiungila anche al filtro.

## UI

**Dashboard 160×80** — sfondo nero, accent lime: velocità grande al centro (7-seg),
marcia D/1-5 in giallo con modalità AUTO/MAN, barra RPM a segmenti verde→giallo→rosso,
angoli alti con batteria e temperatura + micro-barre, riga inferiore heading + 4WD LOCK.

**Navigator 240×240** — header con GPS/SAT, titolo WAY MAP e orologio; tabella roadbook
a righe bianche (DIST | freccia | INFO | TOT) con waypoint attivo evidenziato in giallo e
pericoli in rosso, frecce disegnate con primitive (dritto, 45°, 90°, hairpin, guado, finish);
colonna destra con SPEED / GEAR / RPM / 4WD; footer con heading e distanza al prossimo
waypoint. La lista scorre da sola man mano che l'odometro simulato avanza.

Per provare il portale senza hardware:

```bash
python3 tools/preview_portal.py
# apri http://127.0.0.1:8765
```

## Unità

`config.h`: `USE_KMH` (default `false` → MPH/mi come le reference) e `USE_CELSIUS`
(default `false` → °F). Cambiale e tutta la UI si adegua.
