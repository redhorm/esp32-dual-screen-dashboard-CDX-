# API locale — protocollo 4

Base URL quando si è collegati all’access point:

```text
http://192.168.4.1
```

Versione firmware documentata: `5.0.0-beta.1`. Le API sono locali e offline.
Le risposte dinamiche principali usano `Cache-Control: no-store`.

## Convenzioni

- `GET`: parametri in query string.
- `POST`: `application/x-www-form-urlencoded`, salvo gli upload media che
  usano `multipart/form-data` con campo `image`.
- Risposta normale: JSON con `"ok": true`.
- Input non valido: HTTP 400.
- Conflitto di sessione/transazione: HTTP 409.
- Immagine non valida: HTTP 415.
- Media troppo grande: HTTP 413.
- LittleFS non disponibile: HTTP 503.
- Spazio insufficiente: HTTP 507.

Esempio:

```bash
curl http://192.168.4.1/api/v4/capabilities
```

## Capacità

### `GET /api/v4/capabilities`

Restituisce versione, protocollo, feature e frequenze nominali:

```json
{
  "ok": true,
  "protocol": 4,
  "version": "5.0.0-beta.1",
  "features": [
    "trailAssault", "seededTracks", "fixedStep60Hz", "ghostReplay",
    "analogController", "webAudioEvents", "diagnostics", "rtlController",
    "escStateMachine", "realismDash", "stockOffroadDash", "bilingualUi",
    "photoStorageV4", "mediaPreview", "uniqueApCredentials",
    "rtlOutputWatchdog"
  ],
  "maxClients": 2,
  "stateHz": 10,
  "inputHz": 20,
  "physicsHz": 60,
  "inputTimeoutMs": 300
}
```

Alias compatibili: `/api/v2/capabilities` e `/api/v3/capabilities`. Per non
rompere i client beta esistenti, questi riportano `protocol: 3`, `stateHz: 5`
e `inputHz: 10`; le feature elencate restano quelle del firmware corrente.

## Impostazioni e profilo

### `GET /api/settings`

Campi principali:

| Campo | Tipo / valori |
|---|---|
| `displayTheme` | `0` Classic, `1` Realism, `2` Stock Offroad |
| `driverTheme`, `navigatorTheme` | duplicati compatibili del tema corrente |
| `language` | `0` italiano, `1` inglese |
| `setupComplete` | boolean |
| `useKmh`, `useCelsius` | boolean |
| `driverBrightness`, `navigatorBrightness` | 32–255 |
| `bootImageEnabled` | boolean |
| `bootHoldMs` | 500–3500 |
| `photoModeEnabled` | boolean |
| `photoTarget` | bit 0 pilota, bit 1 copilota; `3` entrambi |
| `photoIntervalS` | 3–300 |
| `photoTransition` | `0` taglio, `1` dissolvenza |
| `photoRandom`, `photoOverlay` | boolean |

### `POST /api/settings`

Campi obbligatori:

```text
displayTheme, units(metric|imperial), temperature(c|f),
driverBrightness, navigatorBrightness
```

In alternativa a `displayTheme` sono accettati insieme `driverTheme` e
`navigatorTheme`. L'endpoint salva uno **snapshot completo**, non una patch:
il portale invia sempre anche `bootImageEnabled`, `bootHoldMs`,
`photoModeEnabled`, `photoTarget`, `photoIntervalS`, `photoTransition`,
`photoRandom` e `photoOverlay`. Se omessi, i checkbox diventano falsi e gli
altri campi MEDIA ricadono nei valori minimi/default determinati dal parser.
Solo `language`, se omesso, conserva il valore corrente.

### `GET /api/v3/profile`

Restituisce il documento di stato compatibile protocollo 3, inclusa
`identity`. Per lo stato ricco protocollo 4 usare `/api/v4/game/state`.

### `POST /api/v3/profile`

```text
number=4400&vehicle=My+Rig&driver=Mario
```

`vehicleName` è accettato come alias di `vehicle`. Tutti e tre i valori devono
essere presenti e non vuoti. Alias v2 disponibili.

## Trail Assault

### Flusso consigliato

1. configura modalità e seed;
2. avvia con una sessione casuale non zero;
3. invia input ogni 50 ms con sequenza crescente;
4. leggi lo stato ogni 100 ms;
5. invia `neutral` quando il controller perde focus;
6. invia `quit` quando si esce.

### `POST /api/v4/game/config`

| Campo | Obbligo | Valori |
|---|---|---|
| `mode` | no | `time_attack`, `gate_rush`, `hill_climb` |
| `seed` | no | uint32 decimale, senza prefisso `0x` |

Configura il percorso ed entra nello stato `attract`. Modalità non riconosciuta
restituisce `invalid_mode`; seed non numerico restituisce `invalid_seed`.

### `POST /api/v4/game/action`

