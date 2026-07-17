# INIZIA QUI — BETA 5

Questa è la procedura minima per usare **Ultra4 Microdash 5.0.0-beta.1**.

## 1. Accendi e collegati

1. Alimenta la scheda con un cavo USB-C dati.
2. Attendi **CONTROLLO SISTEMA / SYSTEM CHECK**.
3. Al primo avvio leggi nome rete e password sui display; se la scheda era già
   configurata tieni premuto **BOOT** per mostrarle.
4. Dal telefono collegati a `ULTRA4-DASH-XXXXXXXX`.
5. Ignora l’avviso “nessuna Internet” e resta su questa rete.
6. Se la pagina non compare, apri `http://192.168.4.1`.

`PSRAM OPTIONAL` è normale su questa scheda N8. Per rivedere in seguito le
credenziali, tieni premuto **BOOT** mentre il dispositivo è acceso.

## 2. Completa il primo avvio

Scegli **IT** o **EN**, poi inserisci:

1. numero gara;
2. nome veicolo;
3. pilota.

Premi **SALVA E INIZIA**. Questi dati si modificano in qualsiasi momento da
HOME → **MODIFICA**.

## 3. Usa il menu in basso

- **HOME** — riepilogo e diagnostica;
- **DASH** — Classic Rally, Race Realism o Auto di serie;
- **MEDIA** — immagine boot e cornice digitale;
- **GAME** — ULTRA4: Trail Assault;
- **RTL** — barra posteriore e ingresso ESC.

## 4. Scegli la dashboard

In **DASH** scegli una delle tre coppie grafiche, imposta unità e luminosità,
quindi premi **SALVA DASH**.

- Classic Rally: stile gara originale.
- Race Realism: quadro pilota racing e navigazione copilota offroad.
- Auto di serie: quadro OEM moderno.

## 5. Carica una foto

1. Apri **MEDIA**.
2. Scegli JPG/JPEG, PNG, WEBP o HEIC/HEIF. Se il browser non apre HEIC, usa
   JPG o PNG.
3. Controlla le due anteprime.
4. Premi **CARICA** e attendi il messaggio di conferma.

Il telefono crea automaticamente la versione 160×80 e quella 240×240. Non
spegnere durante il commit. Se l’upload fallisce, la foto precedente resta
intatta. La galleria contiene fino a 24 foto; intervallo, ordine, transizione e
display di destinazione sono regolabili.

## 6. Gioca a Trail Assault

1. Apri **GAME** e ruota il telefono in orizzontale.
2. Scegli Time Attack, Gate Rush o Hill Climb.
3. Lascia il seed proposto oppure inseriscine uno condiviso.
4. Premi **AVVIA / START**: su iPhone questo gesto attiva anche l’audio.
5. Muovi il dito sul volante; scorri verticalmente su GAS e FRENO.

Il controller invia comandi analogici. Se la pagina perde il collegamento per
300 ms, il gioco azzera automaticamente sterzo, gas e freno. Uscendo dalla
scheda GAME i comandi tornano subito neutrali.

Tastiera: `A/D` o `←/→`, `W/↑/spazio` gas, `S/↓` freno, `P` pausa.

## 7. Controlla lo stato

Apri HOME → **DIAGNOSTICA**. Verifica:

- display A e B: `OK`;
- LittleFS: pronta;
- AP: online;
- PSRAM: assente/0 è previsto;
- `lastError`: vuoto o `none`.

FPS nominali: pilota circa 30; copilota circa 12,5 in dashboard e 20 durante
Trail Assault. Sono obiettivi da verificare sulla scheda reale.

## Programmare o aggiornare via USB

Non serve Homebrew.

Aggiornamento normale, che conserva foto e immagine boot:

```bash
# macOS / Linux
python3 tools/flash_dashboard.py --flash

# Windows
py tools\flash_dashboard.py --flash
```

Prima installazione su una scheda nuova:

```bash
python3 tools/flash_dashboard.py --flash --factory-init-media
```

`--factory-init-media` inizializza LittleFS ma **cancella tutta la galleria e
l'immagine boot**. Non usarlo per gli aggiornamenti ordinari.

Al primo utilizzo resta online: il tool può installare PlatformIO in una
cartella isolata del progetto.

Questa versione non ha aggiornamento OTA né rollback firmware: la flash è
sempre cablata. Se la scheda non viene trovata, cambia cavo; poi tieni premuto
BOOT, premi e rilascia RESET, rilascia BOOT e ripeti.

## RTL ed ESC: prima la sicurezza

- GPIO 4: solo DATA WS2812, con 5 V separati e massa comune;
- GPIO 5: ingresso PWM ESC di sola lettura, disabilitato finché non configurato;
- mai collegare GPIO o LED direttamente ai 12 V;
- il segnale ESC deve essere limitato a 3,3 V;
- la barra non sostituisce luci o sistemi di sicurezza omologati.

Per dettagli tecnici consulta [README.md](README.md) e
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
