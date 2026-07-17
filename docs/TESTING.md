# Build, test e collaudo — beta 5

Questa guida separa tre livelli: test nativi deterministici, build firmware e
collaudo su hardware. Un build riuscito non sostituisce il collaudo fisico.

## Prerequisiti

- Python 3;
- cavo USB dati;
- ESP32-S3 DevKitC-1 N8 con 8 MB flash e senza PSRAM;
- display ST7735S 160×80 e ST7789 240×240;
- PlatformIO, installabile localmente dal tool del progetto.

Senza Homebrew:

```bash
python3 tools/flash_dashboard.py --flash
```

Il tool cerca anche `~/.platformio/penv/bin/pio` e può creare
`.tools/platformio-venv`.

## Test nativi

```bash
pio test -e native
```

La suite corrente registra **98 funzioni di test** in sei gruppi:

| Gruppo | Test | Copertura principale |
|---|---:|---|
| `test_core` | 56 | simulatore, race, animazioni, formattazione, RTL |
| `test_trail_assault` | 16 | seed, 60 Hz, modalità, replay input, stall, collisioni, ghost, audio |
| `test_esc_logic` | 10 | normalizzazione, isteresi, reverse, timer, shift light |
| `test_config_integrity` | 6 | CRC, corruzione, selezione A/B, wrap, migrazione |
| `test_diagnostics` | 6 | frame timing, uptime wrap, input, stringhe, snapshot |
| `test_ui_settings` | 4 | ID temi, lingua, testi display e copia sicura |

Il numero indica i `RUN_TEST` presenti nel sorgente, non una garanzia di esito.
Per una release, il comando deve terminare con tutti i gruppi `PASSED` nella
stessa revisione che verrà flashata.

### Invarianti importanti

I test devono dimostrare almeno:

- stesso `mode + seed + generator_version` → stesso fingerprint e percorso;
- cadenze di update diverse → stesso stato al medesimo tick 60 Hz;
- ogni percorso contiene tutti e sei i terreni;
- sessione errata e sequenza vecchia vengono rifiutate;
- timeout controller → input neutri;
- tutte e tre le modalità raggiungono un risultato valido;
- ghost corrotto non viene caricato;
- CRC standard e record A/B restano corretti al wrap sequenza;
- timer ESC e diagnostica restano corretti al wrap `millis()`.

## Build firmware

```bash
pio run
```

Controlli obbligatori nell’output:

- ambiente `esp32-s3-devkitc-1`;
- nessun errore o warning nuovo non spiegato;
- firmware entro `app0` da 3 MiB;
- RAM statica riportata dal linker entro il target N8 senza PSRAM; heap
  dinamico da misurare poi sulla scheda reale.

Non aggiungere `BOARD_HAS_PSRAM` per far passare il build: l’hardware target non
ha PSRAM.

Ultimo risultato completo disponibile **prima dell'ultimo passaggio
safety/UI** (non vale come gate finale della revisione corrente):

| Controllo | Risultato |
|---|---:|
| Test nativi del passaggio precedente | 95 / 95 superati |
| RAM statica firmware | 63.732 / 327.680 byte (19,4%) |
| Flash applicazione | 1.129.461 / 3.145.728 byte (35,9%) |

La revisione corrente registra ora 98 test e deve rilanciare entrambi i comandi
prima del flash. Questi numeri storici non sostituiscono il gate finale,
misure di heap dinamico o collaudo sui due display reali.

## Anteprima portale

```bash
python3 tools/preview_portal.py
```

Apri `http://127.0.0.1:8765` e verifica almeno queste viewport:

- iPhone portrait 390×844;
- iPhone landscape 844×390;
- Android piccolo 360×800;
- desktop 1280×800.

Checklist UI:

- onboarding senza autofocus e tastiera non aperta automaticamente;
- menu inferiore sempre raggiungibile;
- IT/EN applicato a HOME, DASH, MEDIA, GAME e RTL;
- tutte le label complete, senza testo tagliato sul lato sinistro;
- tre temi selezionabili e anteprime leggibili;
- MEDIA con anteprime pilota/copilota, pulsanti distinti e progress bar;
- GAME in landscape con volante, GAS e FRENO utilizzabili insieme;
- contrasto, focus visibile e target touch di almeno 44 px;
- `prefers-reduced-motion` non compromette l’uso.

