# Risultati delle prove

## Firmware 2.2.7 — 9 settembre 2026

- `39/39` test API del simulatore superati, inclusi ordine cronologico dei
  campioni diagnostici, doppia temperatura, CPU fissa, riavvio programmato e conservazione delle calibrazioni.
- Build PlatformIO V2 riuscita e immagine OTA ECDSA-P256 firmata generata e
  verificata con la chiave pubblica incorporata.
- Portale verificato con gli asset incorporati: i grafici scorrono dal campione
  più vecchio a sinistra al più recente a destra; uptime, istante di avvio e
  temperature CPU e QMI8658 e frequenza CPU sono leggibili nella pagina Sistema.
- La frequenza CPU è fissata a 240 MHz. Se sulla 2.2.6 era stato salvato un
  profilo ridotto, la relativa chiave viene rimossa senza modificare le altre preferenze.
- L'export dei log usa un task separato a priorità bassa, snapshot delle dimensioni
  e blocchi da 2 KB; una seconda esportazione contemporanea viene rifiutata.
- Il caricamento ordinario dello storico restituisce cinque pesate e consulta al
  massimo gli ultimi 32 KB del file recente.
- Il collaudo sull'impianto installato è completato: quattro AS5600, calibrazione,
  display, touch, audio, microSD, RTC, batteria, Wi-Fi e integrazione CaskLogic
  sono stati verificati nel funzionamento operativo.
- L'aggiornamento OTA sostituisce soltanto la partizione applicativa; NVS,
  calibrazioni, configurazione di rete e dati su microSD restano invariati.
- Il riavvio programmato usa chiavi NVS indipendenti e non passa dal salvataggio
  del blocco `calibration_v1`.

## Firmware 2.1.3 — 6 settembre 2026

- Ripristino di fabbrica rimosso completamente dal tasto alimentazione: PWR
  gestisce soltanto lo spegnimento; BOOT resta l'unica sorgente del reset a 10 secondi.
- Il conto alla rovescia e il risveglio forzato del display sono associati soltanto
  a BOOT.
- `35/35` test API del simulatore superati; build V1/V2 riuscite e artefatti OTA
  firmati generati. RAM statica `59.036 / 327.680 byte` (`18,0%`), flash
  applicazione `1.718.662 / 6.291.456 byte` (`27,3%`).

## Firmware 2.1.2 — 6 settembre 2026

- Corretto il risveglio forzato del controller LCD e della retroilluminazione
  durante il conto alla rovescia del ripristino, anche se il display era spento.
- BOOT e tasto alimentazione condividono il ripristino a 10 secondi; rilasciare
  il tasto alimentazione tra 2 e 10 secondi mantiene lo spegnimento ordinario.
- `35/35` test API del simulatore superati; build V1/V2 riuscite e artefatti OTA
  firmati generati. RAM statica `59.036 / 327.680 byte` (`18,0%`), flash
  applicazione `1.718.914 / 6.291.456 byte` (`27,3%`).

## Firmware 2.1.1 — 6 settembre 2026

- `34/34` test API del simulatore superati, inclusi persistenza dello
  spegnimento automatico, mantenimento del portale durante il provisioning,
  presenza dei controlli nel portale e riavvio manuale.
- Test C++ di `ScaleCore` e `ReliabilityCore` superati con
  `-Wall -Wextra -Werror`.
- Build PlatformIO V2 e V1 riuscite: RAM statica
  `59.036 / 327.680 byte` (`18,0%`), flash applicazione
  `1.718.506 / 6.291.456 byte` (`27,3%`). Generati i nuovi artefatti OTA V1 e
  V2 con la chiave di produzione ruotata; entrambe le firme sono state
  verificate indipendentemente con OpenSSL.
- Portale verificato con WebKit in emulazione iPhone a 393 px: documento senza
  overflow (`393/393`), campi data larghi 337 px e contenuti entro il margine
  destro a 366 px. Visibili anche controlli di spegnimento e riavvio.
- Upload USB del profilo V2 su ESP32-S3 riuscito con verifica hash, senza
  cancellare la NVS. Seriale: `firmware_version=2.1.1`, access point di recupero
  attivo su `192.168.4.1`, touch CST328 rilevato via fallback, microSD montata,
  IMU/RTC/batteria disponibili e intervallo acquisizione massimo 20 ms.
- La NVS preesistente è rimasta intatta: SSID, IP statico, capacità batteria e
  stato predefinito del display sono stati riletti dopo il flash.
- Dopo l'installazione USB della nuova chiave pubblica è stato eseguito un OTA
  reale da `1.748.112 byte`: firma verificata sul dispositivo, riavvio sulla
  partizione aggiornata e registro concluso con `ota_boot_validated` e
  `signature_verified=true`.

Il flusso captive è stato verificato anche sul dispositivo mobile impiegato
nell'impianto; il relativo collaudo è completato.

