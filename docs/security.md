# Sicurezza del gateway

## Perimetro di rete

Il portale amministrativo ascolta in HTTP sulla sola rete locale. Non deve
essere pubblicato su Internet, esposto tramite port forwarding o inserito in
una rete ospiti non controllata. La configurazione raccomandata e un SSID
dedicato associato dall'access point a una VLAN tecnica, con regole firewall
che consentano:

- accesso al portale soltanto dalle postazioni amministrative;
- DNS, NTP e HTTPS in uscita dal dispositivo;
- nessuna connessione iniziata dalle altre VLAN verso l'ESP32, salvo quelle
  amministrative esplicitamente autorizzate.

La VLAN non e un parametro che una stazione Wi-Fi ESP32 possa imporre: il tag
802.1Q e l'associazione SSID/VLAN appartengono all'access point e allo switch.

## Protezioni del portale

- autenticazione HTTP Basic anche durante il primo provisioning;
- blocco per 60 secondi dopo cinque credenziali errate;
- token CSRF casuale per ogni avvio su tutte le richieste che modificano dati;
- Content Security Policy, divieto di framing, `nosniff`, referrer e permessi
  browser restrittivi;
- nessun CORS e nessuna stampa seriale della password amministrativa;
- asset statici, API, log, export e OTA protetti dalla stessa autenticazione.

Le credenziali iniziali richieste sono `admin` e `casklogic`. Sono credenziali
di bootstrap note nel firmware: devono
essere sostituite dalla pagina Sistema prima dell'uso operativo.

Basic invia le credenziali codificate Base64 ma non cifrate: il portale deve
restare su AP/VLAN tecnica e non essere esposto su reti non fidate. Un portale HTTPS locale richiede la sostituzione del `WebServer`
Arduino con `esp_https_server`, oltre alla gestione sicura del certificato e
della chiave del dispositivo. Questa migrazione non e implementata nella
versione 1.2.1. Riferimento ufficiale:
<https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_https_server.html>.

## Collegamento a CaskLogic

Gli endpoint eventi e notifiche accettano esclusivamente URL `https://`. Il
firmware richiede una CA configurata, verifica il certificato del server e non
usa modalita TLS insicura. Sono supportati anche certificato e chiave client
per mTLS; la chiave privata non viene mai restituita dall'API delle
impostazioni. Il token Bearer resta disponibile come identita applicativa del
dispositivo.

Le pesate `scale.snapshot` possono inoltre essere firmate HMAC-SHA256. Il
gestionale deve confrontare la firma in tempo costante e deduplicare
`event_id`; HMAC autentica il payload ma non sostituisce TLS.

MQTT e opzionale e usa lo stesso archivio CA e, se configurato, lo stesso
certificato client mTLS. I soli comandi ammessi sono `display.set`,
`config.sync` e `diagnostics.run`; ogni altro comando viene rifiutato. Il
firmware non mantiene una coda MQTT persistente. La configurazione remota e
accettata solo via HTTPS verificato, solo con una versione crescente e solo
per i campi operativi in whitelist. Wi-Fi, credenziali amministrative, token,
certificati e chiavi non sono modificabili dal gestionale.

`GET /api/metrics` richiede un Bearer token dedicato quando configurato,
altrimenti usa l'autenticazione del portale. Il pacchetto assistenza non include
SSID, indirizzi IP, token, password, certificati o chiavi private.

## Watchdog heartbeat

Il watchdog e disattivato per impostazione predefinita. Quando viene abilitato,
conta soltanto le mancate risposte all'heartbeat HTTPS. Al raggiungimento della
soglia registra l'evento e pianifica un riavvio. Prima del riavvio salva in NVS
un'inibizione persistente: il dispositivo non entra quindi in un ciclo di
riavvii. Un heartbeat successivo con risposta `2xx` azzera errori e inibizione.

Il watchdog non sostituisce il monitoraggio server-side e non dimostra che il
gestionale abbia pubblicato il peso al kiosk; dimostra soltanto che l'endpoint
ha ricevuto e accettato la richiesta.

## Segreti e aggiornamenti

Token, segreto HMAC, password Wi-Fi, password amministrativa e chiave privata
mTLS sono
conservati nella NVS della scheda. La NVS non e cifrata in questa build: per
protezione contro accesso fisico e lettura flash servono Secure Boot, Flash
Encryption e provisioning delle chiavi in produzione. L'OTA applicativo
richiede una firma ECDSA-P256 ed esegue la validazione del nuovo avvio con
rollback della partizione, ma questo non equivale al Secure Boot della ROM.
Secure Boot e Flash Encryption richiedono un provisioning fisico irreversibile
da pianificare sulla scheda reale. Vedere [`firmware-signing.md`](firmware-signing.md).
