# stormDASH — requisiti e riscontri

Versione di consegna **7.0.0-rc.1**. Questa matrice collega il brief originale alle funzioni e alle verifiche disponibili. **Implementato** indica codice o artefatto presente; **verificato** indica una prova esplicita con il relativo perimetro. I test software non certificano resa ottica, componenti non collegati o affidabilità di una settimana.

| Area del brief | Consegna / decisione | Verifica disponibile e limite residuo |
|---|---|---|
| Baseline corretta e preservazione | Copia isolata del worktree RECOIL PULSE 6.0.0-rc.2 con stato non committato; snapshot completo e inventario separati in `stormDASH-preservation` | Copia approvata conservata, distinta dal vecchio progetto principale; nessuna rinomina dei dati legacy per fini estetici |
| Grafica strumenti identica | Renderer IQ3 e HDS-7 LIVE, font, mappa, maschera e sedici punti preservati | [600/600 fotogrammi identici](../evidence/instrument-pixel-comparison.json); [CAD coincidente](../evidence/cad.txt); proprietario conferma strumenti visibili correttamente |
| Identificazione hardware | ESP32-S3 rev. 0.2; modulo WROOM-1 N16R8 confermato dal proprietario; flash quad 16 MB; PSRAM octal 8 MB 3,3 V | [Identificazione Espressif](../evidence/flash-id.txt), [runtime e prova PSRAM 1 MB](../evidence/runtime-reboot.txt); non inferito dalla vecchia configurazione |
| Memoria configurata e sfruttata | Profilo 16 MB / `qio_opi`; ghost in PSRAM; due slot applicazione; riserva 5 MB | Memoria reale riconosciuta e testata; framebuffer interni intenzionali per compatibilità trasferimenti; riserva non spacciata per archivio già usato |
| Cockpit e pin | TFT 160×80 mascherato e 240×135 landscape; pin, offset e rotazioni invariati; PWM GPIO5 ingresso, WS2812 GPIO4 opzionali | Verifica CAD e input software; presenza fisica della barra non confermata. `hardwareReady` non è rilevamento di una barra |
| Provenienza dei dati | Strumenti/SHOW DEMO; Trail manuale; PWM reale distinto | Etichette nel portale e scene; nessun GPS, IMU, sensore RPM/tensione/temperatura o uscita motore aggiunti |
| Identità stormDASH // AFTERDARK | Nome prodotto, simbolo tempesta/ruote, firma completa SVG, logo pixel, palette nuova quasi nera e viola | [Asset originali](../assets/stormDASH-logo.svg), renderer C++ e portale; strumenti protetti non ricolorati |
| Portale mobile offline | Trail, AFTERDARK, Pit & luci; comandi principali diretti; avanzate secondarie; guida connessione | Anteprime mobili 320/390 px; API e pagina provate su AP reale; il completamento di un test browser simulato non viene confuso con test radio |
| Trail Session | Timer su ESP32, pausa/ripresa, gate, penalità, undo, peso 0–100, tabellone volontario, 64 risultati, CSV e card locale da risultati chiusi, collaudi etichettati | Flusso e persistenza dopo reset verificati sulla scheda; massimo 128 eventi / 24 ore; sessione non chiusa interrotta da reboot; nessun gate automatico o conformità universale dichiarata |
| Luci e segnale | Neutro/deadband configurabili, segnale valido/assente, tre tipi ESC, preset barra, prova visiva e ripristino parametri | Test logica ESC, timeout e API; estremi nominali fissi 1000/2000 µs. Collaudo con ricevente/ESC e barra fisici da completare |
| Pit Check | Pattern TFT e sedici punti, ritorno strumenti, stato PWM/rete/memoria, partizioni e diagnostica | Renderer e ripristino verificati; l'utente deve osservare pannelli, luminosità e aperture. Il test software non certifica un pannello sano |
| Passaggio tra schermi | Buggy condiviso, tempi coerenti, portali intenzionali; ciclo breve 3 s riusato in SHOW 6 s e arrivo gioco | [Animazione C++](../output/stormDASH/PORTAL_LINK.gif), SHOW reale avvio/fine verificati. La separazione millimetrica tra superfici non è misurata |
| NIGHT STAGE | Gioco laterale 45 s sull'ESP32: salto, ostacoli, urti, difficoltà crescente, seed, record, restart, parallax, polvere, ghost | Partita completa eseguita sulla scheda; test determinismo e timeout; [anteprima](../output/stormDASH/NIGHT_STAGE.gif). Ghost volatile per seed corrente; record persistente |
| Comandi e disconnessione | BOOT breve contestuale; BOOT lungo esce/credenziali; heartbeat web e pausa background | Test pulsante e perdita controller; percorso game jump/background via Wi-Fi verificato. L'accesso al gioco non è condizionato a un presunto arresto ricavato dal PWM |
| Architettura e prestazioni | Proprietario unico display, core controllo separato, code/snapshot, niente allocazioni per frame, I/O lento fuori dal rendering | Misure software reali circa 29–30/14–15 FPS strumenti e 30/30 gioco. Latenza comando→fotone, tearing e prova estesa concorrente non sono certificati |
| Migrazione dati | NVS e LittleFS alle stesse posizioni, backup completo recuperabile, schema archivio 3 separato, migrazione dello schema di sviluppo, verifica dei commit | Backup flash 16 MB, regioni inventariate, prima installazione eseguita preservando NVS/FS; archivio e record riletti dopo riavvio |
| Aggiornamenti | OTA slot inattivo, verifica immagine, controllo salute e conferma; recupero USB documentato | OTA finale 7.0.0-rc.1 PASS: app1 seq. 4 `VALID`, portale raggiungibile, storico/record/media conservati. Rollback provocato PASS in una prova precedente: reset a 5 s, seq. 3 `VALID` e candidato seq. 4 `ABORTED`. Override di conferma differita verificato nell'ELF. Power-cut durante scrittura ancora da provare; nessuna firma autore configurata |
| Ricerca mirata | Espressif per modulo/memoria/OTA; UKRCCA per limiti regolamenti; LANE Boys per comportamento ESC; Rockstar per atmosfera; Meta per ipotesi Reel | [Fonti e decisioni](RESEARCH.md); nessun asset Rockstar o promessa di viralità |
| Consegna | Firmware, portale incorporato, logo, scene/animazioni, binari con manifest, guide, matrice e storyboard | [README](../README.md), [binari](../output/stormDASH/BINARI), [Reel 15 s con tre aperture](REEL.md). Dettaglio finale prove e revisione in [TESTING](TESTING.md) |

