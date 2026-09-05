# Hardware

## Distinta materiali aggiornata

| Componente | Quantità | Riferimento |
| --- | ---: | --- |
| Waveshare ESP32-S3-Touch-LCD-2.8, 16 MB flash / 8 MB PSRAM | 1 | [Documentazione ufficiale](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8) |
| Adafruit PCA9546 compatto a 4 canali, cod. 5663 (compatibile TCA9546A) | 1 | [Amazon.it - B0BSF13WD7](https://www.amazon.it/dp/B0BSF13WD7) |
| Kit con 4 moduli AS5600 a 12 bit e magneti | 1 | [Amazon.it - B0FH1Y3GLG](https://www.amazon.it/dp/B0FH1Y3GLG) |
| Cavetti micro JST-SH 1,0 mm a 4 pin | 1 confezione | [Amazon.it - B0BNCHC5Q4](https://www.amazon.it/dp/B0BNCHC5Q4) |
| Pigtail IPEX-1/U.FL verso SMA femmina, 15 cm | 1 confezione da 5 | [Amazon.it - B07YBYMBSV](https://www.amazon.it/dp/B07YBYMBSV) |
| Antenna Wi-Fi 2,4 GHz 2 dBi, SMA maschio | 1 confezione da 2 | [Amazon.it - B0CR5JPMNX](https://www.amazon.it/dp/B0CR5JPMNX) |
| Cavo dati Lapp LiYY 4 x 0,14 mm2, nero | 1 spezzone da 10 m | [Amazon.it - B0C69CJYZT](https://www.amazon.it/dp/B0C69CJYZT) |
| Kit viti a macchina svasate nere M2 / M2,5 / M3, acciaio al carbonio | 1 kit da 500 | [Amazon.it - B0D1N4D5ZR](https://www.amazon.it/dp/B0D1N4D5ZR) |
| Speaker 8 ohm 2 W 2030 | 2 | Inclusi con la Waveshare |
| Batteria LiPo 803040, 3,7 V, 1000 mAh, protetta | 1 | [Amazon.it - B0G5NM9YJ4](https://www.amazon.it/dp/B0G5NM9YJ4); 8 x 30 x 40 mm, connettore JST 1,25 mm, polarità da verificare |
| SanDisk Extreme microSDXC 128 GB, A2 / U3 / V30 | 1 | [Amazon.it - B07FCMKK5X](https://www.amazon.it/dp/B07FCMKK5X); montata e verificata in lettura/scrittura |

Le inserzioni commerciali possono cambiare: prima di alimentare verificare
sempre serigrafia, tensione, connettore e polarità del componente ricevuto.

## Destinazione d'uso

CaskLogic PesaLink digitalizza la pesa meccanica Laveggio Printomatic per uso
esclusivamente interno e non fiscale. Il peso acquisito ha finalità informative
e operative: non sostituisce uno strumento omologato e non è valido per
transazioni commerciali, adempimenti fiscali o verifiche metrologiche legali.

## Waveshare ESP32-S3-Touch-LCD-2.8

La scheda riunisce ESP32-S3, LCD ST7789 verticale 240x320, touch capacitivo,
slot microSD SD_MMC, audio PCM5101 con amplificatore, IMU QMI8658, RTC PCF85063,
gestione LiPo e misura della batteria su GPIO8. Il firmware riconosce entrambi i
controller touch documentati:

- revisione V2: CST3530;
- revisione V1: CST328.

Waveshare indica che la V1 è stata sostituita dalla V2; verificare la revisione
stampata sulla scheda prima del primo flash. I due profili compilati differiscono
solo nella preferenza iniziale del touch e mantengono un rilevamento di fallback.

La scheda dispone di antenna ceramica integrata e connettore IPEX1. Per usare il
pigtail IPEX-1/U.FL verso SMA e l'antenna esterna da 2,4 GHz occorre spostare la
resistenza di selezione RF. Sul montaggio realizzato la resistenza è stata
spostata a sinistra con saldatore ad aria calda e flussante, lavorando a scheda
disalimentata. Non forzare il connettore IPEX1. La posizione precedente e la
direzione dello spostamento sono illustrate in [cablaggio](wiring.md#antenna-wi-fi-esterna).

## Multiplexer e sensori

Il PCA9546 Adafruit cod. 5663 isola quattro rami I2C, necessario perché tutti gli
AS5600 usano l'indirizzo `0x36`. È compatibile con il TCA9546A usato dal firmware,
risponde normalmente a `0x70` e può essere configurato da `0x70` a `0x77`. Il bus
esterno della nuova scheda usa GPIO11 SDA e GPIO10 SCL a 100 kHz.

Questo modello compatto sostituisce il precedente STEMMA QT cod. 5664. Viene
fornito come PCB assemblato senza cavi o sensori e richiede intestazioni o fili
saldati sui pin `VIN`, `GND`, `SDA`, `SCL`, `RST`, `SD0`-`SD3` e `SC0`-`SC3`.

Ogni AS5600 restituisce 0-4095 su un giro. Per la lettura I2C servono solo VCC,
GND, SDA e SCL; DIR, GPO e OUT rimangono scollegati. Il magnete deve essere
diametralmente magnetizzato, centrato e ruotare sul proprio asse. Distanza e
centraggio vanno validati con i flag MD, ML e MH e con una rotazione completa.

Il cavo Lapp LiYY 4 x 0,14 mm2 è fornito in uno spezzone da 10 m ed è composto da
quattro conduttori non schermati. Va tagliato nelle lunghezze necessarie: la
quantità acquistata non autorizza una singola tratta I2C da 10 m. Mantenere i
rami quanto più corti possibile e verificare sul montaggio reale qualità del
segnale, frequenza, pull-up e affidabilità delle scansioni.

## Alimentazione

La scheda può essere alimentata via USB-C o tramite una LiPo 1S sul connettore
dedicato. Multiplexer e sensori ricevono 3,3 V dalla scheda e condividono la
massa. La precedente scheda esterna bq25185 non è più necessaria nel cablaggio
standard: non collegarla in parallelo al caricatore integrato senza un progetto
elettrico specifico che impedisca ritorni di corrente.

Riferimenti:

- [Waveshare ESP32-S3-Touch-LCD-2.8](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8)
- [PCA9546 Adafruit cod. 5663](https://learn.adafruit.com/adafruit-pca9546-4-channel-i2c-multiplexer/pinouts)
- [Datasheet AS5600](https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf)
