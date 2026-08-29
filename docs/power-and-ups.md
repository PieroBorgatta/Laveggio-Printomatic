# Alimentazione tampone e rilevazione rete

## Modulo previsto

Il modulo Adafruit bq25185 USB / DC / Solar Charger with 5V Boost puo essere
installato nel contenitore insieme a una LiPo 1S protetta. Il collegamento di
potenza previsto e:

```text
Alimentatore USB-C -> ingresso USB-C Adafruit bq25185
LiPo 3,7 V         -> connettore BATT del modulo
5V OUT modulo      -> VCC_5V Waveshare
GND modulo         -> GND Waveshare
```

Non collegare la LiPo direttamente a `3V3`, `VCC_5V` o USB della Waveshare. Non
usare contemporaneamente due sorgenti 5 V senza isolamento contro il ritorno di
corrente. Per programmare la Waveshare mentre e alimentata dal modulo usare un
cavo USB solo dati oppure scollegare in modo controllato l'uscita del modulo.

## Segnale opzionale di mancanza rete

Il firmware puo leggere GPIO20 per distinguere alimentazione esterna e batteria.
Il pin ESP32 non tollera 5 V: il VBUS di ingresso del modulo deve arrivare a
GPIO20 soltanto tramite un partitore dimensionato e verificato, con massa comune.
Un esempio da validare elettricamente e `100 kOhm` fra VBUS e GPIO20 e `100 kOhm`
fra GPIO20 e GND, che produce circa 2,5 V quando VBUS e 5 V.

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

Con rapporto `2,000`, una batteria a 4,2 V porta circa 2,1 V sul GPIO. Il BAT
non deve essere collegato direttamente al GPIO. Il firmware mostra tensione,
percentuale lineare stimata e capacita nominale configurata; la capacita
predefinita e `1200 mAh` per la batteria AFTERTECH 103040 indicata. La
percentuale non e una misura coulombmetrica e va verificata sotto carico.

Il bq25185 non fornisce al firmware una misura diretta della corrente assorbita
attraverso questo collegamento. Per visualizzare ampere e consumi reali serve
un sensore di corrente dedicato. Pinout di riferimento:
<https://learn.adafruit.com/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board/pinouts>.
