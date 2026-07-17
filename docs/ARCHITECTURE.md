# Architettura — Ultra4 Microdash beta 5

Questo documento descrive l’implementazione reale di `5.0.0-beta.1` sul target
ESP32-S3 DevKitC-1 N8. Le prestazioni indicate sono obiettivi di progetto: la
validazione finale va eseguita sull’hardware di produzione.

## Vista d’insieme

```text
Telefono / browser
    │  Wi-Fi AP locale + HTTP
    ▼
ConfigPortal ── impostazioni / media / gioco / RTL / diagnostica
    │
    ├── ExperienceEngine ── TrailAssaultGame + AudioEventBus
    ├── PhotoStore ──────── LittleFS transazionale
    ├── RtlLogic ────────── WS2812 + stato ESC
    └── DiagnosticsRegistry
             │
             ▼ snapshot immutabile
      SceneDashboard       SceneNavigator
      ST7735S 160×80       ST7789 240×240
```

Il portale è incorporato nel firmware (`PROGMEM`), quindi resta disponibile
anche se LittleFS non viene montato. Le risorse grafiche di dashboard e gioco
sono primitive/RGB565 compilate nel programma; LittleFS è riservato ai media
utente e ai file transazionali.

## Moduli

| Percorso | Responsabilità |
|---|---|
| `src/core/` | simulatore, gara, Trail Assault, audio events, ESC/RTL, diagnostica e CRC config |
| `src/hal/` | display LovyanGFX, PWM ESC e uscita WS2812 |
| `src/app/` | scene pilota/copilota per dashboard, gioco e media |
| `src/services/` | access point, HTTP API, pagina web, NVS e PhotoStore |
| `include/config.h` | pin, frequenze, timeout e tarature hardware |
| `test/` | test Unity nativi, senza Arduino né display |
| `tools/` | flash assistito, anteprima del portale e conversione asset |

## Avvio e modalità degradata

L’ordine di avvio riduce la frammentazione e rende visibili i guasti:

1. inizializzazione seriale e registro diagnostico;
2. inizializzazione dei due display e allocazione anticipata dei framebuffer
   nella RAM interna;
3. logo iniziale;
4. inizializzazione scene;
5. mount LittleFS, caricamento configurazione, RTL/ESC e access point;
6. reset simulatore, roadbook ed esperienza;
7. creazione del task di controllo;
8. schermata localizzata **CONTROLLO SISTEMA / SYSTEM CHECK**;
9. creazione del task copilota;
10. ingresso nel loop normale.

Il controllo iniziale riporta display A/B, LittleFS, PSRAM, RTL, ESC e AP. Sul
target N8 `PSRAM OPTIONAL` è previsto. Un errore non essenziale non blocca il
resto del prodotto:

- LittleFS assente: dashboard, gioco e portale restano operativi, MEDIA no;
- display copilota assente: il pilota e il portale continuano;
- task copilota non creato: viene registrato
  `navigator_task_start_failed`;
- task di controllo non creato: il loop esegue `controlIoTick()` ogni 10 ms e
  registra `control_task_start_failed`;
- record impostazioni non valido: l’ESC torna disabilitato e la diagnostica
  riporta `settings_crc_invalid`.

## Concorrenza e frequenze

| Contesto | Core | Priorità | Frequenza/periodo | Lavoro |
|---|---:|---:|---:|---|
| Arduino `loop()` | 1 | runtime Arduino | continuo | DNS/HTTP, simulazione, gioco e display pilota |
| Display pilota | 1 | nel loop | 33 ms, circa 30 FPS | render e push ST7735S |
| `control-io` | 0 | 3 | 10 ms, 100 Hz | campione ESC, logica RTL, watchdog e WS2812 |
| `navigator` | 0 | 1 | 80 ms normale | render e push ST7789 |
| `navigator` in gioco | 0 | 1 | 50 ms, 20 FPS | roadbook Trail Assault |
| Salute lenta | 1 | nel loop | 1 s | heap, filesystem, rete e uscite |
| Profilo seriale | entrambi | — | 5 s | FPS e tempi render/push |

Durante una segnalazione visiva critica della dashboard, il copilota può essere
rallentato a 120 ms. La fisica Trail Assault non dipende da questi periodi:
`experience.update()` viene chiamato anche prima del gate del display.

### Passaggio dello stato ai display

`SharedFrame` contiene telemetria, effetti, statistiche, impostazioni ed
`ExperienceFrame`. Esiste un solo writer. La pubblicazione usa un sequence lock:

1. il writer rende dispari il contatore;
2. copia l’intero frame;
3. rende pari il contatore;
4. il reader accetta la copia solo se il contatore è rimasto pari e invariato.

