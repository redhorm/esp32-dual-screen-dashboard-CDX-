# Riferimenti e decisioni stormDASH

Consultati il 14 ottobre 2026.

- [Espressif, ESP32-S3-WROOM-1/1U](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): modello confermato dall’utente N16R8. Flash quad da 16 MB e PSRAM octal da 8 MB. Pin riservati alla memoria esclusi dal cablaggio cockpit. Il dato del modulo è distinto dall’identificazione eFuse/JEDEC e dalla misura del runtime.
- [Espressif, chip ESP32-S3](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf): variante R8 con PSRAM in package octal 3,3 V. La lettura esptool riporta embedded PSRAM 8MB AP_3v3, rev. 0.2; non inferiamo la capacità fisica dalla vecchia tabella partizioni.
- [ESP-IDF 4.4.7, OTA](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/ota.html): due slot, metadati OTA, conferma del nuovo avvio e rollback. Riferimento coerente con il framework Arduino utilizzato. L’aggiornamento della tabella via USB è distinto dal successivo OTA applicativo.
- [ESP-IDF, RAM esterna](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html): i vincoli di cache e DMA non scompaiono con la PSRAM. I buffer LCD restano interni; i campioni ghost usano esplicitamente MALLOC_CAP_SPIRAM. Non dichiariamo il trasferimento diretto DMA da PSRAM.
- [UKRCCA](https://ukrcca.org/rules): esistono regolamenti distinti per classi e manifestazioni. stormDASH usa regole personali semplici e contatori manuali; non dichiara conformità UKRCCA o riconoscimento automatico dei gate.
- [LANE Boys, configuratore](https://laneboysrc.github.io/rc-light-controller/) e [sorgenti](https://github.com/laneboysrc/rc-light-controller): distinguono avanti/freno/retro, crawler e avanti/freno. stormDASH conserva il processore ESC testato e rende esplicita la scelta nel portale. Le soglie descrivono comandi, non il movimento del mezzo. Non replica tutti i tipi e timeout del prodotto LANE Boys.
- [Rockstar, Vice City](https://www.rockstargames.com/VI/vice-city): riferimento d’atmosfera per tramonto, contrasto e profondità. Logo, sprite e scena stormDASH sono originali; nessun asset Rockstar viene incluso.
- [Meta, Reels Insights](https://about.fb.com/news/2023/04/instagram-reels-trending-audio-and-gifts-updates/) e [Trial Reels](https://about.fb.com/news/2024/12/trial-reels-try-content-non-followers-first-see-what-perfoms-best/): motivano il confronto tra tre aperture e la valutazione del tempo di visione. Il risultato social non è garantito.

Nessuna opinione di forum viene trattata come specifica hardware. Parallax, portale luminoso e ghost sono scelte progettuali: la separazione fisica dei TFT è intenzionale, senza fingere una continuità millimetrica non misurata.


## Riferimenti delle cinque modalità WOW

I seguenti progetti hanno guidato la valutazione di fattibilità. Il motore, le mesh e i disegni della versione 7.1 sono originali: non sono stati importati sorgenti o asset da questi repository.

- MIDNIGHT RUN: [esp32s3-arcade-3d](https://github.com/davidmonterocrespo24/esp32s3-arcade-3d), riferimento per corsa pseudo-3D su S3. La versione stormDASH usa le sue risoluzioni e controlli dal telefono.
- LIVING COCKPIT: [Grobot Animations](https://github.com/tanmaywankar/Grobot_Animations), riferimento per espressioni animate. Occhi e reazioni stormDASH sono procedurali.
- LIQUID REACTOR: [esp32-fluidbox](https://github.com/V4C38/esp32-fluidbox), riferimento per particelle interattive; stormDASH usa una simulazione 2D a 48 particelle, controllata dal dito senza IMU.
- GARAGE 3D: [stevesch-Mesh](https://github.com/stevesch/stevesch-Mesh), riferimento per visualizzazione 3D embedded; stormDASH contiene una mesh procedurale originale e proprie trasformazioni.
- BEAT STORM: [MDN — Visualizations with Web Audio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API/Visualizations_with_Web_Audio_API), analisi dello spettro nel browser. Lo streaming riguarda solo quattro valori di intensità; l’audio resta nel telefono.

Non vengono trasferite al dispositivo dichiarazioni di FPS o compatibilità delle schede usate nei progetti di riferimento. Le verifiche di questa consegna sono nel registro di test.