La prova alternata di 180 secondi è riuscita senza errori segnalati; non sostituisce una prova estesa concorrente. Il pacchetto del 15 ottobre include una rifinitura cosmetica della pagina avanzata successiva alle prove fisiche: compilata, ancora da caricare quando il proprietario ricollegherà la scheda. I due hash sono distinti nel riepilogo delle verifiche.

## Limiti operativi espliciti

Il prodotto è utile con timer, contatori manuali, display e controller web già presenti. Richiederebbero hardware ulteriore: velocità e distanza reali, posizione/cartografia GPS, RPM misurati, tensione e temperature reali, assetto IMU e rilevamento automatico dei gate. Queste espansioni non sono presentate come funzionalità attuali.

Lo storico limitato e gli sprite procedurali riducono scritture, complessità e carico senza consumare memoria per principio. Non è implementato un archivio multimediale da 5 MB nel nuovo `stormdata`, né ghost persistente, audio sul cockpit, doppio buffering grafico asincrono o aggiornamenti firmati. La PSRAM verificata è sfruttata dove il codice attuale ne trae un beneficio concreto; non impone lo spostamento dei buffer sensibili al trasferimento.

Restano prove fisiche specifiche: comportamento con ESC/ricevente reali e con barra installata, verifica ottica completa al montaggio finale, latenza percepita sul telefono scelto, lunga durata con carichi concorrenti, interruzioni controllate dell'alimentazione durante la scrittura. Il rollback tramite reset prima della conferma è già verificato. La conferma degli strumenti da parte del proprietario è acquisita; non viene estesa automaticamente a tutti questi scenari.


## Cinque modalità WOW — 7.1.0-rc.1

| Richiesta | Realizzazione | Verifica |
|---|---|---|
| MIDNIGHT RUN | Strada pseudo-3D, curve, colline, galleria, tramonto; accelerazione automatica, sterzo telefono, urti, gara 60 s e record | Motore deterministico, partita completa, timeout, comandi browser e renderer |
| LIVING COCKPIT | Occhi animati, sguardo dal dito, lampeggio, reazioni gate/penalità/chiusura/record NIGHT STAGE; battito pilota | Eventi e gesti nel motore, browser e anteprime C++ |
| LIQUID REACTOR | 48 particelle interattive con coesione/repulsione, onda BOOT e vasca pilota coordinata | Simulazione limitata e ripetibile, stress bounds, touch browser |
| GARAGE 3D | Buggy geometrico originale, rotazione, componenti esplosi/ricomposti, seconda vista pilota | Trasformazioni e limiti, browser, clipping CAD |
| BEAT STORM | Audio locale sul telefono, analisi quattro bande, forme e spettro sui TFT, sedici punti ottici | Vero Web Audio in Chromium con WAV originale di prova, fine/stop/background e file invalido |
| Strumenti identici | Stessi renderer originali, uscita volontaria da WOW | Confronto 600 fotogrammi, zero differenze |

Le cinque modalità sono implementate e compilate per N16R8. La scheda è scollegata: caricamento, FPS reali, resa sui TFT e compatibilità audio sui telefoni effettivi restano da collaudare. Le animazioni non richiedono IMU, GPS, microfono o altoparlante sulla scheda. LIQUID è una simulazione visuale 2D originale, GARAGE un modello stilizzato; non rappresentano misure fisiche né il CAD esatto del veicolo.
