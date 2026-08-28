# Laveggio Printomatic

Digitalizzazione non invasiva del selettore di peso di un bilico meccanico
Laveggio Printomatic del 1965.

Il progetto usa quattro sensori magnetici AS5600, un multiplexer I2C a quattro
canali e una scheda Waveshare ESP32-C6-LCD-1.47. L'obiettivo è leggere le
quattro manopole senza modificare il funzionamento meccanico originale e, in
una fase successiva, inviare il peso al gestionale di pesatura.

> [!IMPORTANT]
> Il repository contiene un firmware diagnostico realmente provato e una
> specifica del futuro firmware operativo. L'invio Wi-Fi al gestionale **non è
> ancora implementato** e non viene presentato come funzionante.

## Struttura del progetto

| Area | Stato | Contenuto |
| --- | --- | --- |
| [`firmware/calibration-reader`](firmware/calibration-reader/) | In prova | Lettura dei quattro canali, diagnostica I2C/AS5600 e visualizzazione locale |
| [`firmware/production-gateway`](firmware/production-gateway/) | Da implementare | Requisiti del firmware definitivo Wi-Fi e integrazione gestionale |
| [`docs/hardware.md`](docs/hardware.md) | Documentato | Componenti e collegamenti di riferimento |
| [`docs/wiring.md`](docs/wiring.md) | Documentato | Schema testuale e avvertenze sui colori dei cavetti |
| [`docs/production-architecture.md`](docs/production-architecture.md) | Proposta | Flusso ESP32, backend e kiosk |
| [`docs/test-results.md`](docs/test-results.md) | Aggiornato | Risultati osservati e limiti ancora aperti |

## Hardware utilizzato

- [Waveshare ESP32-C6-LCD-1.47](https://docs.waveshare.com/ESP32-C6-LCD-1.47?variant=ESP32-C6-LCD-1.47)
- [Adafruit PCA9546 / TCA9546A, multiplexer I2C STEMMA QT a 4 canali](https://learn.adafruit.com/adafruit-pca9546-4-channel-stemma-qt-multiplexer/pinouts)
- 4 moduli AS5600 a 12 bit, indirizzo I2C fisso `0x36`
- [Manuale del modulo AS5600 utilizzato come riferimento](https://manuals.plus/asin/B0FH2G8PLS)
- [Datasheet ufficiale ams OSRAM AS5600](https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf)
- 4 magneti diametralmente magnetizzati compatibili con AS5600
- Cavetti e connettori JST-SH/STEMMA QT, alimentazione USB-C

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
- Wi-Fi, protocollo verso backend, calibrazione persistente e integrazione kiosk
  restano da sviluppare.

## Licenza

Salvo indicazione diversa nei singoli file, il progetto è distribuito con
licenza [Creative Commons Attribution 4.0 International](LICENSE). È richiesta
l'attribuzione a Piero Borgatta e ai contributori del progetto.
