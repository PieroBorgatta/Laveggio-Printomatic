# Batteria e alimentazione

## Collegamento previsto

La Waveshare ESP32-S3-Touch-LCD-2.8 integra circuito di carica e gestione della
batteria. Il collegamento standard e:

```text
Alimentatore USB-C ------> USB-C Waveshare
LiPo 1S protetta 3,7 V --> connettore BAT MX1.25 2 pin Waveshare
3V3 + GND Waveshare ----> multiplexer e quattro AS5600
```

Verificare la polarita del connettore della batteria con serigrafia e
multimetro: lo stesso formato meccanico non garantisce la stessa disposizione
dei fili. Non collegare una LiPo a 3V3 o 5V e non mettere in parallelo il vecchio
caricatore/boost bq25185 con quello integrato senza isolamento progettato.

## Monitoraggio implementato

Il firmware campiona GPIO8 piu volte, usa la mediana per ridurre il rumore e
applica il rapporto del partitore previsto dalla scheda. Espone nel display,
nel portale, nello stato JSON e nei log:

- tensione della batteria;
- percentuale stimata;
- stato alimentazione esterna, se il rilevamento opzionale e abilitato;
- temperatura e accelerazione QMI8658;
- stato e data/ora RTC PCF85063;
- salute microSD, heap e Wi-Fi.

La percentuale e una stima basata sulla tensione, non una misura coulombmetrica;
varia con carico, temperatura e chimica della cella. La scheda non misura la
corrente in ampere, quindi autonomia e consumi reali richiedono uno strumento
esterno o un sensore dedicato.

## Rilevazione opzionale della rete

GPIO15 e riservato come ingresso digitale opzionale per distinguere USB/rete da
batteria. Non collegare mai direttamente 5 V al GPIO. Abilitare la funzione nel
portale solo dopo avere aggiunto un segnale protetto a 3,3 V e verificato con il
multimetro livelli e polarita. Senza questo collegamento il portale indica la
funzione come non configurata; la lettura della batteria su GPIO8 resta attiva.

## Prove obbligatorie

Prima dell'uso continuo verificare avvio a sola batteria, carica con sistema
acceso, temperatura, picchi Wi-Fi/microSD/speaker, spegnimento del display,
riaccensione, perdita e ritorno USB. La checklist completa e in
[`friday-hardware-validation.md`](friday-hardware-validation.md).

Riferimento: [documentazione ufficiale Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8).

## Firmware 2.1

Percentuale da curva LiPo approssimata, avviso basso livello con isteresi, luminosità e attenuazione configurabili. Il tasto batteria può chiudere la microSD e rilasciare POWER_HOLD dopo due secondi. Con USB presente può non spegnere fisicamente la scheda. Provare la sequenza sul dispositivo; resta una stima da tensione, senza misuratore di corrente.
