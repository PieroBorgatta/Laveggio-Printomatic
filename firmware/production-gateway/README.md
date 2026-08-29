# Firmware operativo Laveggio Printomatic

Firmware completo per la Waveshare ESP32-C6-LCD-1.47. Legge quattro AS5600
attraverso il PCA9546/TCA9546A, converte le posizioni in peso, registra gli
eventi sulla microSD e rende disponibile un pannello di amministrazione web.

## Funzioni implementate

- scansione continua dei quattro sensori a 100 kHz;
- calibrazione persistente delle dieci posizioni di ogni manopola;
- distanza angolare circolare, tolleranza, isteresi e finestra di stabilita;
- display locale spento per impostazione predefinita e controllabile dal web;
- storico append-only e log diagnostici su microSD FAT32;
- campione diagnostico dei quattro sensori registrato ogni minuto;
- Wi-Fi DHCP o statico, scansione reti e access point di recupero;
- autenticazione HTTP Basic dopo il provisioning iniziale;
- invio asincrono di snapshot completi e heartbeat al futuro backend CaskLogic;
- certificato CA configurabile e obbligatorio per endpoint HTTPS;
- rilevazione opzionale della perdita di alimentazione esterna;
- aggiornamento firmware OTA con doppia partizione;
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

- `.pio/build/waveshare_esp32c6_lcd_147/firmware.bin`: aggiornamento OTA;
- `.pio/build/waveshare_esp32c6_lcd_147/firmware.factory.bin`: prima installazione completa.

Misure della build `1.0.1` verificata il 29 agosto 2026:

- RAM statica: `49.508 / 327.680 byte` (`15,1%`);
- flash applicazione: `1.548.522 / 1.900.544 byte` (`81,5%`).

La RAM allocata dinamicamente per coda HTTPS, task e richieste web non e
compresa nel primo valore; prima dell'installazione operativa va quindi
controllato anche l'heap libero esposto nella pagina Sistema.

Il file `include/WebAssets.h` e generato automaticamente da `data/` prima della
compilazione. Deve essere versionato affinche il contenuto web incorporato resti
ispezionabile e riproducibile.

## Simulatore e test API

```powershell
npm test
npm run simulate
```

Il simulatore risponde su `http://127.0.0.1:4177` e consente di provare tutte le
sezioni dell'interfaccia senza la scheda collegata.

## Prima configurazione

Se non trova credenziali Wi-Fi salvate, il dispositivo crea:

- SSID `Laveggio-PW-casklogic`;
- password `casklogic`;
- indirizzo `http://192.168.4.1`.

Lo stesso access point viene attivato se la rete configurata non e raggiungibile.
Il dispositivo continua a tentare la riconnessione e spegne automaticamente
l'access point dopo 120 secondi consecutivi di connessione stabile alla rete
principale. Durante il primo provisioning l'interfaccia e raggiungibile dalla
rete creata dal dispositivo. Dopo il salvataggio del Wi-Fi, le pagine richiedono
l'utente `admin` e la password amministrativa generata mostrata sulla seriale;
la password va cambiata dalla sezione Sistema.

## File sulla microSD

La scheda deve essere FAT32. Il firmware crea:

```text
/weights/history.ndjson
/weights/history-YYYYMMDD-HHMMSS-<uptime>.ndjson
/logs/system.ndjson
/logs/system-YYYYMMDD-HHMMSS-<uptime>.ndjson
```

Lo storico viene scritto soltanto quando tutte le cifre sono valide, la misura
rimane stabile per la finestra configurata e il peso stabile cambia. I file
vengono ruotati con nomi univoci e gli archivi non vengono sovrascritti. La
pagina Storico legge i record piu recenti anche oltre le rotazioni; il comando
di export concatena in ordine tutti gli archivi NDJSON presenti sulla scheda.
L'esito di pubblicazione piu recente e visibile nello stato integrazione, mentre
il record locale rimane immutabile e non costituisce conferma di ricezione.
La pagina Sistema offre separatamente l'export completo dei log, inclusi i
campioni periodici di presenza, angolo, stato magnete, AGC e magnitudine.

## Limiti della verifica corrente

La compilazione e i flussi web sono verificabili senza hardware. Restano da
provare fisicamente: centraggio dei magneti, assorbimento, autonomia UPS,
scritture reali sulla microSD, commutazione di alimentazione, Wi-Fi del sito,
display, upload OTA e corrispondenza fra peso meccanico e digitale.
