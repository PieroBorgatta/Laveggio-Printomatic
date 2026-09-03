# Cablaggio

## Schema logico

```text
USB-C oppure LiPo 3,7 V
          |
          v
Waveshare ESP32-S3-Touch-LCD-2.8
  3V3  ---------------- V+    PCA9546/TCA9546A
  GND  ---------------- GND          |
  GPIO11 (SDA) -------- SDA          +-- CH0 -- AS5600 #0
  GPIO10 (SCL) -------- SCL          +-- CH1 -- AS5600 #1
                                      +-- CH2 -- AS5600 #2
                                      +-- CH3 -- AS5600 #3

Per ciascun canale: V+ -> VCC, GND -> GND, SDA -> SDA, SCL -> SCL.
AS5600: DIR, GPO e OUT non collegati nella lettura I2C standard.
```

## Scheda verso multiplexer

| Waveshare ESP32-S3 | Multiplexer | Nota |
| --- | --- | --- |
| 3V3 | V+ | Non usare 5 V senza verifica del modulo |
| GND | GND | Massa comune obbligatoria |
| GPIO11 | SDA | Bus esterno dedicato dal firmware |
| GPIO10 | SCL | Bus esterno dedicato dal firmware |
| GPIO18 | RST, opzionale | Collegare solo se si vuole il reset pilotato |

Il multiplexer funziona anche con il solo cablaggio a quattro fili se il suo
RST e gia mantenuto alto dalla scheda. GPIO18 viene predisposto alto dal
firmware, ma non agisce se non e collegato fisicamente.

## Attenzione ai colori dei cavetti

Sul prototipo precedente alcuni connettori specchiavano l'ordine dei fili. Il
colore non identifica il segnale. Prima di alimentare:

1. scollegare USB-C e batteria;
2. leggere V+, GND, SDA e SCL sulle serigrafie;
3. verificare ogni filo con il multimetro in continuita;
4. escludere corti tra V+ e GND;
5. collegare un solo AS5600 e verificare `0x70` e `0x36`;
6. aggiungere gli altri rami uno alla volta.

SDA/SCL invertiti impediscono la comunicazione; VCC/GND invertiti possono
danneggiare i componenti. Display, touch, audio, RTC, IMU e microSD sono gia
cablati sulla Waveshare e non devono essere riportati sul bus esterno.