Ogni renderer lavora così su una copia immutabile senza bloccare l’altro
display. Stato RTL/ESC e profilo diagnostico del copilota usano sezioni critiche
brevi e copie locali; rete, NVS e filesystem non vengono eseguiti nel task di
controllo a 100 Hz.

## Dashboard

`UiTheme` conserva identificatori stabili:

| ID | Tema | Display pilota | Display copilota |
|---:|---|---|---|
| 0 | Classic Rally | strumentazione rally | roadbook classico |
| 1 | Race Realism | cluster racing | mappa/roadbook offroad |
| 2 | Stock Offroad | quadro OEM | navigazione OEM coordinata |

La lingua `0=Italiano`, `1=English`, unità, temperatura e luminosità viaggiano
nello stesso snapshot. Il portale beta 5 seleziona una coppia grafica coerente,
pur mantenendo nel record i campi pilota/copilota per compatibilità.

## Trail Assault deterministico

### Generazione

La chiave del percorso è:

```text
seed + modalità + generator_version(1)
```

Il generatore produce 64–80 segmenti da 10–20 m con curva, pendenza, larghezza,
superficie, ostacolo e flag. Ogni percorso include tutte le superfici: dirt,
rock, mud, sand, water e jump. Gate Rush inserisce gate regolari; Hill Climb
genera un dislivello netto positivo. Un fingerprint FNV del percorso permette
ai test di verificare la riproducibilità.

### Fisica

Il tempo ricevuto viene trasformato in unità intere `delta_ms × 60`. Ogni
1.000 unità viene eseguito un tick; non vengono usati delta time variabili o
float per l’integrazione principale. In questo modo chiamate a 5, 10 o 100 ms
producono lo stesso risultato allo stesso tick.

Ogni superficie definisce velocità massima, accelerazione, resistenza e grip.
La simulazione aggiunge pendenza, sterzo, drift, uscita pista, danni,
temperatura, collisioni, salti e atterraggio.

Se il loop resta bloccato durante una gara, il recupero è limitato a 500 ms,
cioè 30 tick: il firmware non riproduce secondi di fisica arretrata. Durante il
countdown può consumare in una chiamata solo i tick ancora necessari alla
partenza; l'eventuale tempo eccedente viene scartato e l'orologio input viene
riallineato.

### Contratto controller

Ogni comando contiene:

```text
session_id, sequence(uint16), client_ms, received_ms,
steering(-100..100), throttle(0..100), brake(0..100)
```

Una sessione errata o una sequenza duplicata/vecchia viene rifiutata. Dopo
300 ms senza input valido, il tick di fisica imposta sterzo, acceleratore e
freno a zero. Il browser invia inoltre una richiesta di neutro quando perde
focus, passa in background, cambia scheda o viene chiuso.

### Snapshot e limiti statici

Il gioco non alloca memoria nel tick. Lo snapshot è trivially-copyable e
limitato a 192 byte; contiene al massimo:

- 4 oggetti visibili;
- 8 campioni del percorso;
- 2 note roadbook;
- stato controller e ghost.

L’`AudioEventBus` è un ring buffer statico da 16 eventi. Il browser legge gli
eventi dopo una sequenza e sintetizza localmente motore ed effetti con Web
Audio.

Il ghost usa al massimo 300 campioni statici. Parte a 10 Hz; a ogni saturazione
dimezza la storia conservata e raddoppia il passo di campionamento. Copre così
l'intera gara con dettaglio progressivamente minore, senza crescita di memoria.
Viene salvato un solo best ghost in NVS, con schema, versione generatore e CRC,
ed è visibile soltanto se seed e modalità corrispondono.

## Persistenza impostazioni A/B con CRC

Le impostazioni beta 5 sono piccoli record NVS alternati tra `cfg5A` e `cfg5B`.
Questa ridondanza riguarda i dati, non l’immagine firmware.

Ogni record contiene un envelope di 16 byte:

```text
magic "U4CF" | schema | payload_size | sequence | crc32
```

Il CRC-32 IEEE è calcolato su campi little-endian espliciti e payload, senza
padding C++. Al salvataggio:

1. si incrementa la sequenza;
2. si scrive lo slot opposto a quello attivo;
3. si rilegge lo slot;
4. si convalida magic, schema, dimensione e CRC;
5. solo allora lo slot diventa attivo.

All’avvio viene scelto il record valido più recente con confronto wrap-safe
della sequenza; in caso di parità deterministica vince A. Se non esiste ancora
un record v5 valido, le precedenti chiavi singole vengono preservate e
racchiuse in un nuovo record. Se sono presenti record corrotti, gli output
fisici tornano allo stato sicuro.

La password AP e il ghost hanno persistenza separata. Il ghost ha un proprio
CRC ma non usa la coppia A/B.

## PhotoStore non distruttivo

La partizione viene montata esplicitamente con etichetta `littlefs`; è previsto
il fallback in sola compatibilità all’etichetta storica `spiffs`. Il mount non
formatta automaticamente una memoria illeggibile.

