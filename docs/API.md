# API locale stormDASH

Firmware **7.1.0-rc.2**. Base URL: `http://192.168.4.1`, sulla rete Wi-Fi AP del dispositivo. Il portale usa richieste HTTP locali; non richiede Internet, account, WebSocket o risorse CDN. I POST ordinari sono `application/x-www-form-urlencoded`, non JSON. Upload JPEG e firmware usano multipart.

## Pagine

| Metodo / percorso | Contenuto |
|---|---|
| `GET /` | Portale stormDASH: Home, Trail, Arcade / AFTERDARK, WOW, Pit & setup |
| `GET /advanced` | Personalizzazione coordinata: Dispositivo, Luci, Preferenze; identità, luminosità, avvio e diagnostica |
| `GET /update` | Modulo OTA locale con token del dispositivo |

La revisione 7.1.0-rc.2 cambia presentazione e organizzazione del portale, mantenendo gli endpoint applicativi. Home legge lo stesso snapshot `/api/storm`: sessione e modalità TFT non sono valori inventati dal browser. I collegamenti tra Home, Trail, Arcade, WOW e Pit sono navigazione locale della pagina; non richiedono nuovi percorsi sul dispositivo.

HTML, CSS e JavaScript sorgenti sono in `web/`; `tools/build_portal.py` incorpora anche gli SVG di `assets/portal/` e produce `src/services/storm_page.h`, `output/stormDASH/portal.html` e `portal.js`. La pagina avanzata conserva form, ID e contratti API in `src/services/settings_page.h`.

Il portale è incorporato nell'applicazione e resta accessibile anche senza LittleFS. L'AP consente al massimo due client. Il firmware non autentica gli utenti con account: l'accesso dipende dalla connessione alla rete locale protetta dalla password del cruscotto. Il token OTA impedisce richieste prive del token corrente, ma non identifica l'autore del binario.

## Stato e comandi stormDASH

`GET /api/storm` restituisce uno snapshot dello stato. Campi:

| Campo | Significato |
|---|---|
| `mode` | 0 strumenti, 1 SHOW, 2 NIGHT STAGE, 3 Pit Check, 4 tabellone, 5 MIDNIGHT RUN, 6 LIVING COCKPIT, 7 LIQUID REACTOR, 8 GARAGE 3D, 9 BEAT STORM |
| `session`, `running` | Sessione aperta e timer in marcia; sono indipendenti dalla modalità TFT |
| `elapsed` | Tempo Trail accumulato in millisecondi |
| `gates`, `penalties`, `weight`, `score` | Inserimenti manuali, peso penalità e `penalties × weight − gates` |
| `gameMs`, `hits`, `paused`, `done` | Tempo di gioco, urti e stato NIGHT STAGE |
| `best`, `seed` | Miglior punteggio persistente e seed della partita |
| `history` | Numero risultati chiusi in archivio, massimo 64 |
| `wow` | Stato WOW: `ms`, `elapsed` (gara, 0–60000 ms), `score`, `best`, `hits`, `paused`, `done`, `audioActive`, `seed`, `exploded` (float 0–1), `bands` (quattro byte da bassi ad alti), `saved` (ultimo controllo NVS del record MIDNIGHT) |
| `saved` | Ultimo stato di disponibilità/verifica dell'archivio; false anche per un archivio nuovo non ancora scritto |

Il server invia `Cache-Control: no-store`. Dopo un comando, rileggere lo stato: l'accettazione in coda non certifica che l'azione sia già applicata o che il risultato sia già scritto in NVS.

`POST /api/storm` richiede `action` e accetta `value` numerico opzionale, intero senza segno a 32 bit. Valori negativi, stringhe non numeriche, overflow e azioni ignote producono **400** con `{"error":"invalid_command"}`. Successo: **202** `{"accepted":true}`; coda satura/non disponibile: **503** `{"error":"queue_full"}`.

