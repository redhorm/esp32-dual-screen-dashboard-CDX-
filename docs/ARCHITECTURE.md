# Architettura stormDASH

Firmware **7.0.0-rc.1**, Arduino ESP32-S3 con PlatformIO, modulo **ESP32-S3-WROOM-1 N16R8**. L'architettura separa ingresso reale, stato del prodotto, rendering, rete e persistenza. Mantiene i driver e i renderer IQ3/HDS-7 della baseline approvata per proteggere la compatibilità fisica e grafica.

## Proprietà dello stato e dei display

| Contesto | Responsabilità | Vincoli |
|---|---|---|
| Core 1, ciclo Arduino | Simulazione strumenti, BOOT, stato Trail/gioco/SHOW, entrambi i renderer e trasferimenti TFT | Un solo proprietario degli sprite e dei display; nessun accesso ordinario al filesystem nel rendering |
| Core 0, task controllo | PWM in sola lettura, logica ESC, priorità e timeout luci | Tick richiesto ogni 10 ms, priorità 3; niente HTTP o salvataggi nel tick |
| Core 0, task portale | HTTP/DNS, API, OTA, archivio, media opzionali, diagnostica lenta | Priorità 1; salvataggi fuori dal ciclo grafico |

`StormService` riceve comandi in una coda FreeRTOS di 16 elementi. Il ciclo proprietario applica al massimo 16 comandi per passaggio e pubblica una copia dello stato protetta da una breve sezione critica. I risultati chiusi passano in una seconda coda, separando la chiusura della sessione dal commit NVS. Una coda satura viene segnalata via HTTP 503; non ci sono attese bloccanti per aggiungere un comando.

Impostazioni legacy, stato esperienza e diagnostica usano snapshot protetti. Le sezioni critiche non contengono I/O o disegno. Se un task non parte, il ciclo principale offre un fallback e registra l'errore; in quel caso non è garantito lo stesso isolamento dai carichi HTTP.

## Modalità e tempo

`Storm::Mode` distingue **Instruments, Show, Game, Pit, Board**. Trail Session è uno stato indipendente: il timer non dipende da pagina web aperta o modalità TFT. Avvio e credenziali Wi-Fi sono presentazioni temporanee; il rientro in Instruments riprende i renderer originali.

Il proprietario campiona `millis()` e aggiorna lo stato con sottrazioni unsigned compatibili con il normale wrap del contatore. SHOW dura 6 secondi. Entrambi i renderer leggono lo stesso `Snapshot`, quindi la posizione del buggy deriva dalla stessa base temporale. Il portale luminoso rende intenzionale il salto tra superfici senza affermare una continuità geometrica in millimetri che non è stata misurata.

NIGHT STAGE usa passi fisici di 20 ms, con recupero temporale limitato a 100 ms per aggiornamento. Il seed inizializza un generatore xorshift; gli ostacoli sono ripetibili. La velocità aumenta ogni 15 secondi, la partita termina a 45 secondi, ogni ostacolo può produrre un solo urto. Il record è `45000 / (1 + urti)`, con divisione intera: più alto è meglio. Un ritardo esteso può rallentare il tempo di gioco rispetto al tempo reale; non vengono recuperati centinaia di passi in un solo frame.

Il controller web invia heartbeat periodici. Dopo 2,5 secondi senza contatto il gioco si ferma; il passaggio della pagina in background invia anche un comando esplicito. Un salto da BOOT attiva il controllo locale per quella partita. BOOT lungo esce dal gioco e mostra le credenziali; nessuna azione arcade scrive sulle uscite del veicolo.

## Rendering preservato e nuove scene

`instrument_data.h` è l'adattatore dati degli strumenti. `instrument_pilot.h`, `instrument_navigator.h`, `recoil_geometry.h` e i relativi font/geometrie mantengono l'aspetto approvato. La prova host ricompila il renderer della copia RECOIL e quello stormDASH con gli stessi ingressi e confronta **600 fotogrammi, 300 per pannello**: [zero differenze](../evidence/instrument-pixel-comparison.json). La verifica delle [coordinate CAD](../evidence/cad.txt) controlla finestra curva e sedici centri luminosi.

`storm_renderer.h` contiene logo pixel, buggy, portali, gioco, tabellone e Pit Check. Il pilota usa la stessa finestra curva degli strumenti; i punti luminosi vengono disegnati con le coordinate originali. Il copilota usa un canvas limitato a 240×135. Le anteprime nuove registrano [zero scritture fuori dal canvas](../evidence/new-mode-render.json). Sono verifiche software, non misure dell'allineamento ottico.

La sequenza di accensione esistente resta separata da SHOW; l'eventuale immagine personale viene decodificata una sola volta durante setup. Tornare agli strumenti non rilancia l'accensione. Nessuna modifica di marchio è applicata ai disegni IQ3/HDS-7 protetti.

