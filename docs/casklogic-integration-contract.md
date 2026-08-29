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

Eventi inviati:

- `scale.snapshot`: misura valida e stabile cambiata;
- `scale.heartbeat`: ultima misura completa durante inattivita;
- `device.power`: perdita o ripristino dell'alimentazione esterna.

Campi di identita e ordinamento obbligatori:

```json
{
  "type": "scale.snapshot",
  "schema_version": 1,
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
  "sensors": []
}
```

## Regole backend

- autenticare il token confrontandone un hash, senza memorizzarlo in chiaro;
- deduplicare per `(device_id, boot_id, sequence)`;
- rifiutare versioni schema sconosciute e payload non validi;
- mantenere l'ultima misura live separata dalle pesate confermate;
- considerare scaduta la misura dopo un timeout configurabile;
- pubblicare l'aggiornamento al kiosk tramite il canale realtime esistente;
- non creare automaticamente lordo, tara o una pesata definitiva;
- non sovrascrivere un campo mentre l'operatore sta inserendo un valore manuale;
- conservare audit di connessioni, errori e cambi di alimentazione;
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

Il dispositivo mantiene comunque lo storico locale anche quando il backend non
e raggiungibile. Le rotazioni usano file univoci, senza sovrascrivere gli
archivi precedenti, e l'interfaccia permette un export NDJSON concatenato. La
microSD non e la fonte canonica delle pesate confermate nel gestionale e il
campo locale `delivery` registra l'intenzione di invio, non una ricevuta del
backend.