| `action` | `value` | Effetto e condizioni |
|---|---|---|
| `start` | omesso | Apre una sessione se non è già aperta; azzera tempo e contatori |
| `pause` | omesso | Ferma il timer senza chiudere la sessione |
| `resume` | omesso | Riparte se la sessione è aperta |
| `finish` | omesso | Chiude una sessione aperta e accoda un risultato per il salvataggio |
| `gate` / `penalty` | omesso | Aggiunge un evento manuale se la sessione è aperta e ci sono meno di 128 eventi |
| `undo` | omesso | Toglie l'ultimo evento della sessione aperta |
| `rules` | 0–100 | Imposta il peso penalità solo senza sessione aperta |
| `show` | omesso | Avvia o riavvia SHOW di 6 secondi |
| `exit` | omesso | Ripristina gli strumenti; non chiude una sessione Trail |
| `game` | seed | Avvia o riavvia NIGHT STAGE; 0 o valore omesso vengono normalizzati a 1 |
| `jump` | omesso | Salta, se a terra; riprende il gioco in pausa |
| `heartbeat` | 0/omesso oppure 1 | Contatto del controller. MIDNIGHT: 0 rispetta pausa BOOT/background, 1 riprende esplicitamente. NIGHT STAGE mantiene il comportamento precedente |
| `background` | omesso | Mette in pausa NIGHT STAGE/MIDNIGHT e azzera subito BEAT quando si lascia la pagina |
| `pit` | omesso | Mostra il test visivo TFT e sedici punti |
| `board` | omesso | Mostra volontariamente il tabellone Trail |
| `midnight` | seed 32 bit | Corsa di 60 s; zero/omesso diventa 1, record distinto da NIGHT STAGE |
| `living` / `liquid` / `garage` / `beat` | omesso | Apre la modalità corrispondente |
| `steer` | 0, 1, 2 | MIDNIGHT: sinistra, neutro, destra; scade dopo 500 ms senza rinnovo |
| `touch` | 0–65535 | LIVING/LIQUID: coordinate normalizzate, `(x << 8) | y`, x e y 0–255 |
| `orbit` | 0–65535 | GARAGE: coordinate normalizzate con lo stesso formato |
| `explode` | 0 oppure 1 | GARAGE: ricomponi oppure separa, transizione interpolata |
| `impulse` | omesso | Azione BOOT contestuale della modalità WOW |
| `audio` | 0–4294967295 | BEAT: quattro bande 0–255, basso nel byte meno significativo; nessun dato audio grezzo |

Le precondizioni di dominio possono rendere un comando accettato privo di effetto: ad esempio `rules` durante una sessione, `gate` senza sessione o `jump` dopo l'arrivo. I client devono confermare il risultato con GET. Non ripetere automaticamente `gate`, `penalty` o `undo` dopo un timeout ambiguo: questi comandi non hanno una chiave di idempotenza e la ripetizione può duplicare un inserimento.

Esempio:

```sh
curl -d 'action=start' http://192.168.4.1/api/storm
curl -d 'action=gate' http://192.168.4.1/api/storm
curl http://192.168.4.1/api/storm
curl -d 'action=finish' http://192.168.4.1/api/storm
```

I controlli WOW ad alta frequenza usano una sola richiesta in corso, accorpano il dato successivo più recente e inviano al massimo circa 10 aggiornamenti/s. Lo spettro scade dopo 500 ms; `background` lo azzera immediatamente. Comandi inadatti alla modalità non hanno effetto. I valori non previsti (ad esempio `steer=3`, `orbit=65536`, `explode=2`) sono rifiutati con 400. Le azioni senza valore accettano solo zero/omesso.

Il timer Trail è locale e continua senza telefono, fino al limite di 24 ore. Il gioco usa passi di 20 ms e dura 45 secondi di simulazione; senza controllo locale va in pausa dopo 2,5 secondi senza contatto. Il valore `jump=1` nell'interfaccia interna è riservato all'uso BOOT per selezionare il controllo locale; il portale invia `jump` senza valore. I dati del gioco non alimentano la logica ESC.

## Storico ed esportazione

- `GET /api/storm/results` restituisce un array in ordine dal risultato più vecchio al più recente. Ogni oggetto espone `duration` in ms, `gates`, `penalties`, `penaltyWeight`, `score` e `test` (true per risultati di collaudo).
- `GET /api/storm/history` scarica `stormDASH-sessions.csv` con colonne `session,duration_ms,gates,penalties,penalty_weight,score,source`; `source` distingue `MANUAL` e `COMMISSIONING`.

Gli indici CSV sono posizioni nello storico corrente, non ID permanenti. L'archivio conserva i 64 risultati chiusi più recenti e il peso usato per ciascuno. Non c'è orologio calendario o rilevamento GPS: non vengono inventati data, posizione o identità del percorso reale. Il nome modello della card è un inserimento nel browser e non una colonna dello storico. La card PNG 1080×1920 viene composta e scaricata dal browser, senza upload esterno, usando esclusivamente un risultato chiuso restituito dall’archivio con il suo peso penalità. Il pulsante principale sceglie l’ultimo risultato manuale; i collaudi sono esportabili dalla loro riga e riportano una dicitura esplicita. Nessuna card viene creata da contatori iniziali o da una sessione in corso.

