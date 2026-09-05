# Affidabilità e chiusura sperimentale della bascula — 2.1.0

## Priorità operative

L'acquisizione dei quattro AS5600 usa un task dedicato sul core 1, priorità 5,
con periodo nominale di 20 ms. Il task possiede il bus I2C esterno; portale,
display, microSD, NVS, DNS e sincronizzazione remota non eseguono operazioni
all'interno del percorso di acquisizione.

Gli snapshot stabili entrano in una coda RAM senza attese, prima della
telemetria opzionale della scheda. Un dispatcher sul core 0, priorità 4,
prepara i messaggi e alimenta due trasporti indipendenti, HTTPS e MQTT,
priorità 3. Heartbeat e servizi accessori non condividono la coda delle pesate.
Lo storico viene elaborato separatamente dal ciclo del portale.

Le code sono limitate: 32 snapshot in acquisizione, 32 per trasporto e 64
record di archivio. Le code grandi usano PSRAM. Eventuali saturazioni sono
contate ed esposte, senza bloccare il campionamento. Non sono una coda
persistente, non sopravvivono al riavvio e non reinviano automaticamente lo
storico. Un guasto di rete o del destinatario può ancora impedire la consegna.

L'IMU viene interrogata dopo le letture, solo nel tempo residuo disponibile.
RTC e batteria vengono aggiornati circa ogni secondo. Non si presume che un
microcontrollore general purpose garantisca assenza assoluta di jitter:
scritture flash/NVS, OTA, guasti elettrici e timeout I2C richiedono misure sul
dispositivo. Intervallo massimo, ritardi, età del campione e saturazioni sono
visibili in Sistema e in `GET /api/status`, sezione `reliability`.

Una pausa superiore a 100 ms azzera la stabilizzazione: due letture uguali
separate da un intervallo non osservato non confermano un peso. Stato del
magnete e angolo vengono letti insieme a ogni scansione.

## Ora senza rete

Il PCF85063 conserva UTC. All'avvio si controllano BCD, calendario, anno,
indicatore di arresto dell'oscillatore e bit STOP, prima di impostare l'ora di
sistema. Il fuso POSIX riguarda solo la presentazione e le cartelle locali.

Per migrare dai firmware che scrivevano l'ora locale serve una prima
sincronizzazione NTP riuscita: il firmware marca il formato UTC soltanto dopo
la scrittura e lettura valide del RTC. Un RTC vecchio di formato ignoto non
viene interpretato arbitrariamente come UTC. È necessaria alimentazione di
backup del RTC per conservarlo dopo lo spegnimento completo.

`time_source` distingue `ntp`, `rtc` e `unavailable`; `time_synchronized`
indica una sincronizzazione NTP in questo avvio, mentre `time_valid` indica
che lo snapshot possiede un timestamp. Gli eventi conservano l'istante di
acquisizione anche se archivio o invio avvengono successivamente.

## Stabilità, archivio e consegna

- **Stabile**: le cifre valide sono rimaste invariate per la finestra impostata.
- **Salvata su microSD**: apertura e scrittura della riga sono riuscite; il file
  viene scaricato e chiuso. Un errore non produce il tono di successo.
- **Ricevuta dal gestionale**: risposta HTTPS 2xx allo specifico evento.
- **Pubblicata MQTT**: scrittura sul trasporto MQTT QoS 0 riuscita; non è una
  conferma applicativa del gestionale.

Il display e il portale mostrano lo stato dell'ultimo snapshot. Gli esiti di
trasporto sono anche righe `delivery.result` nei log, correlate per `event_id`.
Lo storico originale mantiene `delivery=requested` quando l'invio è richiesto:
quel valore, da solo, non è una ricevuta. La conferma operatore della pesata
nel gestionale rimane un'azione distinta.

## Calibrazione e ordine sensori

In Calibrazione, **Ordine delle cifre** associa le quattro posizioni logiche ai
canali fisici 0–3 del multiplexer. Ogni canale deve comparire esattamente una
volta. I 10 punti, il rumore e i riferimenti magnetici restano associati al
sensore fisico; i moltiplicatori appartengono alle posizioni logiche.

Esempio: con ordine `[3,1,2,0]`, la prima cifra proviene dal canale fisico 3 e
usa il primo moltiplicatore. Non occorre spostare cavi o ricatturare i punti.
Dopo ogni cambio ordine verificare il peso ricostruito sulla meccanica reale.

Una cattura richiede 25 campioni sani, un campione recente e dispersione non
superiore a 12 unità raw. Punti con distanza circolare minore o uguale a
`2 × tolleranza + isteresi` sono rifiutati perché le regioni si sovrappongono.
La pagina mostra rumore, margine rispetto alla tolleranza e scostamento della
magnitudine rispetto al punto memorizzato. Questi dati aiutano a riconoscere
movimenti del supporto o del magnete; non stimano autonomamente un errore in kg.

