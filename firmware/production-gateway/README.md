# Firmware operativo CaskLogic PesaLink

Firmware completo per la Waveshare ESP32-S3-Touch-LCD-2.8. Legge quattro AS5600
attraverso il PCA9546/TCA9546A, converte le posizioni in peso, registra gli
eventi sulla microSD e rende disponibile un pannello di amministrazione web.

Il dispositivo digitalizza la pesa meccanica esclusivamente per uso interno e
non fiscale. Le letture non sostituiscono uno strumento di pesatura omologato e
non sono valide per transazioni commerciali o verifiche metrologiche legali.

## Funzioni implementate

- scansione continua dei quattro sensori a 100 kHz;
- calibrazione persistente delle dieci posizioni di ogni manopola;
- distanza angolare circolare, tolleranza, isteresi e finestra di stabilità;
- display ST7789 240×320 controllabile dal web, con cinque pagine a card, swipe orizzontale, scroll verticale, footer touch e fallback BOOT;
- touch con autodetect CST3530 per V2 e CST328 per V1;
- doppio tono su speaker PCM5101 alla conferma della pesata, disabilitabile e persistente dal portale;
- volume speaker persistente da 0 a 100%, applicato sia alla conferma sia alla prova audio manuale;
- palette copiata dal portale interno del dispositivo e rendering incrementale per eliminare i lampeggiamenti degli aggiornamenti live;
- monitoraggio integrato di batteria GPIO8, QMI8658 e RTC PCF85063;
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
- tensione e stima percentuale della batteria integrata su GPIO8;
- rilevazione opzionale della perdita di alimentazione esterna;
- autodiagnosi, grafici delle ultime 24 ore e pacchetto assistenza ZIP
  anonimizzato;
- controllo periodico della microSD e contatori di errore/magnete per sensore;
- aggiornamento firmware OTA a blocchi, firmato ECDSA-P256, con avanzamento,
  doppia partizione, rollback e registro degli aggiornamenti;
- simulatore locale che usa gli stessi file HTML, CSS e JavaScript incorporati.

## Compilazione

Il progetto usa Arduino ESP32 3.x tramite la piattaforma pioarduino. Il profilo
predefinito è la revisione Waveshare V2 con touch CST3530; il secondo profilo
mantiene la compatibilità con la V1 dotata di CST328. Su Windows conviene usare
una cache PlatformIO corta per evitare il limite storico dei percorsi:

```powershell
py -m pip install --user platformio
$env:PLATFORMIO_CORE_DIR = 'C:\pio'
py -m platformio run -e waveshare_esp32s3_touch_lcd_28_v2
py -m platformio run -e waveshare_esp32s3_touch_lcd_28_v1
```

Artefatti principali:

- `.pio/build/<profilo>/firmware.signed.bin`: aggiornamento OTA accettato dal
  portale;
- `.pio/build/<profilo>/firmware.bin`: binario non firmato usato come input
  della firma, non caricabile dal portale;
- `.pio/build/<profilo>/firmware.factory.bin`: prima installazione completa,
  da usare quando si passa dalla vecchia ESP32-C6 alla ESP32-S3.

Misure della build `2.1.0` verificata il 5 settembre 2026 sui profili Waveshare V2/V1:

- RAM statica: `59.004 / 327.680 byte` (`18,0%`);
- flash applicazione: `1.713.714 / 6.291.456 byte` (`27,2%`).

Le due partizioni OTA occupano `0x600000` byte, cioè 6 MiB ciascuna, sulla flash
da 16 MB. La partizione SPIFFS è stata rimossa perché gli asset web sono
incorporati nel firmware e i dati operativi risiedono sulla microSD.

La RAM allocata dinamicamente per coda HTTPS, task e richieste web non è
compresa nel primo valore; prima dell'installazione operativa va quindi
controllato anche l'heap libero esposto nella pagina Sistema.

Il file `include/WebAssets.h` è generato automaticamente da `data/` prima della
compilazione. Deve essere versionato affinché il contenuto web incorporato resti
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

Lo stesso access point viene attivato se la rete configurata non è raggiungibile.
Il dispositivo continua a tentare la riconnessione e spegne automaticamente
l'access point dopo 120 secondi consecutivi di connessione stabile alla rete
principale. Durante il primo provisioning l'interfaccia è raggiungibile dalla
rete creata dal dispositivo e richiede già l'utente `admin` e la password
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
pagina Storico legge soltanto le 20 righe più recenti all'apertura e applica
filtri e ordinamento lato dispositivo; l'export usa gli stessi filtri attivi.
L'esito di pubblicazione più recente è visibile nello stato integrazione, mentre
il record locale rimane immutabile e non costituisce conferma di ricezione.
La pagina Sistema offre separatamente l'export completo dei log, inclusi i
campioni periodici di presenza, angolo, stato magnete, AGC e magnitudine.

Le regole di esposizione in rete, il limite dell'HTTP locale e la cifratura
verso CaskLogic sono descritte in [`../../docs/security.md`](../../docs/security.md).

## Limiti della verifica corrente

Le build ESP32-S3 V2 e V1, la firma OTA, i test host e il portale in Chromium
sono stati verificati senza la nuova scheda. Non sono ancora prova fisica di
display, touch, speaker, batteria, RTC, IMU, SD_MMC o bus I2C esterno. La prima
installazione richiede il file factory tramite USB: un OTA della precedente
ESP32-C6 non può trasformare o migrare l'hardware.

La sequenza completa per il collaudo è in
[`../../docs/friday-hardware-validation.md`](../../docs/friday-hardware-validation.md).

## Versione 2.1.0

Acquisizione dedicata a 50 Hz, trasporti prioritari indipendenti, RTC UTC offline,
calibrazioni versionate con CRC, riordino dei canali, diagnosi del rumore,
conferme distinte di salvataggio/consegna e chiusura bascula sperimentale.
La rilevazione è inizialmente disattivata. Parametri e prove:
[guida 2.1](../../docs/reliability-2.1.md).

Per i test C++ del nucleo, con un compilatore host:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude src/ScaleCore.cpp src/ReliabilityCore.cpp tests/reliability_test.cpp -o reliability-test
./reliability-test
```

Per aggiornare una scheda già configurata usare OTA firmato o l'upload PlatformIO
con file separati. Il binario factory combinato può cancellare la NVS: riservarlo
alla prima installazione. Il comando seriale `status` restituisce lo stato corrente.
