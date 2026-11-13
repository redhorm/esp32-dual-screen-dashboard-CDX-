# stormDASH · 7.1.0-rc.9-audio

Dashboard RC per **ESP32-S3 N16R8**, display pilota ST7735 160×80 e copilota ST7789 240×135. Portale locale con temi chiaro/scuro, navigazione unica e controlli separati per strumenti ESC e luci.

**[Scarica firmware e pacchetto completo](https://github.com/redhorm/esp32-dual-screen-dashboard-CDX-/releases/tag/firmware-7.1.0-rc.9-audio)** · [Modifiche e installazione](README_AUDIO.md) · [Risultati dei test](docs/validation/firmware-7.1.0-rc.9-audio)

![Home, tema scuro](docs/screenshots/home-dark-1440.png)

## Aggiornamento del 13 novembre 2026

Il nuovo pannello Suono motore riproduce un V8 dal telefono seguendo gas, cambiate e limitatore simulati dal dashboard.

Pin, calibrazione, interpretazione crawler, grafica TFT, partizioni e salvataggi restano quelli della base Industrial UI. ESC e luci condividono il segnale PWM ma hanno controlli indipendenti. Il dashboard legge il segnale: non comanda motore o servo. RPM, velocità e marce sono scenografici.

## Suono motore dal telefono

In **Pit & setup → Suono motore**, attiva gli strumenti ESC e premi **Avvia motore**. Il suono V8 segue RPM, cambiate e limitatore simulati dal cruscotto. Volume iniziale 25%; regola anche il volume multimediale del telefono. Tieni questa pagina in primo piano: il suono si ferma se la lasci, se blocchi il telefono o se manca il PWM/collegamento. Per riprendere, premi nuovamente Avvia. [Istruzioni complete](README_AUDIO.md).

## Funzioni

- **Home:** panoramica cockpit e accessi rapidi.
- **Trail:** sessioni, tempo, gate, penalità e risultati.
- **Arcade:** gioco e comandi sui TFT.
- **WOW:** cinque esperienze interattive, inclusa riproduzione audio dal telefono.
- **Pit & setup:** strumenti ESC, calibrazione, suono motore, luci, display e diagnostica.

## Installazione

1. Scarica `stormDASH-7.1.0-rc.9-audio.bin` dalla release: applicazione per la configurazione ESP32-S3 N16R8 esistente, non un dump completo.
2. Collegati al Wi-Fi del dashboard e apri `http://192.168.4.1`.
3. Termina sessioni/effetti, torna a Strumenti e apri **Pit & setup → Aggiorna firmware**.
4. Seleziona il file, attendi il riavvio e ricollegati. In **Hardware e diagnostica** verifica `7.1.0-rc.9-audio`.

Impostazioni e foto restano gestite dal sistema OTA esistente. Non è prevista la cancellazione completa della flash. Firmware caricato sulla scheda e versione verificata dopo il riavvio: entrambi i display, archivio e Wi-Fi operativi. La prova audio con telefono e ricevitore resta da completare.

## Compilazione e verifiche

Richiede Python 3, Node.js 18+ e PlatformIO. Le dipendenze sono fissate in `platformio.ini` e `package-lock.json`.

```sh
npm ci
python3 tools/build_portal.py
python3 tools/build_portal.py --check
pio run -e esp32-s3-devkitc-1
pio test -e native
npm test
npm run test:network
npx playwright install chrome webkit
npm run test:browser
npm run test:webkit
npm run test:engine
npm run test:engine:webkit
python3 tools/test_portal_domains.py
```

Esito: **183/183 test nativi**, **36/36 test JavaScript**, integrazione Chrome/WebKit, 40 stati responsive/tema, 8 stati OTA e controlli C++ con AddressSanitizer/UndefinedBehaviorSanitizer. I test browser usano API simulate e non comandano la scheda. Nessuna misurazione di FPS fisici è inclusa.

Il binario compilato è `.pio/build/esp32-s3-devkitc-1/firmware.bin`. La pagina web è incorporata e funziona senza CDN.

## Struttura

| Percorso | Contenuto |
|---|---|
| `src/`, `include/` | Firmware e header HTML generati |
| `web/`, `assets/portal/` | Sorgenti del portale e asset locali |
| `tools/`, `test/` | Generazione e verifiche |
| `docs/` | Documentazione, schermate e log |
| `references/recoil/` | Coordinate delle luci RPM |
| `output/stormDASH/` | Anteprima generata, richiede le API del dispositivo |
| `releases/firmware-7.1.0-rc.9-audio/` | Manifest e checksum degli allegati |

[Versione Industrial UI precedente](https://github.com/redhorm/esp32-dual-screen-dashboard-CDX-/releases/tag/industrial-2026-11-09). Il firmware rimane Arduino/C++: CircuitPython 11 alpha richiederebbe una migrazione, non un aggiornamento diretto. Il lavoro CAD è fermo e non fa parte di questa release.
