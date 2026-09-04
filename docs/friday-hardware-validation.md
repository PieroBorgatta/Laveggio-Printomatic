# Collaudo della nuova scheda

Questa checklist separa cio che e gia verificato virtualmente dalle prove che
richiedono la Waveshare fisica.

## Prima di alimentare

1. Leggere sulla scheda se e V2/CST3530 o V1/CST328 e scegliere il relativo
   file factory.
2. Controllare polarita della LiPo MX1.25 e continuita di tutti i cavi I2C.
3. Inserire a dispositivo spento una microSD FAT32 non superiore a 16 GB.
4. Collegare inizialmente solo USB-C, senza batteria, multiplexer o sensori.
5. Se si usa l'antenna esterna, verificare che pigtail e antenna siano SMA
   standard e non RP-SMA; modificare la resistenza RF solo a scheda spenta.

## Prima installazione USB

La vecchia ESP32-C6 e la nuova ESP32-S3 sono dispositivi differenti: usare
`firmware.factory.bin` via USB, non il file OTA. Aprire la seriale a 115200 e
confermare boot senza reset ciclici, modello corretto e partizioni riconosciute.

## Display e touch

- verificare orientamento, colori e assenza di lampeggi a pagina ferma;
- eseguire swipe destra/sinistra su tutte le cinque pagine;
- scorrere verticalmente le pagine con contenuto piu lungo;
- provare i cinque pulsanti touch nel footer;
- spegnere e riaccendere il display dal portale e verificare la persistenza;
- provare BOOT breve e tenere premuto solo in modo controllato per verificare
  l'avviso di ripristino a 10 secondi.

## Audio e pesata

- abilitare lo speaker dal portale;
- simulare una combinazione valida e stabile: deve suonare un solo doppio tono
  quando viene creato il nuovo record;
- mantenere invariato il peso: non deve ripetere il tono;
- cambiare peso stabile: deve suonare di nuovo;
- disabilitare lo speaker lasciando acceso il display: non deve piu suonare;
- riavviare e verificare che le due preferenze restino indipendenti.

## Telemetria e periferiche

- confrontare la tensione batteria del portale con un multimetro a batteria
  carica, intermedia e sotto carico;
- scollegare e ricollegare USB verificando log e continuita operativa;
- confrontare l'ora RTC prima e dopo un riavvio e dopo la sincronizzazione NTP;
- inclinare la scheda e verificare che X/Y/Z della QMI8658 cambino coerentemente;
- creare una pesata, riavviare e verificare il record NDJSON sulla microSD;
- eseguire l'autodiagnosi completa e conservare il pacchetto assistenza.

## Sensori e collaudo finale

Saldare e controllare il PCA9546 Adafruit 5663, collegarlo su GPIO11/GPIO10 e
verificare `0x70`; quindi aggiungere un AS5600 alla volta e verificare `0x36` sul
relativo canale. Provare i singoli spezzoni del cavo LiYY alla lunghezza reale,
senza assumere che l'intera matassa da 10 m sia una tratta I2C utilizzabile. Solo
dopo il test dei quattro rami eseguire i 40 punti di calibrazione, dieci giri
controllati, prove di isteresi e confronto con l'indicazione meccanica. L'esito
e pronto per l'uso operativo interno soltanto se display, audio, alimentazione,
storico e peso reale passano insieme; il sistema resta non fiscale e non
sostituisce una pesa omologata.
