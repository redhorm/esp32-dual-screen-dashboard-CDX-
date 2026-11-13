# stormDASH 7.1.0-rc.9-audio

## Uso

1. Collega il telefono al Wi-Fi del dashboard e apri `http://192.168.4.1`.
2. Apri **Pit & setup**. Nella sezione ESC abilita gli strumenti e salva; verifica che il segnale PWM sia presente. Se necessario premi **Mostra strumenti sul dashboard**.
3. Scorri fino a **Suono motore** e premi **Avvia motore**. Il suono parte soltanto dopo questo tocco e dopo una lettura valida del gas.
4. Regola **Volume motore** (inizialmente 25%) e il volume multimediale del telefono.
5. Mantieni Pit & setup in primo piano e il telefono sbloccato. Premi **Spegni** per fermare l'audio.

Il sintetizzatore V8 riproduce avviamento, minimo, accelerazione, taglio durante le cambiate e limitatore. Segue gli stessi RPM e marce simulati dei display, letti dal PWM: non misura il regime reale del motore RC. Il limitatore interviene al regime alto dell'ultima marcia simulata. Se non raggiungi il massimo, usa la calibrazione gas già presente nella sezione ESC.

Uscita audio: altoparlante o uscita audio selezionata sul telefono. Non servono nuovi collegamenti all'ESP32. Le funzioni luci restano separate. Il modulo audio legge solo dati e non invia comandi a motore, servo o impostazioni.

## Interruzioni

Audio fermo se manca il PWM, la telemetria è DEMO/non valida, il Wi-Fi non risponde, i dati restano fermi, il telefono sospende l'audio oppure si cambia schermata/modalità. Dopo una pausa serve un nuovo tocco su Avvia: il motore non riparte da solo. Un inviluppo sul motore audio silenzia il suono anche quando il browser ritarda i timer JavaScript.

In caso di silenzio controlla volume multimediale, uscita audio selezionata e messaggio del pannello. Ritorna agli strumenti, verifica il PWM e premi Avvia. La latenza dipende da Wi-Fi e dispositivo audio; non sono disponibili misure sul telefono dell'utente.

## Installazione

Il file `FIRMWARE/stormDASH-7.1.0-rc.9-audio.bin` è la sola applicazione per la configurazione ESP32-S3 N16R8 già installata.

Apri **Pit & setup → Aggiorna firmware**, seleziona il file, attendi il riavvio e riconnettiti. Ricarica la pagina e controlla la versione in **Hardware e diagnostica**. Usa il percorso OTA del firmware esistente per conservare configurazione e file. I vecchi script USB di installazione iniziale non vanno usati per aggiornare alla cieca una scheda che ha già cambiato slot OTA.

## Verifica

- Build ESP32-S3 completata; HTML incorporato coerente con i sorgenti.
- 183 test nativi e 36 test del portale superati.
- Sintesi verificata con OfflineAudioContext reale in Chrome e WebKit: intonazione, volume, cambiate, limitatore, arresto e assenza di clipping nei casi di prova.
- 9 gruppi di integrazione audio superati su ciascun browser: gesto iniziale, telemetria, errori, timeout, cambio schermata, richieste tardive e mancata ripartenza automatica. Questi test usano API e ciclo di vita audio simulati.
- Regressione completa del portale Chrome: 40 combinazioni responsive/tema/pagina e 8 combinazioni della pagina OTA.

Questi test non sostituiscono l'ascolto sul telefono e la prova con il ricevitore fisico.

## Installazione verificata sulla scheda

Il 13 novembre 2026 è stato aggiornato solo lo slot applicativo attivo; hash verificato dopo la scrittura. La diagnostica successiva al riavvio riporta versione 7.1.0-rc.9-audio, entrambi i display operativi, archivio pronto e Wi-Fi online. Il segnale PWM era assente: resta da fare la prova al gas e di ascolto con ricevitore e telefono.

## Sorgenti

`web/engine_synth.js`: sintetizzatore locale. `web/engine_audio.js`: consenso all'avvio, lettura della telemetria e arresto. `web/pit.html` e `web/pit.css`: pannello. Generazione con `npm run build:ui`; test audio con `npm run test:engine` e `npm run test:engine:webkit`.
