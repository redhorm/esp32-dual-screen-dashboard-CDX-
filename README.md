# ESP32-S3 Dual Screen — Ultra4 Microdash

> Beta `5.0.0-beta.1` — dashboard doppia, portale AP offline, media,
> diagnostica, barra RTL e **ULTRA4: Trail Assault**. Nessun cloud e nessuna
> app da installare.

## Inizia da qui

- [Procedura essenziale](START_HERE.md)
- [Architettura e limiti reali](docs/ARCHITECTURE.md)
- [API locale](docs/API.md)
- [Build, test e collaudo](docs/TESTING.md)

Per compilare e aggiornare una scheda gia inizializzata, senza Homebrew:

```bash
# macOS / Linux
python3 tools/flash_dashboard.py --flash

# Windows
py tools\flash_dashboard.py --flash
```

Il tool cerca PlatformIO e, se manca, può installarlo in un ambiente Python
isolato dentro il progetto. Internet serve al primo download degli strumenti;
il dispositivo poi funziona completamente offline.

Su una scheda nuova, oppure quando vuoi ricreare intenzionalmente la partizione
MEDIA, usa `--flash --factory-init-media`. Questa opzione inizializza LittleFS
e **cancella tutte le foto e l'immagine boot**; il normale `--flash` le conserva.

## Stato beta 5

| Area | Stato |
|---|---|
| AP, onboarding, IT/EN e cinque schede | implementati |
| Classic Rally, Race Realism, Auto di serie | implementati; collaudo visivo richiesto su ogni pannello |
| Trail Assault, tre modalità, controller e Web Audio | implementati; Web API dipendenti dal browser |
| Boot image, cornice e recovery transazionale | implementati; power-loss test richiesto prima della distribuzione |
| Diagnostica protocollo 4 | implementata |
| RTL WS2812 | implementata; richiede cablaggio protetto e collaudo |
| Ingresso ESC PWM | sperimentale, sola lettura e disabilitato di default |
| OTA / rollback firmware | non supportati su N8 |

Non sono pubblicati numeri di FPS, latenza o heap come “misurati” finché non
provengono dalla diagnostica di una scheda reale nella stessa revisione.

## Collegamento dal telefono

1. Accendi il dispositivo e attendi il controllo iniziale.
2. Al primo avvio leggi rete e password sui display; su una scheda gia
   configurata tieni premuto **BOOT** per mostrarle di nuovo.
3. Collegati a `ULTRA4-DASH-XXXXXXXX`.
4. Resta collegato anche se il telefono indica “nessuna Internet”.
5. Se il portale non compare, apri `http://192.168.4.1`.

La password `U4-…` è casuale, unica per la scheda e persistente. Il punto di
accesso accetta al massimo due client. Tenendo premuto **BOOT** durante il
funzionamento si mostrano nuovamente le credenziali.

Il portale è pensato per Safari/iOS, Chrome/Android e browser moderni su
Windows, macOS e Linux. È una rete locale HTTP senza Internet: non inserire
segreti diversi dai dati del veicolo.

## Primo avvio e portale

La configurazione iniziale richiede soltanto numero gara, nome veicolo e
pilota. Nessun campo riceve autofocus, quindi su iPhone la tastiera non blocca
lo scorrimento. I dati restano modificabili dalla HOME.

Il menu fisso in basso contiene cinque schede:

| Scheda | Contenuto |
|---|---|
| **HOME** | identità del veicolo, stato rapido e diagnostica espandibile |
| **DASH** | grafica, unità, temperatura e luminosità dei due display |
| **MEDIA** | immagine di avvio e cornice digitale |
| **GAME** | Trail Assault, controller analogico e audio del telefono |
| **RTL** | barra WS2812, funzioni luce e lettura PWM ESC |

Il selettore **IT / EN** cambia portale e testi dei due display e salva la
scelta. Uscendo da GAME i comandi tornano neutrali; uscendo da RTL un comando
manuale viene cancellato.

## Tre dashboard coordinate

- **Classic Rally** — layout originale, immediato e ad alto contrasto.
- **Race Realism** — quadro pilota da competizione e vista copilota con
  mappa/roadbook offroad. Grafica, nomi e simboli sono originali.
- **Auto di serie / Stock Offroad** — quadro OEM moderno con strumenti,
  stato 4×4 e navigazione coordinata.

La scelta applica una coppia coerente ai display pilota e copilota. Le tre
dashboard usano grafica originale e non riproducono loghi o asset UI di
produttori. I nomi ULTRA4 e BAJA DESIGNS compaiono come riferimenti descrittivi
al contesto richiesto: appartengono ai rispettivi titolari e questo progetto
non è affiliato né sostituisce strumentazione automobilistica certificata.

## ULTRA4: Trail Assault

Trail Assault è una simulazione offroad deterministica eseguita sull’ESP32,
non nel telefono. Il browser mostra il gioco e invia soltanto i comandi.

