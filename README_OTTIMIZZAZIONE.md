# stormDASH 7.1.0-rc.8-opt — 13 novembre 2026

Ottimizzazione della base pubblicata `adca3e5` (Industrial UI, 9 novembre).
Progetto CAD fermato: nessun file Fusion o modello meccanico modificato.

## Modifiche implementate

- La coda HTTP ha una scadenza: una richiesta rimasta in attesa viene rimossa senza inviarla in ritardo. Il controllo della scadenza avviene anche prima dell'invio, quando il browser sospende i timer in background.
- Il messaggio «richiesta non inviata, cruscotto occupato» distingue la congestione locale dalla perdita di collegamento. I comandi scaduti non sono ripetuti automaticamente.
- Pit & setup segue lo stato di connessione della pagina principale: i dati già caricati non mantengono l'etichetta online quando il collegamento è perso. Il pulsante di riconnessione ricompare.
- Le risposte ESC/luci usano il buffer fisso già presente nel server invece delle concatenazioni dinamiche String. Restano invariati campi, valori e separazione dei controlli. Un errore di capacità restituisce JSON valido con HTTP 500.
- La pagina OTA viene inviata in tre segmenti (HTML, token, HTML), eliminando la copia dinamica dell'intera pagina. Token, modulo e contenuto ricevuto restano identici.
- Anche le preferenze hanno `Cache-Control: no-store`.
- Aggiornata la versione diagnostica e corretta un'aspettativa del test di avvio rimasta alla vecchia RC.5.

Pin, display 160×80 e 240×135, lettura PWM, direzioni crawler, calibrazione gas, simulazione marce, grafica TFT, salvataggi e configurazione delle partizioni sono quelli della base. L'ottimizzazione riguarda il portale e la gestione delle risposte web.

## CircuitPython: esito della verifica

La [release Adafruit CircuitPython 11.0.0-alpha.3](https://github.com/adafruit/circuitpython/releases/tag/11.0.0-alpha.3) è una versione alpha: include SPI asincrono, miglioramenti displayio e correzioni di rete. Il progetto usa Arduino/C++ con LovyanGFX; queste patch non sono aggiornamenti installabili sul nostro codice. Una migrazione richiederebbe riscrivere driver, servizi e persistenza. È stata quindi mantenuta la piattaforma esistente, applicando ottimizzazioni verificabili alla gestione di memoria e richieste. Nessun aumento di FPS è stato misurato o promesso.

## Verifiche eseguite

- Build ESP32-S3 N16R8 riuscita: 1.368.205 byte flash applicativa, 60.160 byte RAM statica (43,5% e 18,4% dei limiti riportati da PlatformIO).
- 183/183 test nativi superati, inclusi ESC/crawler, calibrazione, marce, luci e display.
- 36/36 test JavaScript superati, inclusi scadenza, mancato invio e recupero della coda.
- Chrome: portale completo, 40 combinazioni pagina/tema/layout e 8 combinazioni della pagina OTA.
- WebKit: portale completo a 320, 390 e 1120 px, calibrazione, separazione ESC/luci, errori e recupero; nessun errore JavaScript.
- Test dei veri corpi dei gestori C++ con trasporto simulato, AddressSanitizer e UndefinedBehaviorSanitizer: JSON a 0/16/64 pixel, conteggio invalido, buffer insufficiente e pagina OTA identica byte per byte.
- Test dei gestori di salvataggio: separazione ESC/luci, rollback su errore NVS, luci PWM attive anche con strumenti ESC disabilitati.
- File web generati coerenti con i sorgenti e completamente locali, senza CDN.

Queste sono verifiche sul computer. Il firmware non è stato caricato sulla scheda in questa sessione; restano da verificare sul dispositivo Wi-Fi reale, display e comportamento con il ricevitore collegato.

## Installazione sulla configurazione esistente

1. Usa `FIRMWARE/stormDASH-7.1.0-rc.8-opt.bin`: è la sola applicazione per ESP32-S3 N16R8 con le partizioni di questo progetto, non un dump dell'intera flash.
2. Collega il telefono alla rete Wi-Fi del dashboard e apri `http://192.168.4.1`.
3. Termina sessioni/effetti e torna a Strumenti; apri **Pit & setup → Aggiorna firmware** e seleziona il `.bin`.
4. Attendi il riavvio, riconnettiti al Wi-Fi e ricarica la pagina. In **Hardware e diagnostica** verifica `7.1.0-rc.8-opt`.
5. Verifica prima il neutro, poi avanti e retro; le impostazioni precedenti restano gestite dalla persistenza già presente. Non è prevista una cancellazione della flash.

Se l'OTA non è accessibile, i sorgenti si compilano con `python3 tools/build_portal.py` e `pio run -e esp32-s3-devkitc-1`. I test aggiunti si eseguono con `npm test` e `npm run test:network`; i test browser con `npm run test:browser` e `npm run test:webkit`. Le dipendenze sono dichiarate in `package-lock.json` e `platformio.ini`.

## Contenuto del pacchetto

- `FIRMWARE/`: applicazione compilata e SHA-256.
- `SORGENTI/`: progetto completo, test e sorgenti web; esclusi cache, dipendenze installate e vecchie release.
- `VERIFICHE/`: log e schermate dei test; il build-baseline documenta il punto di partenza.
- `MODIFICHE.patch`: differenza rispetto al commit di partenza, escluso l'HTML generato.
- `MANIFEST_SHA256.json`: integrità dei file del pacchetto.
