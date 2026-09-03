# Hardware

## Distinta materiali aggiornata

| Componente | Quantita | Riferimento |
| --- | ---: | --- |
| Waveshare ESP32-S3-Touch-LCD-2.8, 16 MB flash / 8 MB PSRAM | 1 | [Documentazione ufficiale](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8) |
| Adafruit PCA9546/TCA9546A STEMMA QT a 4 canali | 1 | [Amazon.it - B0BSG8KX8L](https://www.amazon.it/dp/B0BSG8KX8L) |
| Kit con 4 moduli AS5600 a 12 bit e magneti | 1 | [Amazon.it - B0FH1Y3GLG](https://www.amazon.it/dp/B0FH1Y3GLG) |
| Cavetti micro JST-SH 1,0 mm a 4 pin | 1 confezione | [Amazon.it - B0BNCHC5Q4](https://www.amazon.it/dp/B0BNCHC5Q4) |
| Speaker 8 ohm 2 W 2030 | 1 | Incluso con la Waveshare |
| Batteria LiPo 3,7 V protetta | 1 | Connettore MX1.25 2 pin, polarita da verificare |
| MicroSD FAT32 | 1 | Capacita dichiarata dalla scheda fino a 16 GB |

Le inserzioni commerciali possono cambiare: prima di alimentare verificare
sempre serigrafia, tensione, connettore e polarita del componente ricevuto.

## Waveshare ESP32-S3-Touch-LCD-2.8

La scheda riunisce ESP32-S3, LCD ST7789 verticale 240x320, touch capacitivo,
slot microSD SD_MMC, audio PCM5101 con amplificatore, IMU QMI8658, RTC PCF85063,
gestione LiPo e misura della batteria su GPIO8. Il firmware riconosce entrambi i
controller touch documentati:

- revisione V2: CST3530;
- revisione V1: CST328.

Waveshare indica che la V1 e stata sostituita dalla V2; verificare la revisione
stampata sulla scheda prima del primo flash. I due profili compilati differiscono
solo nella preferenza iniziale del touch e mantengono un rilevamento di fallback.

## Multiplexer e sensori

Il PCA9546/TCA9546A isola quattro rami I2C, necessario perche tutti gli AS5600
usano l'indirizzo `0x36`. Il multiplexer atteso risponde a `0x70`; il bus esterno
della nuova scheda usa GPIO11 SDA e GPIO10 SCL a 100 kHz.

Ogni AS5600 restituisce 0-4095 su un giro. Per la lettura I2C servono solo VCC,
GND, SDA e SCL; DIR, GPO e OUT rimangono scollegati. Il magnete deve essere
diametralmente magnetizzato, centrato e ruotare sul proprio asse. Distanza e
centraggio vanno validati con i flag MD, ML e MH e con una rotazione completa.

## Alimentazione

La scheda puo essere alimentata via USB-C o tramite una LiPo 1S sul connettore
dedicato. Multiplexer e sensori ricevono 3,3 V dalla scheda e condividono la
massa. La precedente scheda esterna bq25185 non e piu necessaria nel cablaggio
standard: non collegarla in parallelo al caricatore integrato senza un progetto
elettrico specifico che impedisca ritorni di corrente.

Riferimenti:

- [Waveshare ESP32-S3-Touch-LCD-2.8](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8)
- [PCA9546 Adafruit](https://learn.adafruit.com/adafruit-pca9546-4-channel-stemma-qt-multiplexer/pinouts)
- [Datasheet AS5600](https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf)
