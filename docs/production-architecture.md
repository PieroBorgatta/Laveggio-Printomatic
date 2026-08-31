# Architettura per l'uso operativo

Il gateway ESP32 descritto qui è implementato in
`firmware/production-gateway`. Il backend e il kiosk CaskLogic restano separati
e non sono modificati da questo progetto.

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

## Snapshot implementato

Il contratto è versionato e ogni evento contiene uno snapshot completo:

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

Il dispositivo usa richieste `HTTPS POST` asincrone e puo pubblicare via MQTT
TLS senza interrompere la scansione dei sensori. Il certificato CA deve essere
configurato dall'interfaccia web; il firmware rifiuta endpoint HTTP e TLS privo
di una CA attendibile. Certificato e chiave client opzionali consentono mTLS.
Ogni pesata puo essere firmata HMAC-SHA256. Non e presente una coda persistente
di trasmissione. Il backend deve includere:

- TLS con CA configurata sul dispositivo;
- autenticazione per dispositivo;
- verifica HMAC in tempo costante e deduplicazione per `event_id`;
- heartbeat e timeout di misura scaduta;
- riconnessione con backoff;
- `boot_id` e `sequence` per deduplicazione e riordino;
- snapshot completo a ogni evento, non la sola cifra modificata;
- nessuna persistenza automatica del lordo/tara senza conferma dell'operatore.

Il portale locale resta HTTP ed e destinato a una VLAN tecnica amministrata
dall'access point, con ACL che ne limitino l'accesso. Autenticazione Basic,
rate limit, CSRF e header restrittivi riducono il rischio applicativo ma non
sostituiscono la cifratura del trasporto. Dettagli in
[`security.md`](security.md).

## Affidabilità

- watchdog hardware alimentato soltanto da task realmente sani;
- riconnessione Wi-Fi e backend indipendenti dalla scansione locale;
- configurazione Wi-Fi e backend non inclusa in chiaro nel repository;
- sincronizzazione remota solo di campi operativi in whitelist, con versione
  crescente e conservazione dell'ultima copia locale valida;
- watchdog heartbeat opzionale con un solo riavvio e inibizione persistente
  fino alla successiva risposta valida;
- stato locale evidente: sensori, Wi-Fi, backend e misura valida;
- calibrazione salvata con versione e checksum;
- fallback manuale completo quando sensori o rete non sono disponibili.
