# stormDASH — registro di verifica

Aggiornato il 15 ottobre 2026. Versione candidata alla consegna: 7.1.0-rc.2. I file in `evidence/` separano misure reali, test host e anteprime. Le prove del precedente RECOIL non sono conteggiate come prove del nuovo prodotto.

## Verifica del nuovo portale 7.1.0-rc.2

Questa revisione riprogetta il portale AP, le impostazioni avanzate e la pagina di aggiornamento locale. La navigazione principale comprende **Home, Trail, Arcade, WOW e Pit & setup**; l’area avanzata comprende **Dispositivo, Luci e Preferenze**. I renderer IQ3 e HDS-7 LIVE, la maschera pilota e i sedici punti restano invariati.

Il [report della consegna](../evidence/portal-design-verification.json) raccoglie gli esiti e i conteggi finali dei controlli software, le dimensioni della build e l’identificazione del binario. Il [report della pagina avanzata](../evidence/advanced-browser.json) documenta salvataggi, lingua, SHOW, rilascio dei comandi luci, allineamento della maschera, diagnostica, focus e layout a 320, 390 e 1120 pixel. Le schermate del portale sono in `output/stormDASH/portal-*.png`; le anteprime dichiarano l’assenza di hardware.

- **163/163 test nativi** superati.
- **34/34 verifiche del portale** in ambiente JavaScript controllato: richieste, timeout, home, passaggi tra sezioni, controller e audio.
- **17/17 prove in Chrome** con il motore C++ e Web Audio reali sul computer: Home, navigazione, cinque modalità WOW, sessione Trail, pausa/ripresa, stato offline, responsive, accessibilità e area avanzata simulata. Dettagli: `wow-browser.json`.
- **10/10 prove Chrome della pagina avanzata**: `advanced-browser.json`.
- **Compilazione N16R8 riuscita:** RAM statica 58.856 byte; utilizzo flash dell’applicazione riportato dalla build 1.183.897 su 3.145.728 byte. Dimensioni effettive dei file e SHA-256 sono nel manifest della consegna.

La verifica della UI comprende le sorgenti leggibili in `web/`, la generazione della pagina incorporata con `tools/build_portal.py`, i contratti delle API e i flussi di controllo già presenti. I test browser usano il motore C++ reale su computer e API periferiche simulate. Il banner di anteprima, le misure hardware non disponibili e il rifiuto delle operazioni OTA locali separano questo ambiente dal cruscotto reale.

Per rigenerare e verificare il portale:

```sh
python3 tools/build_portal.py
node tools/test_storm_portal.js
node tools/test_wow_browser.js
node tools/test_advanced_portal.js
pio run -e esp32-s3-devkitc-1
python3 tools/storm_flash.py --verify
```

I test browser richiedono Playwright e Chromium; `STORM_BROWSER_CHANNEL=chrome` seleziona Chrome installato. `tools/test_advanced_portal.js` avvia e chiude una propria fixture Python locale. Per l’esplorazione manuale, `node tools/storm_preview_server.js --port 8767` pubblica l’anteprima soltanto su loopback; richiede Node.js e un compilatore C++. Non scrive sulla scheda. La pagina avanzata è esplorabile con dati simulati; l’OTA richiede il dispositivo e nell’anteprima mostra un avviso.

**Installazione fisica del 15 ottobre 2026:** 7.1.0-rc.2 installata tramite il portale OTA. Lettura USB successiva: app0, sequenza 5, stato VALID con CRC corretto; tutti i 1.184.256 byte dell’applicazione corrispondono allo SHA-256 del pacchetto. Un ulteriore riavvio ha confermato versione, modalità strumenti, heartbeat dei due display, PSRAM e archivio disponibili, senza errori diagnostici. Le tre pagine servite dall’AP corrispondono alle sorgenti incorporate. Impostazioni, elenco foto/dimensioni e risultati sono confrontati prima/dopo e conservati. Le cinque modalità WOW sono state attivate brevemente, controllando stato e salute dei display; questo non costituisce una misura prolungata di FPS né una verifica ottica. Il Mac è stato riconnesso alla rete precedente e Internet verificato. Report: `installed-7.1.0-rc.2.json`.

Negli strumenti, il campione successivo all’OTA riporta 30/15 FPS e heap libero 151.596 byte. Sono contatori software di un campione breve. Restano il collaudo prolungato delle modalità, l’osservazione fisica completa e la riproduzione musicale sui telefoni effettivi. Le prove dettagliate della precedente 7.0 rimangono storiche.

## Verifica WOW precedente 7.1.0-rc.1

La consegna 7.1.0-rc.1 ha completato le cinque modalità WOW e preparato i binari con la scheda scollegata. Le verifiche seguenti si riferiscono a quella revisione e non dichiarano il collaudo fisico delle nuove modalità.

