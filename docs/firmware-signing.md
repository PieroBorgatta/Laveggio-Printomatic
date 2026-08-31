# Firma e rilascio firmware

Il portale OTA, dalla versione 1.2.0, accetta soltanto immagini
`firmware.signed.bin` firmate ECDSA-P256 con SHA-256. La chiave pubblica e
incorporata in `include/OtaPublicKey.h`; la chiave privata non deve mai entrare
nel repository, nel dispositivo o nel pacchetto assistenza.

## Chiave di firma

Il build script cerca per impostazione predefinita:

```text
%USERPROFILE%\.casklogic\laveggio-signing\private_key.pem
```

La posizione puo essere sostituita con `LAVEGGIO_SIGNING_KEY`. La chiave va
conservata in un archivio cifrato, con backup offline e accesso limitato agli
operatori autorizzati. La perdita della chiave impedisce nuovi aggiornamenti
OTA ai dispositivi che incorporano la relativa chiave pubblica; una sua
compromissione richiede una procedura fisica controllata di rotazione.

Per rigenerare l'header pubblico da un PEM pubblico:

```powershell
py tools/public_key_to_header.py `
  C:\percorso\public_key.pem `
  include/OtaPublicKey.h
```

## Build e verifica

`platformio.ini` esegue automaticamente `tools/sign_firmware.py` dopo la
compilazione. Il file caricato dal portale deve essere quello firmato:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:\pio'
py -m platformio run
```

La firma DER e aggiunta al binario in un blocco finale da 512 byte, come
richiesto da `Update.installSignature()`. Il firmware verifica firma e
dimensione prima di selezionare la nuova partizione. Al primo avvio esegue i
controlli minimi e marca l'immagine valida; in caso contrario richiede il
rollback. Ogni tentativo viene registrato in `/updates/registry.ndjson`.

Dal portale, la selezione del file e l'unica operazione manuale. Il browser
trasmette blocchi ordinati da 12 KiB, verificati per offset e completezza, cosi
anche una rete debole non dipende da una singola richiesta HTTP molto lunga.
Dopo la conferma vengono mostrati avanzamento, verifica della firma, riavvio
automatico e versione effettivamente tornata online. Un errore resta visibile
nel pannello senza essere affidato soltanto a una notifica temporanea.

Questa protezione impedisce l'installazione dal portale di un binario non
firmato. Non protegge da un attaccante con accesso fisico alla flash finche non
vengono provisionati anche Secure Boot e Flash Encryption tramite eFuse. Tali
operazioni sono irreversibili e devono essere provate sulla scheda reale con
una procedura di recupero documentata.