Ogni immagine è una coppia:

```text
pilota 160×80  -> *_a.jpg
copilota 240×240 -> *_b.jpg
```

Il protocollo usa un ID transazione esadecimale e impone l’ordine A, poi B.
I file arrivano prima in `*.new`; il commit esegue una validazione strutturale
JPEG baseline, inclusi SOI/EOI, frame, scan e dimensioni previste. Se esiste una
coppia precedente, viene rinominata `*.bak` prima di pubblicare quella nuova.
Il processo è una transazione applicativa basata su staging, backup e recovery,
non una garanzia di atomicità del filesystem. Un abort o il recovery all’avvio
ripristina la coppia precedente quando possibile; un asset che fallisce il
decode in esecuzione viene messo in quarantena e non riutilizzato.

Un mutex serializza upload, lettura HTTP e decode sui display. Anteprime,
upload, commit ed eliminazioni vengono rifiutati durante countdown e gara, per
non contendere filesystem e CPU ai task real-time. Una transazione web inattiva
viene abortita dopo 30 secondi. Il catalogo e il conteggio byte sono cached
dopo ogni mutazione, evitando la scansione LittleFS a ogni frame.

Limiti: 24 slot logici, 64 KiB variante A, 192 KiB variante B e 32 KiB di
riserva. La capacità fisica prevale sul numero di slot: non è possibile
riempire tutti i 24 slot con coppie alla dimensione massima. Asset
sistema/gioco sono nel firmware e risultano quindi 0 byte nella classificazione
LittleFS.

## Asset RGB565 opzionali

Il gioco corrente usa primitive compilate e non decodifica JPEG nel gameplay.
Per aggiungere in futuro piccoli PNG/WebP preconvertiti:

```bash
python3 -m pip install --user pillow
python3 tools/convert_game_assets.py assets/vehicle.png assets/rock.png \
  --output include/generated/trail_assets.h --max-side 240
```

Lo script genera array RGB565 `PROGMEM` con color key, senza dipendenze runtime
sull’ESP32. Ogni nuovo asset va conteggiato nello spazio `app0` e profilato
prima dell’integrazione.

## Diagnostica

`DiagnosticsRegistry` aggrega dati in strutture statiche:

- display: stato, heartbeat, stack libero, FPS, frame totali, tempo
  medio/massimo completo, render e push;
- memoria: heap libero, minimo, blocco maggiore, PSRAM presente/totale/libera;
- storage: totale, usato, libero, foto utente e transazioni;
- input: ricevuti, accettati, gap, stale, timeout e latenza;
- output: RTL, ESC valido e impulso, heartbeat e stack del task controllo;
- firmware, protocollo 4, uptime 64 bit, ultimo errore e reset.

Il contatore uptime e le sequenze sono gestiti con sottrazione unsigned, quindi
restano corretti al wrap di `millis()` e dei contatori. I dati sono disponibili
in HOME e tramite `/api/v4/diagnostics` con `Cache-Control: no-store`.

## Memoria flash N8

La tabella da 8 MiB è:

| Partizione | Offset | Dimensione | Uso |
|---|---:|---:|---|
| `nvs` | `0x009000` | `0x005000` | configurazione e ghost |
| `otadata` | `0x00E000` | `0x002000` | metadati ESP-IDF |
| `app0` | `0x010000` | `0x300000` | unico firmware |
| `littlefs` | `0x310000` | `0x4F0000` | media utente |

Non esistono `app1` o una partizione factory alternativa. La presenza di
`otadata` e il subtype `ota_0` di `app0` **non** creano un sistema OTA A/B:
non c’è uno slot di destinazione per un aggiornamento né uno slot da usare per
rollback. L’aggiornamento supportato è via USB.

Il target non ha PSRAM. Framebuffer, snapshot e buffer sono quindi allocati in
RAM interna e mantenuti compatti/statici. Non aumentare galleria, JSON, oggetti
visibili o ghost senza rieseguire misure di heap minimo e blocco contiguo.

## Confini di sicurezza

- Il prodotto non è una ECU, un data logger certificato o uno strumento
  omologato.
- L’ingresso ESC è di sola lettura e disabilitato di default.
- L’uscita WS2812 non sostituisce luci, fusibili, relè o interlock reali.
- Il portale HTTP è protetto soltanto dalla password Wi-Fi locale; non usa TLS.
- Con al massimo due client, upload e richieste sono comunque serializzati dal
  WebServer e da PhotoStore.
- Fullscreen, orientation lock, Web Vibration e riproduzione audio dipendono
  dalle policy del browser, in particolare dal captive assistant iOS.
- Prima di una distribuzione di massa completare il collaudo in
  [TESTING.md](TESTING.md) su ogni revisione hardware.
