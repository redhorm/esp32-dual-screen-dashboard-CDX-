# stormDASH — memoria, installazione e recupero

Il backup integrale della scheda è in `<user>/Documents/stormDASH-preservation/device-flash-16MB.bin` (16.777.216 byte). SHA-256: `be6642dc1d00d7333ee722f018dcd966d8ef0b666b49b3ffcebda7e0539a7081`. Contiene anche credenziali locali: conservarlo privatamente, non distribuirlo nel pacchetto pubblico.

La lettura USB ha identificato flash da 16 MB e PSRAM embedded da 8 MB AP_3v3; il proprietario conferma ESP32-S3-WROOM-1 N16R8. Il dump dimostra che tutta la metà superiore era 0xFF. Le partizioni effettive sono registrate in `evidence/device-partitions.json`.

| Regione | Prima | stormDASH | Effetto della prima installazione |
|---|---|---|---|
| Bootloader | 0x0000 | 0x0000 | Aggiornato per immagine 16 MB / qio_opi |
| Tabella | 0x8000 | 0x8000 | Estesa; nessuna partizione esistente spostata |
| NVS | 0x9000, 0x5000 | identica | Non sovrascritta dal caricatore |
| OTA data | 0xE000, 0x2000 | identica | Conservata nella migrazione USB iniziale |
| app0 | 0x10000, 3 MB | identica | Nuovo firmware |
| LittleFS | 0x310000, 0x4F0000 | identica | Foto e file mantenuti |
| app1 | spazio vuoto | 0x800000, 3 MB | Secondo slot OTA |
| stormdata | spazio vuoto | 0xB00000, 5 MB | Riserva; non montata né formattata automaticamente |

La configurazione legacy e il namespace `ultra4-ui` restano compatibili. Non rinominare chiavi, schemi o percorsi delle foto per uniformare il marchio. La rete cambia prefisso in stormDASH mantenendo la password NVS. Lo storico nuovo usa `stormdash/archive1`, schema 3 con checksum, limite 64 risultati: scrittura alla chiusura della sessione, al miglioramento del record e al cambio delle regole. NVS fornisce commit atomico; il firmware rilegge il blob e ne verifica i byte. Un errore lascia disponibili stato e CSV in RAM e ritenta dopo 5 secondi. Lo schema 2 esisteva solo durante lo sviluppo: i risultati importati sono marcati COLLAUDO. Lo schema 3 conserva un bit di provenienza per ogni record, incluso nel checksum. Un riavvio interrompe una sessione non chiusa, senza inventare un risultato.

## Recupero USB

Chiudere il monitor seriale, tenere BOOT durante RESET se la scheda non si enumera. Con l’ambiente `.tools/venv` attivo:

```
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 write_flash 0x0 <user>/Documents/stormDASH-preservation/device-flash-16MB.bin
```

Questo ripristino integrale sostituisce anche impostazioni e risultati nuovi con quelli del momento del backup: usarlo solo intenzionalmente. Non richiede erase-all. Il recupero richiede alimentazione stabile; una disconnessione durante bootloader/tabella può richiedere di ripetere il caricamento in modalità ROM USB.

## OTA

Il portale `/update` accetta un solo firmware applicativo e scrive esclusivamente lo slot inattivo mediante Arduino Update / ESP-IDF OTA. Richiede token casuale del dispositivo, nessuna sessione attiva e modalità strumenti. Il formato e il checksum/hash dell’immagine vengono verificati dal loader; non è una firma dell’autore. L’accesso è sulla rete locale protetta dalla password del cruscotto. Non inviare immagini di provenienza sconosciuta.

Il nuovo avvio pending viene confermato dopo 10 secondi solo con heartbeat dei display e controllo, task web attivo, flash da 16 MB e PSRAM riconosciuta da almeno 7 MiB (8 MB fisici meno overhead del gestore heap). Fallimento: rollback. Reset prima della conferma: il bootloader con rollback attivo ripristina il precedente slot valido. Il test software dei display non certifica il pannello fisico. Bootloader precompilato e SDK qio_opi hanno CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1. stormDASH sovrascrive con linkage C la funzione weak Arduino verifyRollbackLater() e restituisce true: senza questo override il core confermerebbe la nuova app prima dei controlli del prodotto. Il rollback prima della conferma è stato provocato e verificato fisicamente; l’interruzione di alimentazione durante la scrittura resta una prova distinta, non eseguita.