## Memoria, SPI e scelte misurate

La flash fisica da 16 MB e la PSRAM octal da 8 MB sono verificate separatamente dalla compilazione. Il profilo usa `qio_opi`, `board_build.psram_type=opi` e `BOARD_HAS_PSRAM`. All'avvio una prova scrive e rilegge un buffer PSRAM da 1 MB, poi lo libera; il risultato partecipa alla verifica di salute OTA.

| Uso | Collocazione / dimensione | Motivo |
|---|---|---|
| Framebuffer pilota | RAM interna, 160×80×2 = 25.600 byte | Compatibilità con driver e trasferimenti validati |
| Framebuffer copilota | RAM interna, 240×135×2 = 64.800 byte | Evita di assumere DMA diretto da PSRAM |
| Registrazione e ghost | PSRAM, 2×2.250 = 4.500 byte | Campione di altezza per ogni passo di una partita; nessuna allocazione per frame |
| Stato e code | RAM, dimensioni limitate | Controllo, timer e input restano piccoli e prevedibili |
| Portale, logo e scene procedurali | Flash applicazione | Disponibili anche senza LittleFS; nessuna CDN o bitmap animata caricata in continuo |
| Storico | Blob NVS, massimo 64 risultati | Scritture su eventi significativi; non a ogni tick |

Il ghost migliore del seed corrente vive in PSRAM e si perde al riavvio o al cambio percorso; il punteggio record vive in NVS. Se l'allocazione ghost fallisce il gioco resta utilizzabile senza ghost. Non vengono allocati nuovi framebuffer per SHOW o gioco.

LovyanGFX configura SPI2 per il pilota e SPI3 per il copilota, con selezione DMA automatica del driver. I framebuffer restano interni e vengono inviati in successione. Non è implementata una pipeline di doppio buffering asincrona né un trasferimento DMA diretto dalla PSRAM. Il ridisegno completo protegge il ripristino dei pixel degli strumenti; gli aggiornamenti parziali non sono necessari per questi framebuffer piccoli e non sono dichiarati come ottimizzazione eseguita.

Le cadenze richieste sono circa 30 FPS sul pilota e 15 sul copilota durante gli strumenti; nelle nuove modalità entrambi vengono aggiornati allo stesso tick. I campioni reali mostrano 29–30 / 14–15 e 30 / 30 nel gioco. I tempi di render e push sono esposti nella diagnostica; non equivalgono a latenza comando→fotone o assenza di tearing. [TESTING](TESTING.md) riporta finestre e limiti delle prove.

## Cablaggio e provenienza dei dati

| Funzione | GPIO / configurazione conservata |
|---|---|
| TFT A, ST7735S 160×80 | MOSI 11, SCLK 12, CS 10, DC 9, RST 8, BL 46; offset 26/1; rotazione 1 |
| TFT B, ST7789V 240×135 | MOSI 13, SCLK 14, CS 15, DC 16, RST 17, BL 18; finestra nativa 135×240, offset 52/40, rotazione 1 |
| Sedici punti della maschera | Pixel del TFT A, non una periferica separata |
| Barra opzionale 16 WS2812 | Dati GPIO4 |
| PWM ESC | GPIO5, esclusivamente ingresso, timeout 100 ms |
| BOOT | GPIO0, pressione breve / lunga |

I pin riservati alla memoria del modulo N16R8 non sono usati dal cablaggio cockpit. GPIO0 e GPIO46 mantengono i vincoli di avvio già presenti: non cambiare i collegamenti per inseguire una prova software. Non sono installati o assunti touchscreen, altoparlante, GPS, IMU o sensori motore.

Velocità, RPM, marcia, temperature, tensione e navigazione restano simulati. Il PWM è un comando radio/ESC; l'interprete distingue retro diretta, sequenza freno/neutro/retro e avanti/freno, senza inferire movimento fisico. I comandi manuali luce hanno scadenza di 1,5 secondi. Lo stato `hardwareReady` indica inizializzazione del driver, non presenza osservata della barra. Il test visivo dei TFT richiede una persona.

## Persistenza, partizioni e OTA

La configurazione legacy mantiene namespace, record A/B con CRC, ID e percorsi media compatibili. La rinomina del prodotto cambia il prefisso AP in stormDASH, conservando la password esistente. Il nuovo archivio usa `stormdash/archive1`, versione schema 3 con checksum; comprende risultati, peso penalità, record gioco e una maschera che distingue i collaudi dalle sessioni manuali. La migrazione legge e valida lo schema 2 della build di sviluppo, conserva i dati e contrassegna i relativi risultati come collaudo; sulla scheda verificata sono i due risultati delle prove iniziali. Il formato RECOIL preesistente resta separato e invariato. Il firmware salva alla chiusura, al miglioramento del record o al cambio regole, rilegge il blob e verifica i byte. Un errore mantiene i dati in RAM e ritenta dopo 5 secondi. La sessione non chiusa non sopravvive a un reset.

