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

- autenticazione HTTP Digest anche durante il primo provisioning;
- blocco per 60 secondi dopo cinque credenziali errate;
- token CSRF casuale per ogni avvio su tutte le richieste che modificano dati;
- Content Security Policy, divieto di framing, `nosniff`, referrer e permessi
  browser restrittivi;
- nessun CORS e nessuna stampa seriale della password amministrativa;
- asset statici, API, log, export e OTA protetti dalla stessa autenticazione.

Le credenziali iniziali richieste sono `info@casklogic.com` e
`Presario41740+`. Sono credenziali di bootstrap note nel firmware: devono
essere sostituite dalla pagina Sistema prima dell'uso operativo.

Digest evita di inviare la password in chiaro, ma non cifra il contenuto della
sessione. Un portale HTTPS locale richiede la sostituzione del `WebServer`
Arduino con `esp_https_server`, oltre alla gestione sicura del certificato e
della chiave del dispositivo. Questa migrazione non e simulata dalla versione
1.1.0. Riferimento ufficiale:
<https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_https_server.html>.

## Collegamento a CaskLogic

Gli endpoint eventi e notifiche accettano esclusivamente URL `https://`. Il
firmware richiede una CA configurata, verifica il certificato del server e non
usa modalita TLS insicura. Sono supportati anche certificato e chiave client
per mTLS; la chiave privata non viene mai restituita dall'API delle
impostazioni. Il token Bearer resta disponibile come identita applicativa del
dispositivo.

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

Token, password Wi-Fi, password amministrativa e chiave privata mTLS sono
conservati nella NVS della scheda. La NVS non e cifrata in questa build: per
protezione contro accesso fisico e lettura flash servono Secure Boot, Flash
Encryption e provisioning delle chiavi in produzione. Prima dell'esercizio
vanno inoltre verificati OTA firmato, procedure di recupero e rotazione dei
certificati.
