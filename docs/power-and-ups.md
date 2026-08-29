# Alimentazione tampone e rilevazione rete

## Modulo previsto

Il modulo previsto e l'Adafruit bq25185 USB / DC / Solar Charger with 5V Boost
Board, Product ID 6106. Integra il caricatore, il power-path e un TPS61023 con
uscita regolata a 5 V fino a 1 A; non richiede un convertitore separato. Va
installato nel contenitore insieme a una LiPo 1S protetta.

Modulo acquistato: [Amazon.it — ASIN B0DXK6YZX8](https://www.amazon.it/dp/B0DXK6YZX8).

Il collegamento di potenza previsto e:

```text
Alimentatore USB-C -> ingresso USB-C Adafruit bq25185
LiPo 3,7 V         -> connettore BATT del modulo
terminale + 5 V    -> VCC_5V Waveshare
terminale -        -> GND Waveshare
```

Non collegare la LiPo, `BAT` o la versione base del modulo direttamente a `3V3`
o `VCC_5V`. Non usare contemporaneamente due sorgenti 5 V senza isolamento
contro il ritorno di corrente. Per programmare la Waveshare mentre e alimentata
dal modulo usare un cavo USB solo dati oppure scollegare in modo controllato il
terminale 5 V del modulo. Lasciare scollegato il pad `EN`: spegnendo il boost il
microcontrollore non potrebbe riattivarsi autonomamente.

Il TPS61023 puo bloccarsi se deve avviare istantaneamente un carico superiore a
200 mA. Il firmware mantiene display e retroilluminazione spenti per default,
ma il montaggio definitivo richiede comunque una prova di avvio a batteria con
Wi-Fi, microSD e sensori collegati.

## Segnale opzionale di mancanza rete

Il firmware puo leggere GPIO20 per distinguere alimentazione esterna e batteria.
Il pin ESP32 non tollera 5 V: il pad `VU` del modulo deve arrivare a
GPIO20 soltanto tramite un partitore dimensionato e verificato, con massa comune.
Un esempio da validare elettricamente e `100 kOhm` fra `VU` e GPIO20 e `100 kOhm`
fra GPIO20 e GND, che produce circa 2,5 V quando l'ingresso USB e a 5 V.

Prima del montaggio definitivo misurare con il multimetro:

1. tensione massima su GPIO20 inferiore a 3,3 V;
2. livello basso certo quando manca l'ingresso USB;
3. polarita del connettore JST della batteria;
4. assorbimento di picco con Wi-Fi, microSD e display acceso;
5. temperatura del caricatore durante carica e funzionamento simultanei.

Il rilevamento si abilita dalla pagina Sistema. Ogni transizione viene salvata
nel log e inviata all'endpoint notifiche quando configurato.

## Lettura opzionale della batteria

La tensione della LiPo puo essere letta dal pad `BAT` del bq25185 tramite un
partitore verso GPIO0:

```text
BAT bq25185 --- 100 kOhm ---+--- GPIO0 ESP32
                            |
                          100 kOhm
                            |
                           GND
```

Con rapporto `2,000`, una batteria a 4,2 V porta circa 2,1 V sul GPIO. Il pad
`BAT` non deve essere collegato direttamente al GPIO. Il firmware mostra tensione,
percentuale lineare stimata e capacita nominale configurata; la capacita
predefinita e `1200 mAh` per la batteria AFTERTECH 103040 indicata. La
percentuale non e una misura coulombmetrica e va verificata sotto carico.

La versione con boost e configurata di fabbrica per 1 A. Impostare il ponticello
a 500 mA finche la corrente di carica ammessa dalla specifica della batteria
AFTERTECH 103040 non e stata verificata. Verificare sempre anche la polarita del JST-PH:
lo stesso tipo di connettore non garantisce la stessa disposizione dei fili.
Il bq25185 prevede inoltre un timeout di sicurezza di carica non modificabile di
6 ore. La versione con boost non espone il controllo `/CE`, quindi questo timer
non e gestibile dal firmware con il cablaggio previsto.

Il bq25185 non fornisce al firmware una misura diretta della corrente assorbita
attraverso questo collegamento. Per visualizzare ampere e consumi reali serve
un sensore di corrente dedicato.

Riferimenti ufficiali:

- pinout del caricatore Adafruit Product ID 6106:
  <https://learn.adafruit.com/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board/pinouts>;
- schema della Waveshare ESP32-C6-LCD-1.47:
  <https://files.waveshare.com/wiki/ESP32-C6-LCD-1.47/ESP32-C6-LCD-1.47_schemetics.pdf>.