- **163/163 test nativi:** suite precedenti e 21 prove WOW/integrative. Comprendono corsa completa da 60 s, collisioni, record, seed, pausa e heartbeat in ritardo, timer wrap, limiti delle particelle per 120 s simulati, gaze/reazioni, rotazione/esploso, scadenza spettro e indipendenza Trail.
- **16/16 test motore sotto AddressSanitizer e UndefinedBehaviorSanitizer**, compilati anche in C++11 rigoroso senza avvisi: `wow-engine-sanitizers.txt`.
- **600 fotogrammi strumenti identici**, zero differenze rispetto alla copia approvata. Nuova esecuzione: `wow-instrument-check.txt`, dettagli hash in `instrument-pixel-comparison.json`.
- **10.600 coppie di fotogrammi WOW** controllate per clipping e aperture pilota, zero errori. Il controllo passa anche sotto i due sanitizzatori. Sono controllate anche **324 coppie di pose Garage** per evitare sovrapposizioni con le etichette. Cinque animazioni C++ da 10 s, con 120 campioni per clip (il GIF può accorpare fotogrammi identici), con ingressi telefono/audio dichiaratamente simulati: `wow-render.json` e `wow-render-sanitizers.txt`.
- **12/12 controlli del caricatore iniziale USB** e verifica geometria CAD. Nessuna scrittura USB. Il caricatore iniziale continua a rifiutare una scheda che usa già la selezione OTA: l’aggiornamento di quella dell’utente è applicativo, dal portale.
- Compilazione ESP32-S3 N16R8 riuscita, con partizioni e configurazione PSRAM mantenute; manifest SHA-256 e integrità ZIP verificati. Dimensioni e riepilogo della precedente consegna: `wow-verification-summary.json`. Il manifest corrente si riferisce alla 7.1.0-rc.2.

Per ripetere i controlli: `pio test -e native`, `node tools/test_storm_portal.js`, `python3 tools/check_wow_sanitizers.py`, `python3 tools/make_wow_preview.py`, `pio run -e esp32-s3-devkitc-1`. Il test browser `node tools/test_wow_browser.js` richiede Playwright e Chromium; `STORM_BROWSER_CHANNEL=chrome` seleziona Chrome installato. Avvia soltanto un server HTTP su loopback e un processo nativo temporaneo, senza accesso alla scheda. Le anteprime C++ richiedono Pillow e un compilatore C++17.

**Prove fisiche residue della 7.1 dopo l’installazione rc.2:** fluidità e tempi Wi-Fi nelle cinque nuove modalità, memoria/stack sotto carico, persistenza del record MIDNIGHT dopo riavvio, visualizzazione con la maschera montata e audio sui telefoni effettivi. Il dimensionamento software non sostituisce queste misure. I risultati fisici riportati nelle sezioni seguenti riguardano la precedente 7.0.0-rc.1.

## Baseline e strumenti

La baseline completa è stata copiata dal worktree `codex/recoil-tft-cornici`, inclusi file non tracciati e ignorati. Tutti i file inventariati dell’originale risultano invariati: `baseline-preservation.json`. Il repository stormDASH è separato.

`verify_instrument_pixels.py` compila separatamente il renderer C++ della copia RECOIL e quello stormDASH. Con la stessa simulazione deterministica confronta 300 fotogrammi per ciascuno dei due pannelli: **600/600 identici**, zero differenze. I file degli strumenti, le coordinate CAD e i driver restano quelli approvati. Il controllo CAD conferma sedici centri e finestra curva. Il renderer delle nuove modalità non scrive pixel fuori dalle superfici; il pilota utilizza il clipping della maschera originale.

Il proprietario ha confermato in chat di vedere correttamente i due strumenti sul cockpit reale. Non è una certificazione strumentale della luminosità o una misura ottica di FPS.

## Software e portale — precedente 7.0.0-rc.1

- **142/142 test nativi**: strumenti, segnali ESC, timeout, gesti BOOT, avvio, storage, diagnostica, sessioni, annullamento, wrap temporale, seed, ghost e integrità/storico limitato.
- **15/15 test del portale**: richieste, timeout senza replay automatico, disconnessione, campi, comandi gioco, card da archivio immutabile, provenienza dei risultati e calibrazione.
- **12/12 controlli del caricatore USB**: file modificati, indirizzi, layout sconosciuti, dati OTA presenti, backup incompleto o incoerente, memoria superiore occupata e controlli attivi anche con Python ottimizzato. Nessuna scrittura USB durante questi test.
- Compilazione PlatformIO ESP32-S3 con flash 16 MB, memoria `qio_opi` e PSRAM abilitata: riuscita.
- Layout verificato nel browser a **320 e 390 pixel**, nelle sezioni Trail, AFTERDARK e Pit con calibrazione espansa: nessun overflow orizzontale. Le sei immagini salvate documentano lo stato offline del server di anteprima, non fingono una connessione alla scheda.
- La card usa il peso originale del risultato archiviato; non viene creata da zeri iniziali o da una sessione in corso. I record di commissioning sono marcati COLLAUDO.

