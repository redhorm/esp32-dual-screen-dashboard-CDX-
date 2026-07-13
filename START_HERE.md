# INIZIA QUI — BETA 4.1

## Dispositivo già programmato

1. Collega un cavo USB-C dati.
2. Attendi circa 5 secondi.
3. Leggi rete e password sui display.
4. Collegati al Wi-Fi `ULTRA4-DASH-XXXXXXXX`.
5. Rimani collegato anche se il telefono indica “nessuna Internet”.
6. Se la pagina non compare, apri `http://192.168.4.1`.

Scegli **IT** o **EN** nell’intestazione: la lingua viene applicata al portale e
ai due display e resta salvata.

Per rivedere in seguito rete e password, tieni premuto **BOOT** mentre il
dispositivo è acceso.

## Prima configurazione

Inserisci soltanto:

1. numero gara;
2. nome veicolo;
3. pilota.

Premi **SALVA E INIZIA**. Puoi modificare questi dati dalla HOME, che mostra
anche velocità, RPM, marcia e batteria.

## Menu in basso

- **HOME** — riepilogo del veicolo;
- **DASH** — Classic Rally, Race Realism o Auto di serie;
- **MEDIA** — immagine di avvio e cornice digitale;
- **GAME** — DESERT BLITZ;
- **RTL** — barra WS2812 e ingresso ESC.

## Caricare una foto

1. Apri **MEDIA**.
2. Scegli un file JPG, JPEG, PNG o WEBP.
3. Controlla l’anteprima.
4. Premi **CARICA** e attendi il 100%.

Il dispositivo ottimizza la foto per entrambi i display e la conserva dopo il
riavvio o il refresh della pagina. Anteprima, selezione e pulsanti sono separati
in aree compatte. Per la cornice digitale puoi selezionare più file e regolare
display, intervallo, ordine e transizione.

## Giocare a DESERT BLITZ

- premi **START**;
- usa i pulsanti sinistra, destra e GAS;
- da tastiera usa `←`/`→` o `A`/`D`, `↑`/`W`/spazio per GAS;
- premi **P** o **PAUSA** per fermare e riprendere la partita.

## Programmare o aggiornare la scheda

```bash
# macOS / Linux
python3 tools/flash_dashboard.py --flash

# Windows
py tools\flash_dashboard.py --flash
```

Al primo utilizzo resta online e conferma l’installazione di PlatformIO. Non
serve Homebrew.

Se la scheda non viene trovata: cambia cavo, tieni premuto **BOOT**, premi e
rilascia **RESET**, rilascia **BOOT** e ripeti il comando.

## Collegamenti RTL ed ESC

- GPIO 4: solo DATA WS2812; alimentazione 5 V separata e massa comune;
- GPIO 5: ingresso PWM ESC, disabilitato finché non viene configurato;
- mai collegare un GPIO direttamente ai 12 V;
- usa protezioni e livelli logici compatibili con 3,3 V.

La barra è scenografica e non sostituisce luci o sistemi di sicurezza
omologati.

## Guida illustrata

[Apri la guida rapida PDF](output/pdf/Guida_Rapida_ESP32_Dual_Screen.pdf)