| Campo | Valori |
|---|---|
| `action` | `enter`, `start`, `restart`, `pause`, `resume`, `exit`, `quit` |
| `session` | uint32 non zero, consigliato per `start`/`restart` |

Senza `session`, il firmware ne genera una casuale. La risposta è lo stato
completo corrente.

### `POST /api/v4/game/input`

Campi obbligatori:

| Campo | Intervallo | Note |
|---|---:|---|
| `session` | uint32 non zero | deve coincidere con la partita |
| `seq` | uint16 | crescente con confronto wrap-safe |
| `steer` | −100…100 | sterzo analogico |
| `throttle` | 0…100 | acceleratore analogico |
| `brake` | 0…100 | freno analogico |

`clientMs` uint32 è opzionale e permette una stima di latenza relativa. Valori
fuori intervallo vengono rifiutati con HTTP 400 e `invalid_input`.

Risposta accettata, HTTP 200:

```json
{
  "ok": true,
  "accepted": true,
  "seq": 42,
  "serverMs": 123456,
  "result": "accepted",
  "inputRttMs": 8
}
```

Risposta rifiutata, HTTP 409, con `result` uguale a `wrong_session`,
`stale_sequence` o `inactive`.

### `POST /api/v4/game/neutral`

Accetta `session` e `seq`; se omessi usa sessione corrente e sequenza
successiva. Imposta subito sterzo, gas e freno a zero. Può restituire 409 se la
sessione o la sequenza non sono valide.

### `GET /api/v4/game/state?audioAfter=N`

`audioAfter` è una sequenza uint16. La risposta include solo gli eventi audio
successivi ancora presenti nel ring buffer.

Campi scalari:

| Gruppo | Campi |
|---|---|
| Identità | `version`, `protocol`, `serverMs`, `experienceMode` |
| Gara | `status`, `mode`, `seed`, `timeMs`, `remainingMs`, `courseClear` |
| Controller | `ackSeq`, `controllerAlive`, `inputRttMs`, `steer`, `throttle`, `brake` |
| Veicolo | `rpm`, `speed`, `damage`, `tempC`, `grip`, `terrain` |
| Progresso | `score`, checkpoint/gate correnti e totali, `progress` |
| Distanza | `distanceCm`, `courseLengthCm`, `worldDistance`, `lineX`, `x` |
| Evento | `event`, `eventAgeMs`, `phaseMs` |
| Ghost | `bestMs`, `deltaMs` |
| Profilo | `identity`, `vehicle`, `bestScore`, `wins` |

Valori `status`: `idle`, `attract`, `countdown`, `running`, `paused`,
`finished`, `failed`.

`speed` è espresso in km/h con un decimale. `damage`, `grip` e `progress` sono
in millesimi, da 0 a 1000. `eventAgeMs=65535` indica che non c’è un evento
valido da temporizzare.

Array:

```json
{
  "audio": [
    {"seq": 8, "type": "checkpoint", "value": 2,
     "intensity": 240, "tick": 990}
  ],
  "objects": [
    {"distanceDm": 230, "lateral": -25, "radius": 20,
     "kind": "boulder"}
  ],
  "notes": [
    {"distanceM": 90, "turn": -4, "grade": 3,
     "terrain": "rock", "flags": 1}
  ]
}
```

Tipi oggetto: `none`, `boulder`, `log`, `rut`, `gate`, `jump_ramp`.
Terreni: `dirt`, `rock`, `mud`, `sand`, `water`, `jump`.

Eventi gioco: `none`, `checkpoint`, `gate_clear`, `gate_missed`,
`collision_small`, `collision_large`, `terrain_changed`, `jump`, `landing`,
`controller_timeout`, `overheat`, `finish`.

Una risposta contiene al massimo 16 eventi audio, 4 oggetti e 2 note. Questi
sono limiti del contratto embedded, non pagine da richiedere successivamente.

Eventi audio possibili: `engine_start`, `engine_rpm`, `gear_change`, `skid`,
`collision_small`, `collision_large`, `mud`, `water`, `checkpoint`,
`countdown`, `finish`, `warning`, `pause`.

### Compatibilità v2/v3

`GET /api/v2/state` e `GET /api/v3/state` restituiscono il serializer legacy
protocollo 3 (`mode`, `identity`, `vehicle` e oggetto `game` annidato).
Sono mantenuti temporaneamente gli adapter digitali:

- `POST /api/v2/game/start`;
- `POST /api/v2/game/input` con `steer` e `throttle` digitale;
- `POST /api/v2/game/action`.

Le nuove integrazioni devono usare `/api/v4/game/*`.

## Diagnostica

### `GET /api/v4/diagnostics`

Struttura:

