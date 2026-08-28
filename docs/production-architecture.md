# Architettura prevista per l'uso operativo

Questo documento descrive la direzione progettuale. Le funzioni di rete e
integrazione non sono ancora presenti nel firmware.

## Responsabilità

```text
AS5600 x4
   │ angoli grezzi
   ▼
ESP32 ── calibrazione, isteresi, stabilità ── snapshot peso valido
   │
   │ connessione persistente autenticata
   ▼
Backend ── stato della sessione di pesatura ── Kiosk
                                                │
                                                ├─ Lordo
                                                ├─ Tara
                                                └─ Inserimento manuale
```

### ESP32

L'ESP32 legge continuamente i quattro sensori anche quando non deve trasmettere.
Pubblica un evento soltanto quando cambia lo snapshot stabile o quando scade un
heartbeat. Non decide lordo/tara e non conferma una pesata.

### Backend

Il backend mantiene l'ultima misura live, verifica autenticità, sequenza,
validità e freschezza. Deve separare la semplice anteprima live dalla conferma
persistente della pesata.

### Kiosk

Il kiosk sceglie la destinazione in base al flusso operativo:

- nuova pesata o nuovo fornitore: anteprima nel `Lordo`;
- riapertura di una bozza che possiede già il lordo: anteprima nella `Tara`;
- dalla selezione utenti, l'avvio di una pesata automatica può aprire
  l'operatore configurato e predisporre il lordo;
- il richiamo di una bozza non deve mai sovrascrivere il lordo esistente.

Il peso diventa definitivo soltanto con una conferma esplicita dell'operatore.

## Inserimento manuale

La modalità manuale deve essere sempre disponibile. Quando l'operatore modifica
un campo manualmente, il flusso automatico non deve sovrascriverlo. Una possibile
macchina a stati è:

```text
AUTO_IN_ATTESA
  ├─ movimento manopola ──> AUTO_LIVE
  └─ focus/tastierino ─────> MANUALE

AUTO_LIVE
  ├─ conferma pesata ──────> AUTO_IN_ATTESA (pesata successiva)
  └─ pulsante manuale ─────> MANUALE

MANUALE
  ├─ input operatore ──────> resta MANUALE
  └─ movimento manopola con consenso ──> AUTO_LIVE
```

Dopo la conferma della pesata successiva, l'interfaccia torna predisposta per
l'inserimento manuale finché una manopola non viene realmente movimentata.

## Snapshot proposto

Il contratto definitivo dovrà essere versionato. Un possibile evento è:

```json
{
  "type": "scale.snapshot",
  "schema_version": 1,
  "device_id": "laveggio-printomatic-01",
  "boot_id": "uuid-generato-al-riavvio",
  "sequence": 1842,
  "captured_ms": 983442,
  "digits": [1, 2, 3, 4],
  "multipliers_kg": [10000, 1000, 100, 10],
  "fixed_units_kg": 0,
  "weight_kg": 12340,
  "stable": true,
  "valid": true,
  "sensor_flags": ["OK", "OK", "OK", "OK"]
}
```

I moltiplicatori e l'ordine delle manopole vanno confermati durante la
calibrazione reale.

## Trasporto

Una connessione WebSocket persistente è adatta agli aggiornamenti immediati e
bidirezionali. Anche HTTPS POST su rete locale può essere sufficientemente
rapido, ma richiede keep-alive, gestione degli errori e un canale separato per
lo stato. La scelta definitiva dovrà includere:

- TLS o rete locale protetta;
- autenticazione per dispositivo;
- heartbeat e timeout di misura scaduta;
- riconnessione con backoff;
- `boot_id` e `sequence` per deduplicazione e riordino;
- snapshot completo a ogni evento, non la sola cifra modificata;
- nessuna persistenza automatica del lordo/tara senza conferma dell'operatore.

## Affidabilità

- watchdog hardware alimentato soltanto da task realmente sani;
- riconnessione Wi-Fi e backend indipendenti dalla scansione locale;
- configurazione Wi-Fi e backend non inclusa in chiaro nel repository;
- eventuale riavvio notturno configurabile fuori dalle ore operative;
- stato locale evidente: sensori, Wi-Fi, backend e misura valida;
- calibrazione salvata con versione e checksum;
- fallback manuale completo quando sensori o rete non sono disponibili.