L’anteprima non emula SPI, RAM, LittleFS, Web Audio iOS o timeout reali.

## Flash USB

Metodo assistito:

```bash
# Aggiornamento: conserva foto e boot
python3 tools/flash_dashboard.py --flash

# Solo prima installazione / reset MEDIA intenzionale
python3 tools/flash_dashboard.py --flash --factory-init-media
```

`--factory-init-media` inizializza la partizione ma cancella foto e immagine
boot. Non usarlo per un normale aggiornamento.

Metodo PlatformIO:

```bash
pio run -t upload
pio device monitor
```

Monitor seriale: 115200 baud.

Se la porta non compare:

1. usa un cavo dati conosciuto;
2. tieni premuto BOOT;
3. premi e rilascia RESET;
4. rilascia BOOT;
5. ripeti il flash.

La tabella ha un solo slot `app0`: non testare o pubblicizzare OTA A/B. Il
rollback consiste nel riflash USB di un binario precedente verificato.

## Collaudo hardware minimo

Eseguire dopo ogni flash destinato a utenti.

### 1. Boot e modalità degradata

- entrambi i display mostrano logo e CONTROLLO SISTEMA / SYSTEM CHECK;
- display A/B risultano READY;
- LittleFS risulta READY oppure MEDIA OFF con portale comunque accessibile;
- `PSRAM OPTIONAL` è accettato;
- ESC all’avvio è SAFE/OFF;
- scollegando un display, l’altro e il portale restano utilizzabili;
- un riavvio durante una transazione media non formatta LittleFS.

### 2. Access point e onboarding

- SSID ha suffisso univoco e password `U4-…`;
- password resta uguale dopo riavvio;
- massimo due client;
- captive portal provato su iOS e Android;
- accesso manuale a `192.168.4.1` funziona;
- BOOT mostra nuovamente le credenziali;
- profilo e lingua persistono dopo power cycle.

### 3. Dashboard

Per Classic Rally, Race Realism e Auto di serie:

- seleziona, salva, riavvia e verifica persistenza;
- controlla tutti i bordi fisici dei due pannelli;
- varia velocità/RPM e verifica che nessun testo si sovrapponga;
- prova IT/EN, mph/km/h e °C/°F;
- prova luminosità minima e massima;
- in Race Realism mantieni RPM stabile vicino a una soglia e verifica che la
  shift strip non lampeggi per jitter.

### 4. Trail Assault

Per ciascuna modalità:

1. copia un seed;
2. avvia e percorri almeno una sessione;
3. riavvia con lo stesso seed e verifica percorso/ostacoli identici;
4. prova sterzo parziale, gas parziale, freno parziale e multitouch;
5. verifica ostacoli sul pilota e note sul copilota;
6. controlla collisione, acqua/fango, salto, checkpoint e fine gara;
7. prova pausa, ripresa, restart e uscita;
8. lascia il controller senza pacchetti oltre 300 ms: tutti gli input devono
   tornare a zero;
9. passa a HOME, blocca il telefono e chiudi il captive portal: ogni caso deve
   neutralizzare il controller;
10. completa una gara, ripeti stesso mode/seed e verifica ghost/delta.

Su una gara oltre 30 secondi verifica che il ghost continui fino all'arrivo:
la storia viene decimata e il passo raddoppia, restando entro 300 campioni.

### 5. Audio e browser mobile

Su iPhone Safari/captive assistant e Android Chrome:

- prima di un gesto l’audio può restare sospeso;
- AVVIA / START o TEST AUDIO deve attivare Web Audio;
- master, motore ed effetti devono reagire;
- background/foreground deve mostrare la richiesta di riattivazione, se il
  browser sospende l’audio;