### Modalità

- **Time Attack** — completa il percorso nel minor tempo possibile.
- **Gate Rush** — attraversa tutti i gate prima che scada il tempo; ogni gate
  valido aggiunge tempo.
- **Hill Climb** — gestisci pendenza, temperatura e danni fino alla cima.

Ogni coppia `modalità + seed` genera sempre lo stesso percorso con generatore
versione 1: 64–80 segmenti, curve, pendenze, larghezze, checkpoint, gate,
salti e ostacoli. I terreni `dirt`, `rock`, `mud`, `sand`, `water` e `jump`
modificano aderenza, accelerazione, resistenza e velocità massima.

La fisica usa un passo fisso esatto di **60 Hz**, indipendente dal rendering.
Il display pilota mostra il tracciato e gli ostacoli; il copilota riceve fino
a due note roadbook future. Il seed si può copiare per ripetere la stessa
sfida.

### Controller mobile

- volante analogico da −100 a +100;
- acceleratore e freno analogici separati da 0 a 100;
- multitouch per sterzare e dosare gas/freno insieme;
- input nominale 20 Hz e stato nominale 10 Hz;
- neutralizzazione automatica dopo 300 ms senza pacchetti, cambio scheda,
  `blur`, `pagehide` o pagina in background;
- sessione e sequenza impediscono che pacchetti vecchi comandino una nuova
  partita.

Da tastiera: `A/D` o `←/→` per sterzare, `W/↑/spazio` per accelerare,
`S/↓` per frenare e `P` per pausa. Un gamepad compatibile può fornire asse,
acceleratore e freno.

### Ghost, audio e limiti

Il miglior ghost viene salvato con CRC e confrontato solo quando modalità e
seed coincidono. La memoria resta fissa a **300 campioni**: quando si riempie,
il firmware dimezza i campioni conservati e raddoppia l'intervallo, così il
ghost continua a rappresentare l'intera gara con dettaglio progressivamente
ridotto. Esiste un solo record ghost persistente alla volta.

Motore ed effetti sono sintetizzati dal browser con Web Audio: non vengono
trasmessi file audio e l’ESP32 non pilota un altoparlante. Su iPhone/iPad il
suono deve essere sbloccato da un gesto: tocca **START** o **TEST AUDIO**. La
vibrazione e lo schermo intero dipendono dal browser; il captive portal iOS può
non esporli.

## Immagine di avvio e cornice digitale

Il browser accetta **JPG/JPEG, PNG, WEBP e HEIC/HEIF** fino a 40 MB, quando il
browser del telefono è in grado di decodificarli; li ruota/ritaglia e genera
due JPEG già dimensionati: 160×80 per il pilota e 240×240 per il copilota. Se
un vecchio browser non apre HEIC, usa JPG o PNG. Il firmware applica inoltre
questi limiti finali:

| Variante | Limite firmware |
|---|---:|
| Pilota | 64 KiB |
| Copilota | 192 KiB |
| Riserva LittleFS sempre libera | 32 KiB |

Sono disponibili un’immagine boot e 24 slot logici nella galleria. Il numero
effettivamente riempibile dipende dalla dimensione dei JPEG: 24 coppie al
limite massimo supererebbero la capacità LittleFS. Si possono regolare durata
boot, ritaglio, display di destinazione, intervallo 3–300 s, ordine,
taglio/dissolvenza e dati sovrapposti.

Il salvataggio è non distruttivo:

1. le due varianti vengono caricate in staging;
2. il firmware valida struttura e frame JPEG;
3. la coppia precedente viene conservata come backup;
4. il commit pubblica entrambe le varianti oppure ripristina la coppia prima;
5. al riavvio vengono recuperate transazioni interrotte.

Un errore di mount non formatta automaticamente LittleFS. La scheda continua a
offrire dashboard e portale, mentre MEDIA viene indicata come non disponibile
o degradata.

## Controllo iniziale e diagnostica

All’accensione i display mostrano **CONTROLLO SISTEMA / SYSTEM CHECK** per
display A/B, LittleFS, PSRAM, RTL, ESC e access point. Su questa variante N8 la PSRAM non è montata:
`PSRAM OPTIONAL` è lo stato corretto, non un guasto.

HOME → **DIAGNOSTICA** e `GET /api/v4/diagnostics` espongono:

- FPS, tempi medi/massimi di frame, rendering e trasferimento dei display;
- heap libero/minimo, blocco contiguo maggiore e stato PSRAM;
- spazio LittleFS classificato tra foto utente e transazioni;
- AP e client collegati;
- pacchetti controller, perdite, pacchetti vecchi, timeout e latenza;
- RTL, ESC, ultimo errore e motivo dell’ultimo reset.

