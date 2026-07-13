# ESP32-S3 Dual Screen — Ultra4 Microdash

> Beta `4.1.0-beta.1` — tre dashboard, italiano/inglese, media, gioco arcade e
> barra RTL. Tutto gira localmente sull’ESP32-S3: nessun cloud e
> nessuna app da installare.

## Avvio rapido

- [Istruzioni essenziali](START_HERE.md)
- [Guida rapida illustrata](output/pdf/Guida_Rapida_ESP32_Dual_Screen.pdf)

Per programmare o aggiornare la scheda, senza Homebrew:

```bash
# macOS / Linux
python3 tools/flash_dashboard.py --flash

# Windows
py tools\flash_dashboard.py --flash
```

Al primo uso il tool può installare PlatformIO. Internet serve soltanto per
scaricare gli strumenti; il dispositivo funziona offline.

## Collegamento dal telefono

1. Accendi il dispositivo e attendi circa 5 secondi.
2. Leggi rete e password sui display.
3. Collegati a `ULTRA4-DASH-XXXXXXXX`.
4. Resta collegato anche se il telefono segnala “nessuna Internet”.
5. Apri `http://192.168.4.1` se il portale non compare da solo.

La password `U4-…` è casuale, unica e persistente. Dopo la prima configurazione
puoi rivedere le credenziali tenendo premuto **BOOT**. Il captive portal è
compatibile con iOS, Android, Windows, macOS e Linux.

### Prima configurazione

Sono richiesti soltanto:

- numero gara;
- nome veicolo;
- pilota.

I tre valori restano modificabili dalla HOME. Nessun campo riceve autofocus:
su iPhone la pagina può essere fatta scorrere senza apertura forzata della
tastiera.

## Portale a cinque schede

| Scheda | Contenuto |
|---|---|
| **HOME** | riepilogo compatto del veicolo e telemetria essenziale |
| **DASH** | tre grafiche, luminosità e unità di misura |
| **MEDIA** | foto di avvio e cornice digitale, con anteprime compatte |
| **GAME** | DESERT BLITZ e controlli di gioco |
| **RTL** | barra WS2812, test luci e configurazione ESC |

Il menu resta fisso in basso. Cambiando scheda gli input del gioco tornano
neutrali e gli eventuali campi testuali perdono il focus.

Il selettore **IT / EN** nell’intestazione cambia subito la lingua dell’intero
sistema: portale, messaggi e testi sui due display. La scelta resta memorizzata.

## Grafiche cockpit

- **Classic Rally** conserva il layout originale da gara.
- **Race Realism** abbina un display pilota ispirato a Racepak IQ3S e una
  navigazione copilota ispirata a onX Offroad.
- **Auto di serie** ricrea un quadro strumenti OEM offroad: tachimetro centrale,
  indicatori veicolo e navigazione coordinata sui due display.

La shift strip superiore Race Realism usa filtro, isteresi e limite di
variazione: con RPM stabile anche i LED restano stabili. Tutti gli elementi
sono disegnati con primitive originali; il progetto non è affiliato ai marchi
citati e non sostituisce strumentazione certificata.

## Foto e cornice digitale

Il browser accetta **JPG, JPEG, PNG e WEBP**. La nuova area MEDIA separa
chiaramente anteprima, selezione e azioni, quindi ottimizza l’immagine per i due
display prima dell’invio. Nome, avanzamento e risultato restano visibili; i
pulsanti vengono bloccati per evitare invii doppi.

- immagine di avvio con durata e ritaglio regolabili;
- fino a 24 foto nella cornice digitale;
- display pilota, copilota o entrambi;
- intervallo, ordine, transizione e informazioni sovrapposte regolabili;
- anteprime persistenti anche dopo il refresh della pagina.

Il backend monta esplicitamente la partizione LittleFS `littlefs`, con
compatibilità per la precedente etichetta `spiffs`. Staging, verifica JPEG,
backup e commit atomico impediscono che una connessione interrotta pubblichi
file incompleti. All’avvio vengono recuperate automaticamente eventuali
transazioni rimaste sospese.

## DESERT BLITZ

Gioco offroad arcade semplice e immediato. L’ESP32 gestisce fisica, ostacoli,
collisioni, vite, combo, punteggio e record; telefono e tastiera inviano solo i
comandi.

- START, pausa/riprendi, fine partita e riavvio;
- ostacoli ad alto contrasto sul display pilota;
- touch: sinistra, destra e GAS;
- tastiera: `←`/`→` oppure `A`/`D`, GAS con `↑`/`W`/spazio, pausa con `P`;
- perdita del controller: sterzo e gas tornano automaticamente neutrali.

La scheda GAME è l’unica a usare la palette retrogaming.

## Barra RTL e ingresso ESC

La barra prevista usa 16 LED WS2812 su **GPIO 4**. Profili disponibili:

- `RTL_S`;
- `ULTRA4_4400`.