## Hardware e diagnostica

`GET /api/storm/hardware` espone:

```json
{
  "flashPhysical": 16777216,
  "flashConfigured": 16777216,
  "psramTotal": 8385847,
  "psramFree": 8366483,
  "heapFree": 154380,
  "heapMinimum": 152872,
  "partitions": [{"label":"app0","offset":65536,"size":3145728}]
}
```

I numeri RAM sono un esempio di campione, non valori garantiti. La risposta reale elenca tutte le partizioni. `flashPhysical` viene da `ESP.getFlashChipSize()`; la verifica di capacità fisica indipendente è la lettura Espressif archiviata. La PSRAM fisica è 8 MB: `psramTotal` è il valore esposto dal runtime, con overhead dell'allocatore.

`GET /api/v4/diagnostics` espone identità firmware, uptime, FPS e tempi render/push, heartbeat, stack, heap, PSRAM, LittleFS, AP, controllo RTL, PWM, reset ed errori. Queste misure provengono dal firmware e non sono misure ottiche del pannello. Il campo `memory.heapLargestBytes` misura il maggiore blocco contiguo allocabile in RAM interna con capacità `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`; la PSRAM è riportata nei campi separati. Nelle evidenze delle build di sviluppo precedenti questo campo poteva includere PSRAM: confrontare i campioni solo tenendo conto della revisione.

`GET /api/v4/capabilities` descrive le capacità di compatibilità; `/api/v2/capabilities` e `/api/v3/capabilities` sono alias. Per il prodotto stormDASH usare gli endpoint `/api/storm` e `/api/storm/hardware` per stato e memoria.

## Luci e ingresso PWM

`GET /api/v3/rtl/state` restituisce configurazione, funzione attiva, pixel teorici, driver e segnale: `escEnabled`, `escValid`, `escPulse`, `escType`, `escState`, `escNormalized`, `timedOut`. `hardwareReady` significa driver inizializzato, non barra fisicamente rilevata. Un segnale assente/non valido è distinto dal neutro.

`POST /api/v3/rtl/command` richiede `function`:

- `clear` o `running`: ritorno al controllo automatico;
- `brake`, `left`, `right`, `hazard`, `reverse_work`, `dust_chase`, `pit_test`: comando manuale luce, scadenza dopo 1,5 secondi senza rinnovo.

`pit_test` richiede ESC abilitato, segnale valido e neutro stabile per almeno 180 ms; altrimenti risponde **409** (`esc_required_for_pit` / `esc_not_neutral`). Questo test della barra è distinto dal Pit Check TFT (`action=pit`). Nessuno dei due può certificare visivamente l'hardware o che il modello sia fermo.

`POST /api/v3/rtl/config` richiede tutti i campi:

| Campo | Valori |
|---|---|
| `profile` | `rtl_s` / `ultra4_4400` |
| `brightness` | 8–255 |
| `escEnabled` | 0 / 1 |
| `escType` | `forward_brake_reverse` / `crawler_forward_reverse` / `forward_brake_only` |
| `escNeutralUs` | 1300–1700 |
| `escDeadbandUs` | 10–180 |
| `escReverseConfirmMs` | 100–2000 |

Configurazione non valida: **400** `invalid_rtl_config`; commit fallito: **500** `rtl_commit_failed`. La configurazione viene applicata dopo verifica del record NVS. La ricezione ammette 800–2200 µs e usa estremi nominali 1000/2000 µs: questi ultimi non sono campi di calibrazione API. Il PWM è esclusivamente un ingresso su GPIO5; non misura movimento o giri.

## Impostazioni e dati legacy compatibili

`GET /api/settings` restituisce le impostazioni, incluse riserve legacy. `POST /api/settings` aggiorna soltanto i campi presenti:

| Campo | Valori |
|---|---|
| `displayTheme` | 4 strumenti protetti, 5 allineamento; applica a entrambi i TFT |
| `driverTheme` / `navigatorTheme` | 4 / 5 per singolo TFT |
| `language` | 0 italiano, 1 inglese per l'interfaccia legacy compatibile |
| `driverBrightness` / `navigatorBrightness` | 32–255 |
| `units` | `metric` / `imperial` |
| `bootImageEnabled` | 1/0, true/false, on/off |
| `bootHoldMs` | 500–3500 |

