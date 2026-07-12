# INIZIA QUI

## Dispositivo già programmato

1. Collega il cavo USB.
2. Attendi 5 secondi.
3. Collegati alla rete Wi-Fi `ULTRA4-DASH`.
4. Inserisci la password `ultra4setup`.
5. Apri `http://192.168.4.1`.

Il messaggio “nessuna connessione Internet” è normale.

## Scheda nuova o aggiornamento

Collega l’ESP32-S3 con un cavo USB dati e avvia:

```bash
# macOS / Linux
python3 tools/flash_dashboard.py --flash

# Windows
py tools\flash_dashboard.py --flash
```

Al primo utilizzo:

1. resta collegato a Internet;
2. rispondi `S` alla richiesta PlatformIO;
3. attendi il messaggio `FLASH COMPLETATO`.

Non serve Homebrew.

## Guida illustrata

[Apri la guida rapida PDF](output/pdf/Guida_Rapida_ESP32_Dual_Screen.pdf)

## Se la scheda non viene trovata

1. cambia cavo USB;
2. tieni premuto `BOOT`;
3. premi e rilascia `RESET`;
4. rilascia `BOOT`;
5. ripeti lo stesso comando.
