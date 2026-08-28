# Cablaggio

## Schema logico

```text
USB-C
  │
  ▼
ESP32-C6-LCD-1.47
  3V3  ─────────────── V+    PCA9546/TCA9546A
  GND  ─────────────── GND          │
  GPIO1 (SDA) ──────── SDA          ├── CH0 ── AS5600 #0
  GPIO2 (SCL) ──────── SCL          ├── CH1 ── AS5600 #1
                                    ├── CH2 ── AS5600 #2
                                    └── CH3 ── AS5600 #3

Per ciascun canale:
  V+  ───────────────────────────── VCC
  GND ───────────────────────────── GND
  SDA ───────────────────────────── SDA
  SCL ───────────────────────────── SCL

AS5600: DIR, GPO e OUT non collegati per la lettura I2C standard.
```

## Collegamento ESP32 → multiplexer osservato

| ESP32-C6 | Multiplexer | Colore osservato sul prototipo |
| --- | --- | --- |
| 3V3 | V+ | Rosso |
| GND | GND | Nero |
| GPIO 1 | SDA | Bianco |
| GPIO 2 | SCL | Giallo |

Il pad `RST` del multiplexer non era necessario nel cablaggio a quattro fili
usato durante le prove. Lo sketch mantiene inoltre GPIO 3 alto come predisposizione,
ma senza un collegamento fisico questo pin non agisce sul multiplexer.

## Multiplexer → AS5600: attenzione ai colori

Nel cablaggio realmente osservato i connettori potevano specchiare l'ordine dei
conduttori. Sul ramo che ha risposto correttamente è stata osservata questa
corrispondenza elettrica:

| Multiplexer | AS5600 | Colore osservato sul ramo sensore |
| --- | --- | --- |
| V+ | VCC | Rosso |
| GND | GND | Nero |
| SDA | SDA | Giallo |
| SCL | SCL | Bianco |

Questa tabella **non è una regola universale sui colori**. Prima di alimentare:

1. scollegare USB-C;
2. identificare la posizione `V+`, `GND`, `SDA`, `SCL` sulle serigrafie;
3. verificare con un multimetro in continuità quale filo arriva a ogni pin;
4. controllare che `V+` e `GND` non siano in corto;
5. collegare un solo sensore e verificare `0x70` e `0x36`;
6. aggiungere gli altri sensori uno alla volta.

Collegare SDA e SCL invertiti non consente la comunicazione. Invertire VCC e
GND può invece danneggiare i componenti.