- assenza di Web Vibration o Fullscreen non è un guasto firmware;
- il gioco deve restare utilizzabile anche senza audio.

### 6. Media non distruttivi

Usa JPG, PNG, WEBP e HEIC/HEIF, orientamento portrait/landscape e file grandi.
Su browser senza decoder HEIC verifica che il messaggio consigli JPG/PNG:

- anteprime A/B hanno proporzioni corrette;
- boot resta dopo refresh e riavvio;
- 24 slot sono il massimo;
- il contatore spazio distingue foto e transazioni;
- upload B prima di A viene rifiutato;
- tx errato/scaduto viene rifiutato;
- interrompi rete durante A, durante B e subito prima del commit;
- in ogni caso la coppia precedente deve restare leggibile;
- togli alimentazione durante commit, riaccendi e verifica recovery;
- con meno di 32 KiB di riserva il nuovo upload viene rifiutato;
- una variante oltre 64 KiB/192 KiB viene rifiutata;
- una falsa JPEG o JPEG troncata non viene pubblicata;
- un mount fallito non deve avviare una formattazione automatica.

I 24 slot sono logici: il test deve fermarsi correttamente per `no_space`
quando la capacità reale termina, senza promettere 24 coppie alla dimensione
massima.

### 7. RTL ed ESC

Prima dei test usa alimentazione 5 V protetta, massa comune e livello logico
corretto.

- 16 pixel corrispondono all’anteprima web;
- running, brake, left, right, hazard, reverse, dust e pit test;
- un comando manuale non rinnovato decade entro 1,5 s;
- pit test senza ESC abilitato viene rifiutato con `esc_required_for_pit`;
- pit test con segnale assente o neutro stabile da meno di 180 ms viene
  rifiutato con `esc_not_neutral`;
- perdita segnale ESC produce stato sicuro;
- rumore attorno al neutro non alterna gli stati;
- su `forward_brake_reverse`, la prima frenata non accende la retro;
- la retro compare solo dopo sequenza freno → neutro → conferma;
- disabilitare ESC deve fermare la lettura senza influire su dashboard/gioco.

L’ingresso è di sola lettura: durante il test non deve mai generare un comando
verso motore o servo.

## Prestazioni e soak test

Apri HOME → DIAGNOSTICA oppure:

```bash
curl http://192.168.4.1/api/v4/diagnostics
```

Obiettivi nominali:

| Metrica | Obiettivo |
|---|---:|
| Display pilota | circa 30 FPS |
| Copilota dashboard | circa 12,5 FPS |
| Copilota Trail Assault | circa 20 FPS |
| Input browser | 20 Hz |
| Stato gioco | 10 Hz |
| Fisica | 60 Hz fissi |
| Tick controllo RTL/ESC | 100 Hz |

Eseguire almeno 60 minuti alternando dashboard, gioco, upload e RTL. Registrare
prima/dopo:

- `heapFreeBytes`, `heapMinBytes`, `heapLargestBytes`;
- FPS e tempi medi/massimi render/push;
- `packets`, `accepted`, `lost`, `stale`, `timeouts`;
- latenza ultima/media/massima;
- spazio e `transactionBytes`;
- `lastError`, reset reason e riavvii.

Non fissare una soglia heap universale senza dati della revisione hardware. Il
criterio è assenza di calo progressivo non spiegato, nessun reset e nessuna
transazione rimasta dopo il test.

## Release gate

Una beta distribuibile richiede, nella stessa revisione:

- tutti i 98 test nativi passati;
- build N8 riuscito e dimensioni annotate;
- flash USB riuscito su una scheda pulita e su una già configurata;
- collaudo iOS e Android;
- tre dashboard verificate su entrambi i pannelli;
- tre modalità Trail Assault completate;
- test timeout controller superato;
- test media con interruzione e power loss superato;
- RTL/ESC verificati con cablaggio protetto;
- soak test senza reset, corruzione o perdita progressiva di heap;
- versione in firmware, portale e documentazione coerente.

Conservare log PlatformIO, output test, schermata diagnostica iniziale/finale e
foto dei due display come evidenza di release.
