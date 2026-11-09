# stormDASH / Industrial UI · 9 novembre 2026

Redesign delle cinque sezioni Home, Trail, Arcade, WOW, Pit & setup e della pagina di aggiornamento. Nero, bianco caldo, grafite e accenti rossi. Font a punti Storm Dot e illustrazioni SVG originali, incorporati nel firmware senza dipendenze di rete.

## Sorgenti e design system

- `web/portal.css`: token centrali, componenti, layout, navigazione desktop/mobile, feedback e reduced motion.
- `web/pit.css`: moduli di configurazione che ereditano gli stessi token.
- `web/update.css`: pagina di aggiornamento; stessa palette e tipografia.
- `web/portal.html`, `web/pit.html`, `web/update.html`: struttura e nomi accessibili.
- `web/presentation.js`: solo preferenza visuale chiaro/scuro. Segue il tema del dispositivo finché l’utente non sceglie; salva la scelta nel browser (`storm-theme`).
- `assets/portal/storm-dot.woff`: font originale a punti, circa2kB, per titoli e numeri grandi. Corpo testo con font di sistema; nessun caricamento esterno.
- `assets/portal/{instrument,track,eyes,field,wireframe,signal}.svg`: illustrazioni originali monocromatiche. Non rappresentano misure reali.

Usare `--bg`, `--surface`, `--surface-raised`, `--ink`, `--muted`, `--line`, `--control-line`, `--accent`, `--radius`, `--mono`, `--dot`. `--line` separa i pannelli; `--control-line` rende riconoscibili i campi. La modalità chiara ridefinisce gli stessi token. Non introdurre nuove palette per schermata.

Gli stili attivi precedenti sono stati sostituiti, non ricoperti da un altro tema. I vecchi file di recupero e strumenti storici non fanno parte dell’interfaccia servita. In particolare `tools/preview_portal.py` è un’anteprima storica: usare `output/stormDASH/portal.html` o gli screenshot del pacchetto per questo redesign.

## Funzioni conservate

`web/portal.js`, `web/pit.js`, tutti i file C++ e gli header applicativi (eccetto i due header HTML generati) sono identici alla base rc.7. Restano invariati API, input PWM, calibrazione, comandi ESC/luci, persistenza sul dispositivo, giochi, audio, schermate TFT e upload firmware. Non sono stati cambiati pin o partizioni. La sola nuova preferenza è il tema grafico locale al browser. Il form OTA mantiene azione, token, metodo POST, multipart, nome campo e vincolo .bin.

## Verifiche riproducibili

```
python3 tools/build_portal.py
python3 tools/build_portal.py --check
node --check web/portal.js
node --check web/pit.js
node --check web/presentation.js
node tools/test_storm_portal.js
node tools/test_unified_portal.js
STORM_WEBKIT=1 node tools/test_unified_portal.js
node tools/test_redesign.js
node tools/test_update_design.js
pio run -e esp32-s3-devkitc-1
```

I test browser richiedono Playwright e Chrome/WebKit installati. Eseguono API simulate e non inviano comandi al veicolo. Screenshot con dati di prova, non fotografie dello stato reale. Copertura visuale: 320,390,768,1440px, entrambi i temi; focus tastiera, nomi dei controlli, contrasti testuali, overflow, touch target e preferenza movimento ridotto. Le animazioni usano transizioni brevi;60FPS fisici non misurati sul telefono.

## Installazione

Il pacchetto contiene `firmware.bin` compilato per la configurazione ESP32-S3 N16R8 esistente. Questa revisione grafica è preparata ma non caricata sulla scheda durante questa consegna.

Per installarla: collega il telefono al Wi-Fi stormDASH, apri `http://192.168.4.1`, termina eventuali sessioni e torna a Strumenti. In Pit & setup scegli Aggiorna firmware e seleziona `firmware.bin`; attendi il riavvio e riapri la pagina. Il sistema di aggiornamento e le sue protezioni sono gli stessi della versione precedente. Foto e impostazioni non sono incluse né sovrascritte dal pacchetto applicativo.

La versione applicativa resta7.1.0-rc.7-esc-lights, poiché la logica non è stata revisionata. Per identificare questa consegna usare revisione UI `industrial-2026-11-09` e SHA256 nel manifest.