## Firmware 2.1.0 — 5 settembre 2026

La versione integra le correzioni già pubblicate fino a `6837098` (2.0.5),
comprese ordine dei byte RGB565, volume audio, arresto I2S, microSD e cache web.

### Software e build

- `30/30` test API del simulatore superati, compresi ordine sensori,
  conservazione dei punti fisici, soglie di chiusura e controlli audio esistenti.
- Test C++ dei moduli reali `ScaleCore` e `ReliabilityCore` superati con
  `-Wall -Wextra -Werror`: stabilità e campioni interrotti, magnete invalido,
  rollover timer, permutazioni, sovrapposizione circolare, RTC/BCD/calendario,
  colpo-quiete, pausa, timeout, campioni IMU mancanti e curva batteria.
- Build PlatformIO V2 e V1 riuscite: RAM statica `59.004 / 327.680 byte`
  (`18,0%`), flash applicazione `1.713.714 / 6.291.456 byte` (`27,2%`).
- Immagini factory e OTA firmate per entrambi i profili. Firma ECDSA verificata
  indipendentemente con la chiave pubblica incorporata nel firmware.
- Chromium: Riepilogo, Calibrazione e Sistema senza overflow orizzontale a
  375/768/1024/1440 px; riordino e impostazioni di chiusura verificati nel
  simulatore. Nessuna eccezione JavaScript nei flussi controllati; le richieste
  fallite durante il riavvio volontario del simulatore sono attese.

### Scheda collegata via USB

- Backup completo della flash da 16 MiB conservato localmente, escluso da Git.
- Flash finale su COM5 riuscito con verifica hash e riavvio, mantenendo la
  partizione NVS. Seriale: `firmware_version=2.1.0`.
- **Colori del display confermati corretti dall'utente dopo l'ultimo flash.**
- SanDisk Extreme microSDXC da 128 GB (B07FCMKK5X) montata; prova di
  lettura/scrittura riuscita.
- Touch CST328 rilevato tramite fallback, QMI8658 e PCF85063 raggiungibili;
  batteria letta a circa 4,21 V. Speaker inizializzato. Questi stati non
  attestano tutte le gesture, accuratezza dei sensori o qualità del suono.
- Trenta interrogazioni di stato in 15 secondi dopo autodiagnosi: 49–50
  scansioni/s, intervallo massimo osservato 28 ms, zero ritardi oltre 30 ms,
  zero perdite nella coda di acquisizione; heap stabile a 128.520 byte e
  stesso ID di avvio. **Prova eseguita senza AS5600 esterni collegati.**
- RTC presente ma non ancora sincronizzato: ora dichiarata indisponibile.
  Rilevazione bascula e suggerimento di completamento entrambi disabilitati.

### Collaudo sull'impianto completato

Le limitazioni della verifica USB del 5 settembre sono state superate dal
successivo collaudo sull'impianto installato. I quattro AS5600, le pesate,
l'integrazione HTTPS/MQTT, i quattro rami I2C, l'RTC, le soglie meccaniche,
il touch e l'alimentazione sono stati verificati nel funzionamento operativo.
Le code restano volutamente limitate e non costituiscono una garanzia di
consegna durante interruzioni di rete o riavvii.

## Firmware 2.0.5 e verifica hardware - 4 settembre 2026

- `28/28` test Node.js del simulatore superati;
- asset CSS e JavaScript versionati e serviti senza cache, così il pannello non può riutilizzare il codice di un firmware precedente;
- corretto l'ordine dei byte RGB565 sul bus del display: il blu `#243b6b`, il fondo `#f5f7fa`, i pannelli bianchi e il testo antracite non vengono più trasformati in oro, verde e viola;
- volume PCM5101 regolabile dal portale tra 0 e 100%, persistente e condiviso da conferma sonora e bip di prova;
- boot display di 10 secondi con logo CaskLogic centrale e barra di avanzamento;
- aggiornamento incrementale dei valori live senza ricostruire card e pannelli;
- endpoint e pulsante `Prova bip` limitati a un singolo suono breve; DMA in auto-clear, canale eliminato a fine suono e linee I2S portate a zero;
- build PlatformIO V2/CST3530 completata con RAM statica al `16,6%` e flash applicazione al `26,5%`;
- caricamento USB riuscito su ESP32-S3 con flash da 16 MB e PSRAM da 8 MB;
- avvio seriale confermato con `firmware=2.0.5` e access point di recupero attivo;
- SSID originale ripristinato a `LP-PW_casklogic-192_168_4_1`.
- SanDisk Extreme microSDXC da 128 GB montata e verificata (`121942 MB` fisici,
  `121911 MB` filesystem) senza più conflitti sul clock SD_MMC.

## Porting Waveshare ESP32-S3-Touch-LCD-2.8 - 1 settembre 2026

Verifica eseguita senza la nuova scheda fisica:

- `24/24` test Node.js del simulatore superati;
- build PlatformIO V2/CST3530 completata;
- build PlatformIO V1/CST328 completata;
- RAM statica `54.436 / 327.680 byte` (`16,6%`);
- flash applicazione `1.657.078 / 6.291.456 byte` (`26,3%`);
- immagini factory e OTA ECDSA-P256 firmate per entrambi i profili;
- dashboard verificata in Chromium con gli stessi asset incorporati;
- interruttori display e speaker indipendenti e persistenti nel simulatore;
- telemetria simulata di touch, speaker, batteria, QMI8658 e PCF85063 visibile;
- nessun errore JavaScript di esecuzione osservato nel flusso controllato.

Quella compilazione verificava API e compatibilità software. Il successivo
collaudo sull'impianto ha completato la verifica di ST7789, touch, audio PCM5101,
GPIO batteria, RTC, IMU, microSD SD_MMC, power hold e I2C esterno secondo la
[`checklist hardware`](friday-hardware-validation.md).

## Prove storiche sul precedente prototipo ESP32-C6

## Ambiente osservato

- Waveshare ESP32-C6-LCD-1.47 collegata via USB-C;
- multiplexer rilevato a `0x70`;
- un AS5600 collegato al canale 0 durante l'ultima prova;
- SDA GPIO 1, SCL GPIO 2;
- I2C a 100 kHz;
- seriale a 115200 baud.

## Risultati confermati

- compilazione completata con core Arduino ESP32;
- caricamento su ESP32-C6 completato e verificato dall'utility di flash;
- display inizializzato e aggiornamento limitato alle righe modificate;
- multiplexer e AS5600 raggiungibili;
- scansione aggregata osservata circa 80–101 volte al secondo;
- durata di una scansione completa osservata circa 4,2–5,9 ms;
- magnete rimosso: `raw=0`, `MD=false`, `ML=true`, magnitudine circa 0–1;
- magnete vicino: `raw` circa 3215–3221, `MD=true`, `ML=true`, magnitudine
  circa 1080–1100.

## Limiti della prova diagnostica storica

La prova sul precedente prototipo ESP32-C6 non percorse l'intero intervallo
0–4095 perché impiegava un solo sensore in una configurazione magnetica
provvisoria. Il successivo collaudo del gateway ESP32-S3 installato ha verificato
centraggio, distanza, giri completi, quattro sensori, scatti meccanici, isteresi,
vibrazioni e corrispondenza con l'indicazione della pesa.

## Nota metodologica

La frequenza del ciclo non dimostra da sola che l'esperienza sul display sia
corretta. Il collaudo successivo ha quindi acquisito anche valori grezzi,
minimi, massimi, variazioni e riscontro visivo del magnete.

## Verifica virtuale del gateway operativo

In assenza del dispositivo, il firmware in `firmware/production-gateway` è
stato verificato con strumenti host:

- compilazione PlatformIO per `esp32-c6-devkitc-1` con Arduino ESP32;
- RAM statica `50.708 / 327.680 byte` (`15,5%`);
- flash applicazione `1.686.942 / 2.031.616 byte` (`83,0%`);
- immagine OTA firmata `1.744.992 byte`, con `286.624 byte` (`14,1%`) liberi
  nello slot;
- generazione automatica e inclusione degli asset web nel binario;
- test Node.js del simulatore REST e delle regole di configurazione;
- 20 test Node.js superati su API, segreti, calibrazione, storico, filtri,
  export, TLS, CSRF, header di sicurezza, autodiagnosi, ZIP anonimizzato,
  metriche, configurazione remota e OTA firmato;
- firma ECDSA-P256 dell'immagine OTA verificata indipendentemente con OpenSSL;
- controllo sintattico del JavaScript eseguito nel browser;
- verifica browser dell'interfaccia desktop e mobile tramite Playwright;
- viewport verificati: `1440x1000` e `390x844`, tema chiaro e scuro;
- controllo dei flussi display, calibrazione, storico, impostazioni e
  autodiagnosi, inclusi grafici canvas non vuoti e tabelle mobili scorrevoli.

Queste prove storiche verificavano compilabilità, logica indipendente
dall'hardware, contratto HTTP e comportamento dell'interfaccia. All'epoca non
verificavano fisicamente:

- presenza e scrittura della microSD;
- letture I2C dei quattro AS5600 attraverso il TCA9546A;
- inizializzazione e resa del display ST7789;
- stabilità della rete Wi-Fi sull'impianto;
- aggiornamento OTA e ripristino dopo un'interruzione di alimentazione;
- autonomia o commutazione dell'eventuale alimentazione di backup;
- consegna degli eventi a un backend CaskLogic, che non era stato modificato.

Il collaudo successivo del gateway installato ha completato tali verifiche.

Le misure finali di RAM e flash della build sono riportate nel README del
gateway operativo e devono essere ricontrollate a ogni rilascio.