| Oggetto | Campi |
|---|---|
| radice | `firmware`, `protocol`, `uptimeMs`, `lastError`, `restartCount`, `resetReason`, `resetReasonCode` |
| `displayA`, `displayB` | `ok`, età heartbeat, stack libero, `fps`, `frames`, tempi medio/massimo di frame, render e push in µs |
| `memory` | heap libero/minimo/blocco maggiore, PSRAM presente/libera/totale |
| `storage` | ready, totale/usato/libero, asset sistema, foto utente, asset gioco, transazioni |
| `wifi` | `online`, `clients` |
| `input` | ricevuti, accettati, persi, stale, timeout, latenza ultima/media/massima, controller vivo |
| `rtl` | `ok`, task controllo vivo, età heartbeat e stack libero |
| `esc` | `enabled`, `valid`, `pulseUs` |

Nomi wire esatti dei tempi display:

```text
heartbeatAgeMs, stackFreeBytes, frameAvgUs, frameMaxUs,
renderAvgUs, renderMaxUs, pushAvgUs, pushMaxUs
```

Su N8 `psramPresent=false` e i byte PSRAM a zero sono corretti.

## Media

### `GET /api/photos`

Restituisce slot, boot, stato filesystem e limiti. Campi utili:

```text
schemaVersion, slots, maxSlots, bootDriver, bootNavigator,
usedBytes, totalBytes, freeBytes, fsReady, userPhotoBytes,
transactionBytes, filesystemOverheadBytes, catalogueGeneration,
storageState, partitionLabel, recoveredOnBegin, degraded,
uploadActive, lastError, driverMaxBytes, navigatorMaxBytes, reserveBytes
```

`systemAssetBytes` e `gameAssetBytes` valgono zero perché quegli asset sono nel
firmware.

### Lettura anteprime

- `GET /api/media/boot?display=a|b`
- `GET /api/media/photo?slot=0..23&display=a|b`

Risposta `image/jpeg`; 404 se l’asset non esiste. Durante `countdown` o
`running`, la lettura anteprime restituisce HTTP 409 `game_active` per non
contendere LittleFS e decode ai task real-time.

### Transazione upload

`tx` deve contenere 8–16 caratteri esadecimali. Per una foto:

1. `POST /api/upload/photo-a?slot=S&tx=TX` — multipart `image`;
2. `POST /api/upload/photo-b?slot=S&tx=TX` — multipart `image`;
3. `POST /api/photos/commit` con `slot=S&tx=TX`.

Per l’immagine boot:

1. `POST /api/upload/boot-a?tx=TX`;
2. `POST /api/upload/boot-b?tx=TX`;
3. `POST /api/boot/commit` con `tx=TX`.

Se un passaggio fallisce:

- `POST /api/photos/abort` con `slot` e `tx`;
- `POST /api/boot/abort` con `tx`.

La variante B non è accettata prima della A. Il commit richiede entrambe le
varianti strutturalmente valide, con JPEG baseline e dimensioni attese. Una
transazione inattiva scade dopo 30 s.

Eliminazione:

- `POST /api/photos/delete` con `slot`;
- `POST /api/boot/delete`.

Errori media JSON: `invalid_slot`, `invalid_transaction`, `upload_busy`,
`filesystem_unavailable`, `no_space`, `image_too_large`, `invalid_image`,
`game_active`, `write_failed`, `commit_failed`, `interrupted`. Upload, commit
ed eliminazione restituiscono HTTP 409 `game_active` durante countdown/gara.

## RTL ed ESC

### `GET /api/v3/rtl/state`

Restituisce profilo, funzione, luminosità, stato hardware/watchdog, ESC e array
di 16 pixel RGB.

### `POST /api/v3/rtl/config`

| Campo | Valori |
|---|---|
| `profile` | `ultra4_4400`, `rtl_s` |
| `brightness` | 8–255 |
| `escEnabled` | `0`, `1` |
| `escType` | `forward_brake_reverse`, `crawler_forward_reverse`, `forward_brake_only` |
| `escNeutralUs` | 1300–1700 |
| `escDeadbandUs` | 10–180 |
| `escReverseConfirmMs` | 100–2000 |

### `POST /api/v3/rtl/command`

Campo `function`:

```text
running | clear | brake | left | right | hazard |
reverse_work | dust_chase | pit_test
```

`pit_test` restituisce HTTP 409 `esc_required_for_pit` finché l'ingresso ESC
non è abilitato. Richiede inoltre segnale valido e stato neutro stabile per
almeno 180 ms, altrimenti restituisce `esc_not_neutral`. Non esiste fallback
sulla velocità simulata. I comandi manuali devono essere rinnovati prima del
watchdog di 1,5 s.

## Captive portal

Questi probe restituiscono la pagina principale per aprire il portale su più
sistemi:

- `/generate_204` — Android;
- `/hotspot-detect.html` — Apple;
- `/connecttest.txt` — Windows.

Qualsiasi percorso non API sconosciuto torna al portale. Un percorso `/api/*`
sconosciuto restituisce JSON 404.