Il nuovo portale stormDASH è in italiano; la preferenza legacy non costituisce una promessa di traduzione integrale delle nuove scene. `temperature=c|f` resta accettato per compatibilità. `photoModeEnabled=true` è rifiutato perché lo slideshow non è una modalità del prodotto; false resta accettato. Valore non valido o nessun campo riconosciuto: **400** `invalid_settings`. Commit fallito: **500** `settings_commit_failed` e ripristino del valore precedente.

`GET /api/v3/state` e `GET /api/v3/profile` restituiscono identità e dati simulati con `telemetrySource:"demo"`. `POST /api/v3/profile` aggiorna `number`, `vehicle` (alias `vehicleName`) e `driver`; esistono alias v2. Le chiavi e gli identificatori storici non vengono rinominati con il prodotto.

`GET /api/copilot-display` restituisce geometria, controller, offset e rotazione. `POST /api/copilot-display` accetta solo `profile=240x135_landscape`, salva e pianifica un riavvio dopo 900 ms. Profili quadrati: **400** `invalid_profile`; riavvio già pendente: **409**.

L'API compatibile `GET/POST /api/show` conserva `action=start|stop` e campi `ok,active,elapsedMs,durationMs,pending,requestedActive`. Per nuove integrazioni usare `POST /api/storm` con `show|exit`. Non ci sono endpoint di gioco Pong o listener WebSocket.

## Logo personale e media compatibili

`GET /api/photos` mantiene il catalogo e la diagnostica media: `fsReady`, spazio, logo A/B, transazioni, slot storici e `storageState` (`ready`, `degraded`, `unavailable`). `formattedOnBegin` segnala un'inizializzazione dopo verifica della memoria interamente vuota. Un errore di mount non viene mascherato.

Il logo personale usa una coppia JPEG dimensionata per i pannelli:

1. `POST /api/upload/boot-a?tx=ID`, multipart, 160×80.
2. `POST /api/upload/boot-b?tx=ID`, multipart, 240×135.
3. `POST /api/boot/commit` con lo stesso `tx=ID`.

ID esadecimale da 8–16 caratteri; A apre la transazione. B da sola o commit incompleto vengono rifiutati. `POST /api/boot/abort` annulla lo staging. `GET /api/media/boot?display=a|b` legge il JPEG; `POST /api/boot/delete` rimuove il logo. Il pilota applica la maschera e spegne i punti durante il logo personale.

Gli endpoint `media/photo`, `photos/delete`, `photos/commit`, `photos/abort`, `upload/photo-a|photo-b` restano per i dati preesistenti; non introducono uno slideshow. Non è esposto un comando web di formattazione forzata di LittleFS.

## OTA locale

`GET /update` fornisce il form con token casuale del dispositivo. `POST /api/storm/update?token=…` carica un singolo binario applicativo multipart. Sono richiesti flash da 16 MB, slot inattivo disponibile, nessuna sessione aperta e modalità strumenti. Il timeout dell'upload è 120 secondi; abort, scrittura fallita o immagine non valida non selezionano il nuovo slot.

Risposta **200** dopo verifica dell'immagine e pianificazione del riavvio, oppure **400** per upload rifiutato/incompleto. Dopo il riavvio il controllo di salute conferma l'immagine pending; il bootloader dispone del percorso rollback. Un 200 non è da solo prova dell'avvenuta conferma. [TESTING](TESTING.md) riporta il collaudo effettivo e [MIGRATION](MIGRATION.md) descrive le regioni, il backup e il recupero USB.


## Anteprima locale di sviluppo

`node tools/storm_preview_server.js --port 8767` serve l’interfaccia generata su `http://127.0.0.1:8767`, dopo `python3 tools/build_portal.py`. Il server ascolta solo su loopback e avvia il motore C++ del firmware in un processo locale temporaneo; non cerca né contatta l’ESP32. Le API delle periferiche sono fixture e la diagnostica dichiara l’assenza di hardware. `/advanced` usa le stesse fixture locali; `/update` mostra un avviso di funzione disponibile solo sul dispositivo. Le risposte dell’anteprima non sono evidenza di misura, salvataggio NVS o aggiornamento sulla scheda.