Calibrazioni e ordine vengono salvati insieme in un blob NVS con schema,
revisione e CRC32, conservando la lettura del formato precedente per la
migrazione. Un blob danneggiato disabilita i punti: nessuna misura valida viene
ricostruita usando una calibrazione non verificata. Gli snapshot riportano
`calibration_revision` e `sensor_order`.

## Chiusura della bascula: sperimentale

La funzione è disattivata per impostazione iniziale. Si configura in
**Sistema → Chiusura della bascula**.

| Parametro | Valore iniziale | Significato |
| --- | ---: | --- |
| Abilita rilevazione | No | Attiva acquisizione IMU più frequente e rilevatore |
| Segnala peso completato | No | Se disattivato, sola osservazione locale |
| Soglia colpo | 0,35 g | Accelerazione rispetto alla componente lenta/gravitazionale |
| Soglia quiete | 0,08 g | Livello massimo dopo il colpo |
| Quiete minima | 400 ms | Durata continuativa richiesta sotto la soglia |
| Tempo massimo | 3000 ms | Scadenza del candidato dopo il colpo |
| Pausa fra rilevazioni | 2000 ms | Evita conteggi ripetuti della stessa chiusura |

Il rilevatore usa il modulo tridimensionale dell'accelerazione dopo un filtro
passa-alto: impulso, quiete, lettura valida e stabile. Un intervallo IMU non
osservato superiore a 100 ms annulla il candidato. Il campionamento nominale
è 50 Hz: impulsi molto brevi possono sfuggire. Vibrazione corrente, picco,
candidati e numero di rilevazioni sono visibili dal portale.

In **sola osservazione** viene creato un evento locale `scale.closure_candidate`
nei log, senza inviare colpi al destinatario delle pesate. Con **Segnala peso
completato** viene generato uno `scale.snapshot` aggiuntivo con
`closure_detected=true`, `weight_completed=true` e
`completion_experimental=true`. Il flusso ordinario degli snapshot stabili
continua sempre. Un peso uguale al precedente può produrre un nuovo evento
di completamento dopo una nuova chiusura riconosciuta.

### Procedura di prova

1. Fissare la scheda al supporto definitivo: viene misurato il movimento della
   scheda, non quello di un componente remoto.
2. Abilitare solo la rilevazione; lasciare disattivato il completamento.
3. Registrare chiusure vere, movimenti delle manopole, urti sul banco e rumore
   normale; confrontare picchi, conteggi e mancate rilevazioni.
4. Regolare soglia del colpo, quiete, durata e pausa. Ripetere con chiusure
   leggere e forti; controllare falsi positivi e negativi.
5. Solo dopo il confronto meccanico attivare il completamento sperimentale.
   Il backend dovrà trattarlo come suggerimento, mai come conferma operatore.

## Display, audio e batteria

Luminosità 5–100%, attenuazione dopo inattività (0 disabilita) e ripristino al
touch. Gli interruttori display e speaker restano indipendenti. La percentuale
batteria usa una curva approssimata LiPo: è ancora una stima da tensione, non
un misuratore di corrente o autonomia. L'avviso di batteria bassa è regolabile
e usa isteresi per evitare ripetizioni.

Il tasto batteria, se abilitato, richiede prima un rilascio e poi una pressione
di 2 secondi. Il firmware chiude la microSD, spegne il display e rilascia
POWER_HOLD. Con USB collegata può restare alimentato; letture e invio
continuano, mentre la SD resta chiusa fino al riavvio.

## API e manutenzione

- `GET/POST /api/settings/reliability`: impostazioni display, batteria e bascula.
- `POST /api/calibration/order`: `slot_0`…`slot_3`, permutazione dei canali 0–3.
- `GET /api/calibration`: ordine, revisione e punti fisici.
- `GET /api/status`: stato operativo, telemetria e ricevuta dell'ultimo evento.
- Comando seriale `status` seguito da invio, a 115200 baud: stato in sola lettura.
- Comando seriale `diagnostics`: autodiagnosi, inclusa prova di scrittura SD.
  Eseguirlo fuori dalla pesatura operativa per un collaudo controllato.

Le nuove impostazioni di affidabilità sono persistite in un unico record NVS;
un errore di salvataggio restituisce errore e conserva la configurazione attiva.

Le scritture HTTP mantengono autenticazione e protezione CSRF. La conferma
sperimentale ha una firma aggiuntiva `completion_signature`, oltre alla firma
storica del peso. Il formato di firma e la gestione degli eventi sono descritti
nel contratto d'integrazione. Le prove firmware non sostituiscono il confronto
fisico con la bascula e il destinatario gestionale effettivo.
