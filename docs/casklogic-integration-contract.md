# Contratto futuro con CaskLogic

Questo documento descrive il lato gestionale da implementare in un intervento
separato. Il repository CaskLogic non e stato modificato.

## Endpoint dispositivo

Proposta:

```text
POST /api/v1/scale-devices/events
Authorization: Bearer <token univoco del dispositivo>
Content-Type: application/json
```

Il trasporto deve usare TLS con un certificato server verificabile dalla CA
installata sul dispositivo. Quando viene configurato mTLS, il reverse proxy o
il backend deve inoltre verificare il certificato client e associarlo allo
stesso `device_id` autorizzato dal token applicativo.

Eventi inviati:

- `scale.snapshot`: misura valida e stabile cambiata;
- `scale.heartbeat`: ultima misura completa durante inattivita;
- `device.power`: perdita o ripristino dell'alimentazione esterna.

Campi di identita e ordinamento obbligatori:

```json
{
  "type": "scale.snapshot",
  "schema_version": 1,
  "event_id": "laveggio-printomatic-01:A1B2C3-18FA93DE:1842",
  "device_id": "laveggio-printomatic-01",
  "boot_id": "A1B2C3-18FA93DE",
  "sequence": 1842,
  "captured_at": "2026-08-29T16:42:18+0200",
  "captured_ms": 983442,
  "digits": [1, 2, 3, 4],
  "multipliers_kg": [10000, 1000, 100, 10],
  "weight_kg": 12340,
  "stable": true,
  "valid": true,
  "sensors": [],
  "signature_alg": "HMAC-SHA256",
  "signature": "<64 caratteri esadecimali>"
}
```

La firma copre, in quest'ordine e separati da `\n`, `event_id`, `captured_at`,
`weight_kg` e le quattro cifre unite da un punto. Esempio canonico:

```text
laveggio-printomatic-01:A1B2C3-18FA93DE:1842
2026-08-29T16:42:18+0200
12340
1.2.3.4
```

Il backend deve verificare la firma prima di usare l'evento e confrontarla in
tempo costante. `event_id` e l'identificativo univoco primario; la terna
`(device_id, boot_id, sequence)` resta il vincolo di deduplicazione strutturale.

## MQTT TLS opzionale

Per lo stato realtime e preferibile MQTT TLS rispetto al polling del
dispositivo. Il firmware usa questi topic, dove `<base>` e configurabile:

```text
<base>/<device_id>/availability   retained: online/offline
<base>/<device_id>/weights        eventi scale.snapshot
<base>/<device_id>/status         retained: scale.heartbeat
<base>/<device_id>/commands       comandi gestionali
<base>/<device_id>/command-acks   esito dei comandi
```

I soli comandi ammessi sono `display.set`, `config.sync` e
`diagnostics.run`, ciascuno con `command_id`. Autenticazione broker, CA e mTLS
devono essere associati al dispositivo. Non esiste una coda persistente sul
gateway: un evento MQTT non consegnato non viene riprovato dopo la
riconnessione; lo storico locale rimane disponibile per audit ed export.

## Configurazione remota

Il dispositivo esegue un `GET` HTTPS all'URL configurato e invia
`Authorization: Bearer`, `X-Device-Id` e `X-Config-Version`. Il backend puo
rispondere `304` oppure `200` con una versione strettamente crescente. Sono
accettati soltanto: finestra di stabilita, display predefinito, heartbeat e
watchdog, retention dello storico e calibrazioni. Rete Wi-Fi, amministratore,
endpoint, token e materiale TLS restano esclusivamente locali. Una risposta
valida viene salvata in NVS; errori o payload non validi lasciano attiva
l'ultima configurazione locale.

## Metriche

`GET /api/metrics` espone metriche Prometheus per uptime, heap, Wi-Fi, ora,
microSD, batteria, temperatura, integrazione, MQTT, peso e salute/errori dei
sensori. Il gestionale o il sistema di monitoraggio deve usare il Bearer token
metriche dedicato e raggiungere il dispositivo soltanto dalla VLAN tecnica.

## Regole backend

- autenticare il token confrontandone un hash, senza memorizzarlo in chiaro;
- verificare HMAC-SHA256 in tempo costante e mantenere segreti distinti per
  dispositivo;
- deduplicare per `(device_id, boot_id, sequence)`;
- rifiutare versioni schema sconosciute e payload non validi;
- mantenere l'ultima misura live separata dalle pesate confermate;
- considerare scaduta la misura dopo un timeout configurabile;
- pubblicare l'aggiornamento al kiosk tramite il canale realtime esistente;
- non creare automaticamente lordo, tara o una pesata definitiva;
- non sovrascrivere un campo mentre l'operatore sta inserendo un valore manuale;
- conservare audit di connessioni, errori e cambi di alimentazione;
- registrare ultimo heartbeat ricevuto, codice di risposta e stato online;
- rispondere rapidamente all'heartbeat prima di avviare elaborazioni non
  necessarie alla conferma, per non causare riavvii impropri del dispositivo;
- demandare al backend l'invio e-mail per gli eventi configurati.

## Risposta

Una risposta `2xx` conferma soltanto ricezione e validazione dell'evento:

```json
{
  "accepted": true,
  "device_id": "laveggio-printomatic-01",
  "boot_id": "A1B2C3-18FA93DE",
  "sequence": 1842,
  "server_time": "2026-08-29T14:42:18Z"
}
```

Il dispositivo mantiene lo storico locale anche quando il backend non e
raggiungibile, ma non usa lo storico come coda di reinvio. Le rotazioni usano
file univoci, senza sovrascrivere gli
archivi precedenti, e l'interfaccia permette un export NDJSON concatenato. La
microSD non e la fonte canonica delle pesate confermate nel gestionale e il
campo locale `delivery` registra l'intenzione di invio, non una ricevuta del
backend.

Il watchdog heartbeat del dispositivo considera raggiungibile il gestionale
soltanto dopo una risposta `2xx`. Dopo la soglia configurata esegue al massimo
un riavvio e resta inibito fino a una nuova risposta valida, evitando reboot
loop durante indisponibilita prolungate del backend.
