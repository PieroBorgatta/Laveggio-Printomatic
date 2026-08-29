# Laveggio Printomatic

Digitalizzazione non invasiva del selettore di peso di un bilico meccanico
Laveggio Printomatic del 1965.

Il progetto usa quattro sensori magnetici AS5600, un multiplexer I2C a quattro
canali e una scheda Waveshare ESP32-C6-LCD-1.47. L'obiettivo è leggere le
quattro manopole senza modificare il funzionamento meccanico originale e, in
una fase successiva, inviare il peso al gestionale di pesatura.

> [!IMPORTANT]
> Il repository contiene il firmware diagnostico provato sul dispositivo e il
> gateway operativo completo, compilato per ESP32-C6 e verificato tramite
> simulatore host. Cablaggio, sensori, microSD, Wi-Fi e integrazione backend del
> gateway operativo devono ancora essere collaudati sull'hardware reale.

## Struttura del progetto

| Area | Stato | Contenuto |
| --- | --- | --- |
| [`firmware/calibration-reader`](firmware/calibration-reader/) | In prova | Lettura dei quattro canali, diagnostica I2C/AS5600 e visualizzazione locale |
| [`firmware/production-gateway`](firmware/production-gateway/) | Implementato, da collaudare | Firmware Wi-Fi, calibrazione web, storico microSD, OTA e integrazione backend |
| [`docs/hardware.md`](docs/hardware.md) | Documentato | Componenti e collegamenti di riferimento |
| [`docs/wiring.md`](docs/wiring.md) | Documentato | Schema testuale e avvertenze sui colori dei cavetti |
| [`docs/production-architecture.md`](docs/production-architecture.md) | Implementato lato dispositivo | Flusso ESP32, contratto backend e regole kiosk |
| [`docs/test-results.md`](docs/test-results.md) | Aggiornato | Risultati osservati e limiti ancora aperti |
| [`stl`](stl/) | In sviluppo | Case e supporti stampabili in 3D, separati per autore e licenza |

## Hardware utilizzato

- [Waveshare ESP32-C6-LCD-1.47 — acquisto Amazon](https://www.amazon.it/dp/B0DHTMYTCY)
- [Adafruit PCA9546 / TCA9546A, multiplexer I2C STEMMA QT a 4 canali — acquisto Amazon](https://www.amazon.it/dp/B0BSG8KX8L)
- [Kit di 4 moduli AS5600 a 12 bit con magneti — acquisto Amazon](https://www.amazon.it/dp/B0FH1Y3GLG)
- [5 coppie di cavetti micro JST-SH 1.0 mm, 4 pin — acquisto Amazon](https://www.amazon.it/dp/B0BNCHC5Q4)
- [Documentazione Waveshare ESP32-C6-LCD-1.47](https://docs.waveshare.com/ESP32-C6-LCD-1.47?variant=ESP32-C6-LCD-1.47)
- [Pinout Adafruit PCA9546 / TCA9546A](https://learn.adafruit.com/adafruit-pca9546-4-channel-stemma-qt-multiplexer/pinouts)
- [Manuale del modulo AS5600 utilizzato come riferimento](https://manuals.plus/asin/B0FH2G8PLS)
- [Datasheet ufficiale ams OSRAM AS5600](https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf)
- Alimentazione USB-C

Il multiplexer del prototipo risponde a `0x70`; ogni AS5600 risponde a `0x36`
all'interno del proprio canale isolato.

## Avvio rapido del firmware diagnostico

1. Installare [Arduino IDE](https://www.arduino.cc/en/software) e il core
   `esp32` di Espressif.
2. Aprire
   `firmware/calibration-reader/calibration-reader.ino`.
3. Selezionare la scheda ESP32-C6 con flash da 8 MB e `USB CDC On Boot`.
4. Verificare il cablaggio descritto in [`docs/wiring.md`](docs/wiring.md).
5. Compilare e caricare lo sketch.
6. Aprire il monitor seriale a `115200 baud`.

Lo schermo mostra `S0`–`S3`, il valore grezzo `0–4095` e lo stato del magnete.
La seriale emette ogni secondo un riepilogo JSON con frequenza di scansione,
durata, stato del bus, angolo, AGC e magnitudine.

## Stato attuale

- ESP32-C6, display e multiplexer rilevati e funzionanti.
- Bus verificato con `SDA=GPIO1`, `SCL=GPIO2`, multiplexer `0x70` e AS5600
  `0x36`.
- Lettura del registro `RAW ANGLE` (`0x0C`–`0x0D`) implementata.
- Aggiornamento parziale delle sole righe modificate, senza ridisegnare
  continuamente l'intero display.
- Durante le ultime prove è stato rilevato un campo magnetico debole e una
  variazione angolare limitata. La validazione meccanica con magnete centrato e
  diametrale resta da completare.
- Gateway Wi-Fi, calibrazione persistente, storico microSD, interfaccia web,
  aggiornamento OTA e pubblicazione HTTPS verso il backend sono implementati.
- L'integrazione nel gestionale non appartiene a questo repository: il relativo
  contratto è in
  [`docs/casklogic-integration-contract.md`](docs/casklogic-integration-contract.md).
- Il gateway operativo è stato compilato e testato virtualmente, ma resta da
  collaudare sul dispositivo e sulla meccanica reale prima dell'uso operativo.

## Parti stampabili in 3D

La cartella [`stl/`](stl/) distingue due provenienze:

- `third-party`: modelli scaricati da terzi, conservati con attribuzione, fonte
  e licenza originali;
- `original-designs`: futuri case e supporti progettati specificamente per
  Laveggio Printomatic da Piero Borgatta. I collegamenti MakerWorld saranno
  aggiunti quando i modelli verranno pubblicati.

Il case attualmente presente per il multiplexer Adafruit deriva dal modello
[Enclosure for Adafruit 4ch QT Mux 5664 PCA9546](https://www.printables.com/model/658753-enclosure-for-adafruit-4ch-qt-mux-5664-pca9546/files)
di OpenSensor.io ed è distribuito dall'autore con licenza CC BY-NC-SA 4.0.

## Licenza

Salvo indicazione diversa nei singoli file, il progetto è distribuito con
licenza [Creative Commons Attribution 4.0 International](LICENSE). È richiesta
l'attribuzione a Piero Borgatta e ai contributori del progetto.

Gli STL di terzi non sono coperti dalla licenza generale del repository: fanno
fede il relativo README, il file di licenza nella loro cartella e
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