## Misure sulla scheda — precedente 7.0.0-rc.1

Identificazione USB: ESP32-S3 rev. 0.2, flash 16 MB, PSRAM embedded 8 MB AP_3v3. Il proprietario conferma WROOM-1 N16R8. A runtime: flash **16.777.216 byte**, pool PSRAM riportato dopo inizializzazione **8.385.847 byte**; la differenza dalla capacità fisica dipende dal gestore memoria. Prova di scrittura/lettura di 1 MB: PASS.

Il primo campione seriale di 32 secondi mostra circa **29–30 FPS pilota / 14–15 FPS copilota** negli strumenti, heap interno libero circa 154 KB, archivio pronto e nessun errore applicativo. Durante la partita completa i contatori riportano circa **30/30 FPS**. Sono contatori software, non misure del tempo luce-schermo.

Il test USB ha eseguito avvio, pausa, ripresa, gate, penalità, annullamento, chiusura e salvataggio; SHOW con uscita automatica; perdita controller dopo 2,5 secondi; ripresa con comando BOOT equivalente; partita completa di 45 secondi; Pit e ritorno agli strumenti. Storico e record sono stati riletti dopo reset. Questi risultati di prova non rappresentano un’uscita reale.

Sul Wi-Fi fisico sono riusciti: caricamento del portale, diagnostica, memoria, stato luci, flusso sessione, comandi SHOW, gioco/salto e pausa background. I tempi HTTP salvati sono tempi di risposta della richiesta sul Mac, non latenza completa tra dito e pixel. La riconnessione automatica del Mac dopo il reboot ha richiesto prove separate; non era un guasto del firmware.

Il collaudo alternato di **180 secondi** fra strumenti, SHOW, Pit e gioco ha raccolto 38 campioni senza errori segnalati. Heap libero: 149.804 byte al primo campione, 153.152 all’ultimo, minimo osservato 149.804. È una prova breve via USB, non una verifica di durata di una settimana o una misura sotto saturazione radio. [Risultati](../evidence/soak-summary.json).

## Aggiornamento e recupero — precedente 7.0.0-rc.1

La prima installazione USB ha aggiornato bootloader, tabella e app0 senza scrivere NVS o LittleFS. Le quattro descrizioni di partizione originali sono identiche byte per byte; app1 usa la parte superiore prima vuota. Il backup privato integrale di 16 MB è verificato con SHA-256.

OTA applicativo ricevuto, avviato nello slot inattivo e marcato VALID: provato sulla scheda. Il test negativo ha individuato una conferma anticipata nel framework Arduino: il firmware ora sovrascrive `verifyRollbackLater()` con linkage C e restituisce true, lasciando la conferma ai controlli di salute dopo 10 secondi. Il reset forzato prima della conferma ha prodotto sequenza 4 `ABORTED`, mantenendo la precedente sequenza 3 `VALID`: rollback PASS. Lo script di quel test riporta un errore complessivo per la riconnessione anticipata del Mac; la prova di selezione avvio è distinta ed esplicita. L’OTA successivo ha installato e confermato app1, sequenza 4 `VALID`, con portale raggiungibile, archivio e foto conservati: `ota-final.json`.

Nella consegna 7.0.0-rc.1 del 15 ottobre era stata completata una sola rifinitura successiva alle prove fisiche: nome, palette, collegamento al portale e testo di riconnessione nella pagina avanzata. Il relativo binario finale era compilato e verificato nel pacchetto 7.0; la scheda è stata scollegata e il proprietario ha rimandato il collegamento. Quella rifinitura non è stata installata. `installed-build.json` conserva l’hash della build collaudata; `verification-summary.json` identifica il binario della consegna 7.0; il manifest attuale identifica invece la nuova 7.1.0-rc.2.

## Limiti delle prove

Non sono stati simulati cali di tensione durante scrittura, disturbi elettrici, temperatura estrema o una settimana di utilizzo. Non è stata cancellata la flash per far riuscire un test. Il ripristino completo dal backup è documentato e verificato per integrità/regioni, ma non eseguito perché sovrascriverebbe il prodotto appena installato.

La barra WS2812 separata non è stata confermata fisicamente. La verifica software del suo driver non ne prova la presenza o l’accensione. I sedici punti ottici sono distinti dalla barra. Il PWM e la calibrazione hanno test logici; manca la verifica con radiocomando/ESC reale e strumentazione sui fronti. Nessun output motore o servo è implementato. GPS, IMU, velocità, RPM, temperature e tensione reali richiedono sensori aggiuntivi; i dati degli strumenti restano DEMO.