Comandi manuali: running, stop, left, right, hazard, reverse, dust e pit test.
Un task dedicato e due watchdog mantengono l’uscita sicura anche durante upload
lunghi o perdita del portale.

L’ingresso PWM ESC su **GPIO 5** è disabilitato di default. Quando viene
installato e abilitato, il segnale viene normalizzato con smoothing, zona
neutra, isteresi e rate limiting. La macchina a stati distingue:

`FORWARD → NEUTRAL → BRAKING → REVERSE_ARMED → REVERSE`

Sono supportati tre comportamenti:

- avanti / freno / retro;
- crawler avanti / retro;
- avanti / freno.

Per un ESC da gara la retromarcia richiede freno, ritorno al neutro e un secondo
comando confermato. Perciò la barra non confonde mai la prima frenata con la
retromarcia: rosso in frenata, bianco soltanto nello stato reverse confermato.

### Sicurezza elettrica

- GPIO 4 è solo DATA: la barra richiede un’alimentazione 5 V separata,
  dimensionata e protetta;
- collega le masse e usa un level shifter 3,3→5 V adatto;
- non collegare mai LED o GPIO direttamente ai 12 V;
- il segnale ESC deve essere condizionato e limitato a 3,3 V;
- la barra è scenografica e non sostituisce luci omologate o circuiti di
  sicurezza indipendenti.

## Hardware e pinout

| Display | Pannello | Risoluzione | Ruolo | Host SPI |
|---|---|---:|---|---|
| A | ST7735S 0,96" | 160×80 | pilota | `SPI2_HOST` |
| B | ST7789 1,3" | 240×240 | copilota | `SPI3_HOST` |

| Segnale | Display A | Display B |
|---|---:|---:|
| MOSI | GPIO 11 | GPIO 13 |
| SCLK | GPIO 12 | GPIO 14 |
| CS | GPIO 10 | GPIO 15 / `-1` |
| DC | GPIO 9 | GPIO 16 |
| RST | GPIO 8 | GPIO 17 |
| Backlight | GPIO 46 | GPIO 18 |

| Espansione | GPIO | Stato iniziale |
|---|---:|---|
| Mostra credenziali / BOOT | 0 | attivo basso |
| WS2812 DATA | 4 | controller attivo |
| ESC PWM | 5 | disabilitato |

I display lavorano a 3,3 V. Usa cavi SPI corti.

## API locale

| Metodo | Endpoint | Funzione |
|---|---|---|
| GET | `/api/v3/capabilities` | versione e funzioni disponibili |
| GET | `/api/v3/state` | veicolo, gioco e ostacoli visibili |
| GET/POST | `/api/v3/profile` | i tre dati del veicolo |
| POST | `/api/v2/game/start` | nuova partita |
| POST | `/api/v2/game/input` | sterzo e acceleratore |
| POST | `/api/v2/game/action` | start, pause, resume, restart o quit |
| GET | `/api/v3/rtl/state` | pixel, funzione e stato ESC |
| POST | `/api/v3/rtl/config` | profilo RTL e taratura ESC |
| POST | `/api/v3/rtl/command` | comando luce o reset |
| GET/POST | `/api/settings` | dashboard e media |
| GET | `/api/photos` | slot e stato LittleFS |
| GET | `/api/media/{boot,photo}` | anteprima persistente |
| POST | `/api/{photos,boot}/{commit,abort}` | transazione media |
| POST | `/api/upload/{photo,boot}-{a,b}` | upload ottimizzato |

Le risposte dinamiche usano `Cache-Control: no-store`. Frequenze nominali:
stato 5 Hz, input gioco 10 Hz.

## Build e verifica

```bash
pio run
pio run -t upload
pio device monitor
pio test -e native

python3 tools/preview_portal.py
# http://127.0.0.1:8765
```

## Architettura

```text
src/core/       simulazione, gara, gioco, ESC, shift light e logica RTL
src/hal/        display, cattura PWM e uscita WS2812
src/app/        scene Classic, Race Realism, Auto di serie e arcade
src/services/   portale, persistenza, media e LittleFS
test/           test nativi deterministici
tools/          flash, anteprima e generatore guida
```

Il rendering non usa `delay()`. I due display hanno framebuffer e frequenze
indipendenti; lo stato passa tra i task tramite snapshot compatti.

## Problemi comuni

- **Rete assente:** attendi 5 secondi, poi scollega USB per 10 secondi.
- **Pagina assente:** resta sulla rete del dispositivo e apri
  `http://192.168.4.1`.
- **Foto non caricata:** controlla il messaggio sotto la barra di avanzamento;
  formato, rete, memoria e filesystem vengono distinti.
- **Scheda non trovata durante il flash:** tieni premuto BOOT, premi e rilascia
  RESET, rilascia BOOT e ripeti.
- **Componente caldo, odore o cavo danneggiato:** scollega subito e fai
  controllare il dispositivo.
