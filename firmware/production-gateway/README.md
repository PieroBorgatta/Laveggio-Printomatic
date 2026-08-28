# Firmware 2 — gateway operativo verso il gestionale

**Stato: non ancora implementato.**

Questa area sarà dedicata al firmware definitivo. Rimane separata dal lettore
diagnostico affinché gli strumenti di calibrazione non vengano confusi con il
software destinato all'uso quotidiano.

## Funzioni previste

- caricamento della calibrazione persistente dei quattro sensori;
- scansione locale continua dei quattro canali;
- conversione angolo → posizione meccanica con tolleranza e isteresi;
- conferma di stabilità prima di pubblicare una nuova cifra;
- calcolo del peso come snapshot completo delle quattro posizioni;
- connessione Wi-Fi con indirizzo riservato/statico;
- connessione persistente e riconnessione automatica al backend;
- invio immediato dello snapshot quando cambia una cifra valida;
- heartbeat periodico e contatore sequenziale per rilevare disconnessioni o
  messaggi duplicati;
- watchdog hardware e diagnostica locale;
- aggiornamento firmware controllato e configurazione protetta;
- eventuale riavvio notturno configurabile, utilizzato come protezione
  secondaria e non come sostituto del watchdog.

## Fuori dallo scope del firmware

È il backend/kiosk, non l'ESP32, a decidere se un peso live deve alimentare il
campo lordo o tara. L'ESP32 deve pubblicare soltanto una misura completa,
identificabile e accompagnata dal proprio stato di validità.

Il contratto previsto è descritto in
[`../../docs/production-architecture.md`](../../docs/production-architecture.md).
