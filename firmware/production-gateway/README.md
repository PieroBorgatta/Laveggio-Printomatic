# Firmware operativo Laveggio Printomatic

Firmware completo per la Waveshare ESP32-C6-LCD-1.47. Legge quattro AS5600
attraverso il PCA9546/TCA9546A, converte le posizioni in peso, registra gli
eventi sulla microSD e rende disponibile un pannello di amministrazione web.

## Funzioni implementate

- scansione continua dei quattro sensori a 100 kHz;
- calibrazione persistente delle dieci posizioni di ogni manopola;
- distanza angolare circolare, tolleranza, isteresi e finestra di stabilita;
- display locale controllabile dal web, con stato persistente dopo il riavvio e cinque pagine operative selezionabili con un clic breve su BOOT;
- storico e log NDJSON settimanali in cartelle anno/mese su microSD FAT32;
- campione diagnostico dei quattro sensori registrato ogni minuto;
- sincronizzazione NTP richiesta subito dopo il Wi-Fi e completata senza
  bloccare l'avvio o la lettura dei sensori;
- Wi-Fi DHCP o statico, scansione reti e access point di recupero;
- autenticazione HTTP Basic, rate limit, CSRF e header browser restrittivi;
- invio asincrono HTTPS e MQTT TLS opzionale verso il futuro backend CaskLogic;
- firma HMAC-SHA256 delle pesate, endpoint Prometheus e configurazione remota
  versionata con whitelist e copia locale valida;
- nessuna coda persistente di trasmissione: la microSD conserva lo storico, ma
  gli eventi MQTT non consegnati non vengono accodati per un reinvio;
- CA obbligatoria, soli endpoint HTTPS e certificato client mTLS opzionale;
- heartbeat con watchdog opzionale e protezione persistente dai reboot loop;
- storico filtrabile e ordinabile, con sole 20 righe al primo caricamento ed
  export coerente con i filtri;
- rotazione dimensionale e retention delle pesate configurabili;
- tensione e stima percentuale della batteria opzionali su GPIO0;
- rilevazione opzionale della perdita di alimentazione esterna;
- autodiagnosi, grafici delle ultime 24 ore e pacchetto assistenza ZIP
  anonimizzato;
- controllo periodico della microSD e contatori di errore/magnete per sensore;
- aggiornamento firmware OTA a blocchi, firmato ECDSA-P256, con avanzamento,
  doppia partizione, rollback e registro degli aggiornamenti;
- simulatore locale che usa gli stessi file HTML, CSS e JavaScript incorporati.

## Compilazione

Il supporto Arduino ufficiale di PlatformIO non abilita ancora ESP32-C6. Il
progetto usa quindi la piattaforma comunitaria pioarduino, che installa Arduino
ESP32 3.x. Su Windows conviene abilitare i percorsi lunghi oppure usare una
cache corta solo per il comando di build:

```powershell
py -m pip install --user platformio
$env:PLATFORMIO_CORE_DIR = 'C:\pio'
py -m platformio run
```

Artefatti principali:

- `.pio/build/waveshare_esp32c6_lcd_147/firmware.signed.bin`: aggiornamento OTA
  accettato dal portale;
- `.pio/build/waveshare_esp32c6_lcd_147/firmware.bin`: binario non firmato
  usato come input della firma, non caricabile dal portale;
- `.pio/build/waveshare_esp32c6_lcd_147/firmware.factory.bin`: prima installazione completa.

Misure della build `1.3.0` verificata il 31 agosto 2026:

- RAM statica: `51.124 / 327.680 byte` (`15,6%`);
- flash applicazione: `1.708.468 / 2.031.616 byte` (`84,1%`).

Le due partizioni OTA occupano `0x1F0000` byte ciascuna. La partizione SPIFFS
e stata rimossa perche gli asset web sono incorporati nel firmware e i dati
operativi risiedono sulla microSD. Il file OTA firmato verificato lascia
`263.392 byte` (`13,0%`) in ciascuno slot.

La RAM allocata dinamicamente per coda HTTPS, task e richieste web non e
compresa nel primo valore; prima dell'installazione operativa va quindi
controllato anche l'heap libero esposto nella pagina Sistema.

Il file `include/WebAssets.h` e generato automaticamente da `data/` prima della
compilazione. Deve essere versionato affinche il contenuto web incorporato resti
ispezionabile e riproducibile.

La procedura di gestione delle chiavi e descritta in
[`../../docs/firmware-signing.md`](../../docs/firmware-signing.md). La chiave
privata non appartiene al repository e deve essere custodita e sottoposta a
backup separatamente.

## Simulatore e test API

```powershell
npm test
npm run simulate
```

Il simulatore risponde su `http://127.0.0.1:4177` e consente di provare tutte le
sezioni dell'interfaccia senza la scheda collegata.

## Prima configurazione

Se non trova credenziali Wi-Fi salvate, il dispositivo crea:

- SSID `LP-PW_casklogic-192_168_4_1`;
- password `casklogic`;
- indirizzo `http://192.168.4.1`.

Lo stesso access point viene attivato se la rete configurata non e raggiungibile.
Il dispositivo continua a tentare la riconnessione e spegne automaticamente
l'access point dopo 120 secondi consecutivi di connessione stabile alla rete
principale. Durante il primo provisioning l'interfaccia e raggiungibile dalla
rete creata dal dispositivo e richiede gia l'utente `admin` e la password
iniziale `casklogic`. Le stesse credenziali proteggono il portale
dopo il salvataggio del Wi-Fi. La password non viene stampata sulla seriale e
va cambiata dalla sezione Sistema prima dell'uso operativo.

## File sulla microSD

La scheda deve essere FAT32. Il firmware crea:

```text
/weights/YYYY/MM/history-YYYY-Www.ndjson
/logs/YYYY/MM/system-YYYY-Www.ndjson
/weights/unsynced/history-<boot-id>.ndjson
/logs/unsynced/system-<boot-id>.ndjson
/updates/registry.ndjson
```

Lo storico viene scritto soltanto quando tutte le cifre sono valide, la misura
rimane stabile per la finestra configurata e il peso stabile cambia. I file
vengono ruotati con nomi univoci e gli archivi non vengono sovrascritti. La
pagina Storico legge soltanto le 20 righe piu recenti all'apertura e applica
filtri e ordinamento lato dispositivo; l'export usa gli stessi filtri attivi.
L'esito di pubblicazione piu recente e visibile nello stato integrazione, mentre
il record locale rimane immutabile e non costituisce conferma di ricezione.
La pagina Sistema offre separatamente l'export completo dei log, inclusi i
campioni periodici di presenza, angolo, stato magnete, AGC e magnitudine.

Le regole di esposizione in rete, il limite dell'HTTP locale e la cifratura
verso CaskLogic sono descritte in [`../../docs/security.md`](../../docs/security.md).

## Limiti della verifica corrente

La build 1.3.0 e stata installata tramite OTA firmato sulla scheda reale; Wi-Fi,
riavvio automatico, validazione della nuova partizione e persistenza del display
sono stati verificati. Restano da provare fisicamente: centraggio dei magneti,
assorbimento, autonomia UPS, scritture reali sulla microSD FAT32, commutazione
di alimentazione, rollback provocato e corrispondenza fra peso meccanico e digitale.