| Partizione | Offset | Dimensione | Uso |
|---|---:|---:|---|
| nvs | 0x9000 | 0x5000 | Impostazioni e archivio, posizione originale |
| otadata | 0xE000 | 0x2000 | Selezione e stato slot OTA |
| app0 | 0x10000 | 0x300000 | Primo slot, posizione originale |
| littlefs | 0x310000 | 0x4F0000 | Media legacy, posizione originale |
| app1 | 0x800000 | 0x300000 | Secondo slot, nella metà superiore verificata vuota |
| stormdata | 0xB00000 | 0x500000 | Riserva futura; non montata automaticamente |

LittleFS non è necessaria per strumenti, portale, SHOW o gioco. Una partizione non montabile viene inizializzata soltanto dopo aver confermato che sia interamente vuota (`0xFF`); contenuto presente o errori di lettura impediscono la formattazione. Le foto esistenti restano nella partizione originale.

L'OTA usa Arduino Update / ESP-IDF e scrive lo slot inattivo. Richiede token del dispositivo, nessuna sessione attiva, modalità Instruments e flash riconosciuta da 16 MB. L'override forte `verifyRollbackLater()` è verificato nel file ELF e rinvia la conferma automatica anticipata di Arduino. Il nuovo avvio viene confermato dopo 10 secondi se heartbeat, task web, prova PSRAM e capacità riconosciute superano i controlli. La soglia PSRAM di salute è almeno 7 MiB, per ammettere l'overhead del runtime su un chip fisico da 8 MB. Un'avaria di salute richiede rollback. La verifica del formato e hash immagine non è una firma dell'autore; secure boot e firmware firmato non sono configurati.

Upload, conferma e rollback provocato sono stati verificati sulla scheda. La prova di rollback ha eseguito un reset a 5 secondi dall'upload, prima della conferma di salute: i metadati mostrano sequenza 3 `VALID` e candidato sequenza 4 `ABORTED`, con ritorno all'immagine precedente. Il campo `FAILED` del [rapporto automatico](../evidence/ota-rollback.json) riguarda il tentativo troppo anticipato del Mac di riconnettersi al Wi-Fi; non annulla il riscontro diretto dei metadati flash. Il successivo [OTA finale 7.0.0-rc.1](../evidence/ota-final.json) ha riscritto il candidato e ottenuto app1, sequenza 4, stato `VALID`, con riconnessione al portale riuscita. Sono conservati i due risultati iniziali contrassegnati come collaudo, il record 1956 e i media: LittleFS pronta, 86.016 byte occupati, di cui 69.238 byte di foto utente. Nel campione a 16 secondi dall'avvio i contatori sono 30/15 FPS, entrambi i display e il task controllo risultano attivi. Un reset prima della conferma non equivale a una prova di distacco alimentazione durante la scrittura: quest'ultima resta da eseguire. Procedure e backup: [MIGRATION](MIGRATION.md); dettagli delle prove: [TESTING](TESTING.md).


## Estensione WOW 7.1.0-rc.1

`wow_state.h` è un motore originale a passi di 20 ms, con recupero limitato a 100 ms dopo uno stallo. Cinque modalità condividono un solo stato e 48 particelle allocate staticamente; nessuna allocazione per fotogramma. `storm_state.h` inoltra solo i controlli pertinenti alla modalità attiva e mantiene il timer Trail indipendente. Il record MIDNIGHT viene salvato separatamente in `stormdash/midnight1`, con controllo di integrità e rilettura; lo schema dell’archivio Trail resta 3. Il record viene scritto solo se cambia, con ritentativo dopo 5 s in caso di errore.

Lo snapshot condiviso misura 560 byte nel test host; la sezione critica esegue soltanto una copia limitata. Un solo task mantiene la proprietà dei display. `wow_renderer.h` disegna attraverso il clipping della maschera pilota e della superficie copilota. I renderer originali IQ3/HDS non sono modificati. La barra WS2812 conserva il suo controllo luci: BEAT anima i sedici punti ottici sul TFT.

Il browser riproduce un file locale tramite Web Audio e invia quattro bande numeriche. AudioContext, sorgente e URL temporaneo vengono chiusi/rilasciati alla fine, allo stop, al cambio sezione e alla perdita di collegamento. Il firmware non decodifica file musicali e non richiede un microfono. La coda gesti del browser contiene al massimo il dato più recente mentre una richiesta è in corso. La coda firmware resta limitata a 16 comandi.
