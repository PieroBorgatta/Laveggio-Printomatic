# Firmware 1 — lettura diagnostica e calibrazione

Questa cartella contiene il firmware attualmente disponibile e provato sulla
Waveshare ESP32-C6-LCD-1.47.

## Scopo

- individuare automaticamente il multiplexer agli indirizzi `0x70`–`0x77`;
- selezionare in sequenza i quattro canali;
- rilevare ciascun AS5600 all'indirizzo `0x36`;
- leggere il valore grezzo a 12 bit dai registri `0x0C`–`0x0D`;
- leggere periodicamente `STATUS`, `AGC` e `MAGNITUDE`;
- mostrare i quattro valori sul display senza refresh completo;
- produrre diagnostica JSON sulla seriale a `115200 baud`;
- rilevare una linea I2C bloccata bassa.

Questo sketch serve per montaggio, diagnosi e futura raccolta dei punti di
calibrazione. Non invia dati in rete e non calcola ancora le cifre meccaniche.

## Parametri attuali

| Parametro | Valore |
| --- | --- |
| SDA ESP32 | GPIO 1 |
| SCL ESP32 | GPIO 2 |
| Frequenza I2C | 100 kHz |
| Indirizzo multiplexer osservato | `0x70` |
| Indirizzo AS5600 | `0x36` |
| Intervallo nominale di lettura | 10 ms |
| Diagnostica magnetica | 250 ms |
| Seriale | 115200 baud |

## Interpretazione del display

- `S0`–`S3`: canale del multiplexer.
- `0000`–`4095`: angolo grezzo AS5600.
- `OK`: magnete rilevato con intensità nel campo accettabile.
- `MAG`: magnete assente, troppo debole o troppo forte.
- `----`: nessun AS5600 raggiungibile sul canale.

L'assenza del magnete rende l'angolo non utilizzabile anche se il chip continua
a rispondere via I2C.

## Calibrazione prevista

Per ciascuna manopola si dovranno registrare più campioni stabili per ogni
posizione meccanica:

- manopole `x10`, `x100`, `x1.000`: posizioni da 0 a 9;
- manopola `x10.000`: posizioni effettivamente disponibili sulla pesa;
- campioni presi possibilmente arrivando alla posizione da entrambe le
  direzioni, per misurare il gioco meccanico.

La futura tabella di calibrazione dovrà contenere centro angolare, tolleranza e
isteresi per ogni posizione, gestendo correttamente il passaggio circolare fra
`4095` e `0`.

## Limite attuale

La memorizzazione guidata delle posizioni non è ancora implementata. Per ora i
valori possono essere annotati osservando display e seriale.