Il firmware continua in modalità degradata quando una funzione non essenziale
non parte. Se il task di controllo non può essere creato, il loop principale
mantiene il tick di sicurezza a 100 Hz come fallback.

## Barra RTL e ingresso ESC

La barra prevista usa 16 LED WS2812 su **GPIO 4**, con profili `RTL_S` e
`ULTRA4_4400`. Sono disponibili running, freno, sinistra, destra, hazard,
retro/work, dust chase e pit test.

Un task dedicato aggiorna ingresso ESC e uscita RTL ogni 10 ms. I comandi
manuali scadono dopo 1,5 s se non vengono rinnovati. Il pit test richiede
sempre ingresso ESC abilitato, segnale valido e neutro stabile per almeno
180 ms; non usa la velocità simulata come scorciatoia.

L’ingresso PWM ESC su **GPIO 5** è disabilitato di default e di sola lettura:
non controlla motore, servo o ESC. La macchina a stati distingue
`FORWARD → NEUTRAL → BRAKING → REVERSE_ARMED → REVERSE`; per un ESC da gara
la retro richiede freno, neutro e conferma.

### Sicurezza elettrica

- GPIO 4 è soltanto DATA: alimenta la barra da 5 V separati, protetti e
  dimensionati, con massa comune e level shifter 3,3→5 V adatto;
- non collegare mai LED o GPIO ai 12 V;
- condiziona il segnale ESC affinché non superi 3,3 V;
- la barra è scenografica e non sostituisce luci omologate, interlock o sistemi
  di sicurezza indipendenti.

## Hardware e pinout

Target: **ESP32-S3 DevKitC-1 N8**, flash 8 MB, **senza PSRAM**.

| Display | Pannello | Risoluzione | Ruolo | Host SPI |
|---|---|---:|---|---|
| A | ST7735S 0,96" | 160×80 | pilota | `SPI2_HOST` |
| B | ST7789 1,3" | 240×240 | copilota | `SPI3_HOST` |

| Segnale | Display A | Display B |
|---|---:|---:|
| MOSI | GPIO 11 | GPIO 13 |
| SCLK | GPIO 12 | GPIO 14 |
| CS | GPIO 10 | GPIO 15 |
| DC | GPIO 9 | GPIO 16 |
| RST | GPIO 8 | GPIO 17 |
| Backlight | GPIO 46 | GPIO 18 |

| Espansione | GPIO | Stato iniziale |
|---|---:|---|
| Mostra credenziali / BOOT | 0 | attivo basso |
| WS2812 DATA | 4 | controller attivo |
| ESC PWM | 5 | disabilitato |

I display lavorano a 3,3 V. Usa cavi SPI corti e un’alimentazione stabile.

## Limiti della variante N8

- nessuna PSRAM: framebuffer, snapshot, audio events e ghost usano RAM interna
  e strutture a dimensione fissa;
- un solo slot firmware `app0` da 3 MiB;
- LittleFS nominale `0x4F0000` byte, circa 4,94 MiB;
- nessun secondo slot `app1`: **non esistono OTA A/B né rollback firmware**;
- aggiornamento firmware esclusivamente via USB con il tool di flash o
  PlatformIO;
- i record A/B con CRC riguardano le **impostazioni NVS**, non il firmware;
- prestazioni e stabilità finali devono essere misurate sulla scheda reale,
  non dedotte dal solo build.

## Build e verifica

```bash
pio run
pio test -e native
python3 tools/preview_portal.py
# apri http://127.0.0.1:8765
```

Per caricare via USB:

```bash
pio run -t upload
# oppure
python3 tools/flash_dashboard.py --flash
```

Per la **prima installazione su una scheda nuova**:

```bash
python3 tools/flash_dashboard.py --flash --factory-init-media
```

Attenzione: `--factory-init-media` cancella l'intera galleria e l'immagine
boot. Non usarlo per un normale aggiornamento.

La procedura completa e la matrice di collaudo sono in
[docs/TESTING.md](docs/TESTING.md).

## Problemi comuni

- **Rete assente:** attendi CONTROLLO SISTEMA / SYSTEM CHECK; se AP è OFF, scollega
  l’alimentazione per 10 secondi e riaccendi.
- **Pagina assente:** resta collegato a ULTRA4-DASH e apri
  `http://192.168.4.1`.
- **Audio assente su iPhone:** tocca AVVIA / START o TEST AUDIO; riattiva il volume del
  telefono e non lasciare la pagina in background.
- **Foto rifiutata:** leggi il codice mostrato; elimina una foto se lo spazio è
  insufficiente e non interrompere il commit.
- **Scheda non trovata durante il flash:** usa un cavo dati, tieni premuto BOOT,
  premi e rilascia RESET, poi rilascia BOOT e ripeti.
- **Componente caldo, odore o cavo danneggiato:** scollega subito il sistema e
  fai controllare alimentazione e cablaggio.
