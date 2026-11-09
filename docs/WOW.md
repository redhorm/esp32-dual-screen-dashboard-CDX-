# stormDASH 7.1 — le cinque modalità WOW

Apri il portale locale del cruscotto e scegli **WOW**. Tocca una delle cinque schede: l’effetto appare sui due TFT. **Torna agli strumenti** ripristina IQ3 e HDS con la grafica originale. Le modalità non comandano motore o servo; il timer Trail eventualmente aperto continua sul cruscotto.

| Modalità | Cosa appare | Comandi sul telefono | BOOT breve |
|---|---|---|---|
| **MIDNIGHT RUN** | Corsa pseudo-3D di 60 secondi, curve, gallerie, tramonto; tempo e punti sul pilota | Tieni premuta una freccia per sterzare. Scegli il percorso con il seed e usa **Gioca / riavvia**. **Riprendi** riavvia una corsa in pausa | Pausa / riprendi |
| **LIVING COCKPIT** | Copilota espressivo e pulsazione luminosa sul pilota | Trascina il dito per dirigere lo sguardo; **Fai l’occhiolino** chiude brevemente gli occhi | Occhiolino |
| **LIQUID REACTOR** | 48 particelle luminose e una seconda camera animata sul pilota | Trascina per attirare la corrente; **Crea un’onda** aggiunge un impulso | Onda |
| **GARAGE 3D** | Buggy originale stilizzato, con una seconda vista sul pilota | Trascina per ruotarlo; **Separa i componenti** e **Ricomponi** animano la vista esplosa | Alterna separato / ricomposto |
| **BEAT STORM** | Spettro e impulsi sui due TFT e sui sedici punti ottici della maschera | Scegli un file locale e tocca **Riproduci**. **Ferma** interrompe audio ed effetto | Cambia palette fra quattro colori |

Tenendo premuto **BOOT per 2 secondi** si esce dalla modalità e si mostrano le credenziali Wi-Fi. I pad supportano anche le frecce della tastiera; **Home** li riporta al centro.

## MIDNIGHT RUN

L’accelerazione è automatica: evita gli ostacoli e limita gli urti. Il punteggio dipende dalla distanza e diminuisce con gli urti. Lo stesso seed, con gli stessi comandi, riproduce lo stesso percorso. Il record viene aggiornato all’arrivo e salvato separatamente dallo storico Trail; un salvataggio non confermato è segnalato nel portale.

Il rilascio del dito, l’annullamento del tocco o la perdita del focus rilasciano lo sterzo. Il cruscotto annulla comunque un comando di sterzo dopo 500 ms senza aggiornamenti. Cambiare sezione o mettere la pagina in background mette in pausa la corsa; in caso di perdita del collegamento la pausa scatta dopo circa 2,5 secondi. Tornare alla pagina non fa ripartire da solo il gioco: usa **Riprendi** o BOOT breve. Un messaggio di mantenimento connessione arrivato in ritardo non annulla la pausa.

## Espressioni, luce e modello 3D

Living reagisce agli eventi manuali gate e penalità, alla chiusura Trail e a un nuovo record NIGHT STAGE. Le reazioni durano pochi secondi; non rappresentano sensori o riconoscimenti automatici. Liquid è una simulazione grafica di particelle comandata dal dito, senza accelerometro o giroscopio. Garage usa geometria originale semplificata: non è il modello CAD del mezzo reale.

## Musica dal telefono

BEAT STORM accetta un **file locale non vuoto fino a 30 MiB**, riproducibile dal browser. MP3 e WAV sono le scelte più semplici; altri formati dipendono dal telefono. Il file viene riprodotto dal telefono, analizzato localmente in quattro bande e **non viene caricato sul cruscotto**. Alla scheda arrivano soltanto quattro livelli numerici, al massimo dieci volte al secondo.

Occorre toccare **Riproduci**: non c’è avvio automatico. Se il browser della finestra Wi-Fi non supporta l’audio, apri il portale in Safari o Chrome. La modalità non cattura Spotify, il microfono o l’audio di altre app. Il cruscotto non richiede altoparlante, microfono o la barra WS2812 opzionale: i sedici punti usati sono quelli del TFT pilota nella maschera.

**Ferma**, fine del brano, cambio sezione, uscita dalla modalità, pagina nascosta o perdita del collegamento chiudono la riproduzione e liberano le risorse audio. Tornando alla pagina serve un nuovo tocco su **Riproduci**; il brano riparte dall’inizio. Una sospensione imposta dal telefono viene segnalata. La scheda smorza l’effetto entro 500 ms senza nuovi campioni; il comando di arresto ricevuto lo azzera subito.

## Provenienza e verifica

Le cinque nuove logiche, i renderer e la geometria del buggy sono implementazioni originali del progetto. La ricerca su [esp32s3-arcade-3d](https://github.com/davidmonterocrespo24/esp32s3-arcade-3d), [Grobot Animations](https://github.com/tanmaywankar/Grobot_Animations), [esp32-fluidbox](https://github.com/V4C38/esp32-fluidbox) e [stevesch-Mesh](https://github.com/stevesch/stevesch-Mesh) ha fornito riferimenti concettuali: nessun loro codice o asset è stato importato. L’analisi audio usa le API del browser descritte da [MDN Web Audio](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API/Visualizations_with_Web_Audio_API). Restano le dipendenze già dichiarate del firmware.

Le verifiche software comprendono il motore nativo, il portale, la gestione audio e l’integrazione reale Chromium / Web Audio con il motore C++. L’evidenza browser è in `evidence/wow-browser.json`; non usa hardware. Il confronto dei pixel degli strumenti verifica separatamente che IQ3 e HDS siano rimasti identici.

La scheda era scollegata durante lo sviluppo di questa versione: **fluidità reale, latenza Wi-Fi, resa sui TFT e caricamento del nuovo binario devono essere collaudati quando verrà ricollegata**. Le anteprime sono generate dal renderer del firmware, non sono riprese fisiche del dispositivo.
