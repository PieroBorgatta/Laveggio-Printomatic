# Firma e rilascio firmware

Il portale OTA, dalla versione 1.2.0, accetta soltanto immagini
`firmware.signed.bin` firmate ECDSA-P256 con SHA-256. La chiave pubblica è
incorporata in `include/OtaPublicKey.h`; la chiave privata non deve mai entrare
nel repository, nel dispositivo o nel pacchetto assistenza.

## Chiave di firma

Il build script cerca per impostazione predefinita:

```text
%USERPROFILE%\.casklogic\pesalink-signing\private_key.pem
~/.casklogic/pesalink-signing/private_key.pem
```

La posizione può essere sostituita con `PESALINK_SIGNING_KEY`. Per continuità
con le installazioni precedenti, il processo accetta anche il percorso storico
`.casklogic\laveggio-signing\private_key.pem` e la variabile
`LAVEGGIO_SIGNING_KEY`. La chiave va
conservata in un archivio cifrato, con backup offline e accesso limitato agli
operatori autorizzati. La perdita della chiave impedisce nuovi aggiornamenti
OTA ai dispositivi che incorporano la relativa chiave pubblica; una sua
compromissione richiede una procedura fisica controllata di rotazione.

La chiave di produzione corrente è stata ruotata il 6 settembre 2026. Impronta
SHA-256 della chiave pubblica in formato SubjectPublicKeyInfo DER:

```text
9e92fbe706ddb18feddb24c1fddf581cced623ce5340a6c053ff1dc96aaf41e1
```

Su macOS o Linux una nuova coppia può essere creata così. Questi comandi non
devono essere ripetuti se la chiave esiste già:

```sh
install -d -m 700 ~/.casklogic/pesalink-signing
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 \
  -out ~/.casklogic/pesalink-signing/private_key.pem
openssl pkey -in ~/.casklogic/pesalink-signing/private_key.pem -pubout \
  -out ~/.casklogic/pesalink-signing/public_key.pem
chmod 600 ~/.casklogic/pesalink-signing/private_key.pem
```

Una rotazione richiede poi di rigenerare `OtaPublicKey.h` e installare via USB
il nuovo firmware su ogni dispositivo: il vecchio firmware non può accettare
un OTA firmato dalla nuova chiave.

Per rigenerare l'header pubblico da un PEM pubblico:

```powershell
py tools/public_key_to_header.py `
  C:\percorso\public_key.pem `
  include/OtaPublicKey.h
```

Su macOS o Linux:

```sh
python3 tools/public_key_to_header.py \
  ~/.casklogic/pesalink-signing/public_key.pem include/OtaPublicKey.h
```

## Build e verifica

`platformio.ini` esegue automaticamente `tools/sign_firmware.py` dopo la
compilazione. Il file caricato dal portale deve essere quello firmato:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:\pio'
py -m platformio run
```

Lo script rifiuta chiavi private con permessi troppo aperti e verifica che la
loro impronta corrisponda alla chiave pubblica incorporata, evitando di creare
un artefatto che i dispositivi rifiuterebbero.

La firma DER è aggiunta al binario in un blocco finale da 512 byte, come
richiesto da `Update.installSignature()`. Il firmware verifica firma e
dimensione prima di selezionare la nuova partizione. Al primo avvio esegue i
controlli minimi e marca l'immagine valida; in caso contrario richiede il
rollback. Ogni tentativo viene registrato in `/updates/registry.ndjson`.

Dal portale, la selezione del file è l'unica operazione manuale. Il browser
trasmette blocchi ordinati da 12 KiB, verificati per offset e completezza, così
anche una rete debole non dipende da una singola richiesta HTTP molto lunga.
Dopo la conferma vengono mostrati avanzamento, verifica della firma, riavvio
automatico e versione effettivamente tornata online. Un errore resta visibile
nel pannello senza essere affidato soltanto a una notifica temporanea.

Questa protezione impedisce l'installazione dal portale di un binario non
firmato. Non protegge da un attaccante con accesso fisico alla flash finché non
vengono provisionati anche Secure Boot e Flash Encryption tramite eFuse. Tali
operazioni sono irreversibili e devono essere provate sulla scheda reale con
una procedura di recupero documentata.
