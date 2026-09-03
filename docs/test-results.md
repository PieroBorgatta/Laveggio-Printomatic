# Risultati delle prove

## Porting Waveshare ESP32-S3-Touch-LCD-2.8 - 1 settembre 2026

Verifica eseguita senza la nuova scheda fisica:

- `24/24` test Node.js del simulatore superati;
- build PlatformIO V2/CST3530 completata;
- build PlatformIO V1/CST328 completata;
- RAM statica `54.436 / 327.680 byte` (`16,6%`);
- flash applicazione `1.657.078 / 6.291.456 byte` (`26,3%`);
- immagini factory e OTA ECDSA-P256 firmate per entrambi i profili;
- dashboard verificata in Chromium con gli stessi asset incorporati;
- interruttori display e speaker indipendenti e persistenti nel simulatore;
- telemetria simulata di touch, speaker, batteria, QMI8658 e PCF85063 visibile;
- nessun errore JavaScript di esecuzione osservato nel flusso controllato.

La compilazione verifica API e compatibilita software, non il comportamento
elettrico. ST7789, touch, audio PCM5101, GPIO batteria, RTC, IMU, microSD SD_MMC,
power hold e I2C esterno devono passare la checklist
[`friday-hardware-validation.md`](friday-hardware-validation.md) sulla scheda.

## Prove storiche sul precedente prototipo ESP32-C6

## Ambiente osservato

- Waveshare ESP32-C6-LCD-1.47 collegata via USB-C;
- multiplexer rilevato a `0x70`;
- un AS5600 collegato al canale 0 durante l'ultima prova;
- SDA GPIO 1, SCL GPIO 2;
- I2C a 100 kHz;
- seriale a 115200 baud.

## Risultati confermati

- compilazione completata con core Arduino ESP32;
- caricamento su ESP32-C6 completato e verificato dall'utility di flash;
- display inizializzato e aggiornamento limitato alle righe modificate;
- multiplexer e AS5600 raggiungibili;
- scansione aggregata osservata circa 80–101 volte al secondo;
- durata di una scansione completa osservata circa 4,2–5,9 ms;
- magnete rimosso: `raw=0`, `MD=false`, `ML=true`, magnitudine circa 0–1;
- magnete vicino: `raw` circa 3215–3221, `MD=true`, `ML=true`, magnitudine
  circa 1080–1100.

## Cosa non è ancora validato

Non è ancora stata registrata una rotazione controllata che percorra l'intero
intervallo 0–4095. Il display è aggiornato quando il valore letto cambia, ma
l'ultima configurazione magnetica ha prodotto variazioni minime e un flag di
campo debole.

Prima di usare il sistema sulla pesa occorre:

1. confermare che il magnete sia diametralmente magnetizzato;
2. definire centraggio e distanza meccanicamente ripetibili;
3. registrare min/max e traiettoria durante almeno dieci giri completi;
4. verificare separatamente tutti e quattro i sensori;
5. montare i sensori e registrare ogni scatto meccanico;
6. misurare gioco, isteresi, vibrazioni e deriva termica;
7. verificare che la lettura digitale coincida sempre con l'indicazione
   meccanica prima dell'integrazione gestionale.

## Nota metodologica

La frequenza del ciclo non dimostra da sola che l'esperienza sul display sia
corretta. Le prossime prove devono acquisire contemporaneamente valore grezzo,
minimo, massimo, numero di variazioni e video/riscontro visivo del magnete.

## Verifica virtuale del gateway operativo

In assenza del dispositivo, il firmware in `firmware/production-gateway` è
stato verificato con strumenti host:

- compilazione PlatformIO per `esp32-c6-devkitc-1` con Arduino ESP32;
- RAM statica `50.708 / 327.680 byte` (`15,5%`);
- flash applicazione `1.686.942 / 2.031.616 byte` (`83,0%`);
- immagine OTA firmata `1.744.992 byte`, con `286.624 byte` (`14,1%`) liberi
  nello slot;
- generazione automatica e inclusione degli asset web nel binario;
- test Node.js del simulatore REST e delle regole di configurazione;
- 20 test Node.js superati su API, segreti, calibrazione, storico, filtri,
  export, TLS, CSRF, header di sicurezza, autodiagnosi, ZIP anonimizzato,
  metriche, configurazione remota e OTA firmato;
- firma ECDSA-P256 dell'immagine OTA verificata indipendentemente con OpenSSL;
- controllo sintattico del JavaScript eseguito nel browser;
- verifica browser dell'interfaccia desktop e mobile tramite Playwright;
- viewport verificati: `1440x1000` e `390x844`, tema chiaro e scuro;
- controllo dei flussi display, calibrazione, storico, impostazioni e
  autodiagnosi, inclusi grafici canvas non vuoti e tabelle mobili scorrevoli.

Queste prove verificano compilabilità, logica indipendente dall'hardware,
contratto HTTP e comportamento dell'interfaccia. Non verificano fisicamente:

- presenza e scrittura della microSD;
- letture I2C dei quattro AS5600 attraverso il TCA9546A;
- inizializzazione e resa del display ST7789;
- stabilità della rete Wi-Fi sull'impianto;
- aggiornamento OTA e ripristino dopo un'interruzione reale;
- autonomia o commutazione dell'eventuale alimentazione di backup;
- consegna degli eventi a un backend CaskLogic, che non è stato modificato.

Le misure finali di RAM e flash della build sono riportate nel README del
gateway operativo e devono essere ricontrollate a ogni rilascio.
