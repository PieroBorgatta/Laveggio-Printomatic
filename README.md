<p align="center">
  <img src="docs/assets/casklogic-logo.png" width="280" alt="CaskLogic">
</p>

<h1 align="center">CaskLogic PesaLink</h1>

<p align="center">
  Digitalizzazione non invasiva della pesa meccanica Laveggio Printomatic del 1965.<br>
  Sistema non fiscale, destinato esclusivamente all'uso interno.
</p>

<p align="center">
  <img alt="Firmware 2.0.0" src="https://img.shields.io/badge/firmware-2.0.0-17324d?style=for-the-badge&logo=espressif&logoColor=white">
  <img alt="ESP32-S3" src="https://img.shields.io/badge/ESP32--S3-Touch_LCD-e7352c?style=for-the-badge&logo=espressif&logoColor=white">
  <img alt="PlatformIO" src="https://img.shields.io/badge/PlatformIO-build_passed-f5822a?style=for-the-badge&logo=platformio&logoColor=white">
  <img alt="Test 24 su 24" src="https://img.shields.io/badge/test-24%2F24_passed-16875b?style=for-the-badge&logo=checkmarx&logoColor=white">
</p>

<p align="center">
  <a href="#-panoramica">Panoramica</a> ·
  <a href="#-funzioni">Funzioni</a> ·
  <a href="#-calibrazione-guidata">Calibrazione</a> ·
  <a href="#-hardware">Hardware</a> ·
  <a href="#-avvio-rapido">Avvio rapido</a> ·
  <a href="#-sicurezza">Sicurezza</a> ·
  <a href="#-documentazione">Documentazione</a>
</p>

---

![Dashboard CaskLogic PesaLink per la pesa Laveggio Printomatic](docs/assets/laveggio-dashboard.png)

## 🧭 Panoramica

CaskLogic PesaLink legge le quattro manopole meccaniche della pesa Laveggio
Printomatic attraverso sensori magnetici AS5600, ricostruisce il peso stabile e conserva lo storico su
microSD. La Waveshare ESP32-S3-Touch-LCD-2.8 espone un display capacitivo
240×320 e un portale web CaskLogic per
calibrazione, diagnostica, manutenzione e integrazione con il gestionale.

Il sistema non altera la meccanica originale e mantiene sempre disponibile la
lettura locale. Il gestionale CaskLogic rimane un componente separato: questo
repository contiene firmware, simulatore, contratto di integrazione e
documentazione hardware.

> [!WARNING]
> **Digitalizzazione della pesa a uso interno, non fiscale.** Il peso acquisito
> ha finalita informative e operative interne; non sostituisce uno strumento di
> pesatura omologato ne una misura valida per transazioni commerciali, adempimenti
> fiscali o verifiche metrologiche legali.

> [!IMPORTANT]
> Il firmware ESP32-S3 V2 e il profilo compatibile V1 compilano e il simulatore
> web è stato verificato in Chromium. Poiché la nuova scheda non è ancora
> disponibile, display, touch, speaker, batteria, RTC, IMU e microSD restano da
> convalidare fisicamente al suo arrivo. Le precedenti prove sull'ESP32-C6 non
> costituiscono prova del nuovo hardware.

## ✦ Funzioni

| Area | Funzioni disponibili |
| --- | --- |
| **Acquisizione** | Quattro AS5600 isolati tramite PCA9546/TCA9546A, calibrazione di dieci posizioni per manopola, tolleranza, isteresi e stabilità |
| **Interfacce** | Display verticale a card, swipe orizzontale fra cinque pagine, scroll verticale, selezione touch dal footer, dashboard web responsive e tema chiaro/scuro |
| **Storico** | Prime 20 pesate al caricamento, export coerente con i filtri, file NDJSON settimanali e retention configurabile su microSD |
| **Autodiagnosi** | Test di sensori, microSD, batteria, touch, speaker, IMU, RTC, Wi-Fi, DNS, gestionale e heap; grafici 24 ore e contatori di errore |
| **Audio** | Doppio tono asincrono su PCM5101 alla conferma di una nuova pesata, disabilitabile e persistente dal portale web |
| **Assistenza** | Pacchetto ZIP anonimizzato con stato, diagnostica, log e registro aggiornamenti |
| **Rete** | DHCP o IP statico, scansione Wi-Fi e access point di emergenza `PesaLink_casklogic-192_168_4_1` |
| **Integrazione** | HTTPS/mTLS, MQTT TLS opzionale, heartbeat, eventi firmati HMAC-SHA256, metriche Prometheus e configurazione remota versionata |
| **Aggiornamenti** | OTA firmato ECDSA-P256, doppia partizione, validazione al riavvio, rollback e registro degli esiti |

Il firmware non usa una coda persistente per gli eventi verso il gestionale e
non trasforma la microSD in un NAS.

## 🖥️ Pagine del portale

| Pagina | Cosa permette di fare |
| --- | --- |
| **Riepilogo** | Leggere peso, stabilità, sensori, Wi-Fi, microSD, alimentazione e batteria. Display e conferma sonora hanno interruttori indipendenti e persistenti. |
| **Calibrazione** | Associare a ciascuna manopola le dieci posizioni `0–9`, salvare il valore magnetico reale e regolare moltiplicatore, tolleranza e isteresi senza ricompilare il firmware. |
| **Storico** | Consultare soltanto le 20 pesate più recenti al primo accesso, filtrare e ordinare ogni colonna ed esportare esattamente il risultato dei filtri attivi. |
| **Rete e gestionale** | Cercare reti Wi-Fi, scegliere DHCP o IP statico, configurare HTTPS/mTLS, HMAC, heartbeat, MQTT TLS e sincronizzazione controllata dal gestionale. |
| **Sistema** | Gestire display, speaker, NTP/RTC, credenziali, retention, batteria, log, riavvio e OTA firmato. Swipe e barra touch cambiano pagina; BOOT breve resta disponibile e la pressione continua mostra il ripristino da 10 secondi. |
| **Autodiagnosi** | Eseguire prove attive, controllare errori e magneti dei sensori, osservare i grafici giornalieri e scaricare un pacchetto assistenza anonimizzato. |
| **CaskLogic** | Consultare contatti di assistenza, titolarità, crediti, versione e informazioni legali del dispositivo. |

## 🎛️ Calibrazione guidata

![Calibrazione dei quattro sensori](docs/assets/laveggio-calibrazione.png)

Ogni sensore rappresenta una cifra della pesa. I valori predefiniti dei quattro
moltiplicatori sono `10.000`, `1.000`, `100` e `10 kg`; devono essere confermati
durante il collaudo meccanico reale.

### Procedura

1. Selezionare il sensore corrispondente alla manopola da calibrare.
2. Portare fisicamente la manopola sulla cifra `0`.
3. Verificare che **Qualità magnete** indichi `Regolare` e che il valore corrente
   sia stabile.
4. Premere il riquadro `0 · Memorizza`: il firmware salva in NVS il valore
   grezzo AS5600 letto in quell'istante.
5. Ripetere la stessa operazione per le posizioni da `1` a `9`.
6. Impostare moltiplicatore, tolleranza e isteresi, quindi premere
   **Salva parametri**.
7. Ripetere la procedura per tutti e quattro i sensori, fino a raggiungere
   `40/40 punti`.

Un singolo punto può essere rimemorizzato in qualsiasi momento cliccando di
nuovo la relativa cifra. **Azzera canale** elimina soltanto i dieci punti del
sensore selezionato e richiede conferma; non modifica gli altri canali.

| Parametro | Effetto |
| --- | --- |
| **Moltiplicatore kg** | Determina il contributo della cifra al peso totale: `posizione × moltiplicatore`. |
| **Tolleranza** | Distanza angolare massima tra lettura corrente e punto memorizzato affinché la cifra sia considerata valida. |
| **Isteresi** | Mantiene la posizione precedente entro una fascia aggiuntiva, evitando passaggi continui fra due cifre causati da vibrazioni o gioco meccanico. |
| **Finestra di stabilità** | Tempo durante il quale tutte le cifre devono restare invariate prima che la misura diventi una nuova pesata stabile. |

Il confronto usa la distanza circolare dell'AS5600, quindi gestisce
correttamente anche il passaggio fra `4095` e `0`. Se un magnete è assente,
troppo debole, troppo forte, fuori tolleranza o non calibrato, l'intera misura
rimane non valida e non viene registrata come nuova pesata.

## ⚙️ Dalla lettura alla pesata

1. Il multiplexer interroga continuamente i quattro AS5600 a `100 kHz`.
2. Ogni valore grezzo viene confrontato con i dieci punti del proprio canale.
3. Tolleranza e isteresi determinano la cifra riconosciuta senza oscillazioni.
4. Le quattro cifre vengono moltiplicate e sommate per ottenere i chilogrammi.
5. La combinazione deve restare invariata per la finestra di stabilità.
6. Solo una combinazione valida, stabile e diversa dalla precedente genera una
   nuova riga nello storico e un evento firmato verso il gestionale.

La microSD conserva lo storico indipendentemente dalla connessione di rete, ma
non viene usata come coda automatica di reinvio.

## 📊 Autodiagnosi

![Autodiagnosi CaskLogic PesaLink](docs/assets/laveggio-autodiagnosi.png)

La corrente assorbita è indicata come non disponibile perché la scheda non
integra un sensore amperometrico. Tensione batteria, temperatura della scheda,
RSSI e memoria libera sono invece registrabili senza aggiungere altre schede.

## 🧩 Architettura

```mermaid
flowchart LR
    A[4 manopole] --> B[4 sensori AS5600]
    B --> C[PCA9546 Adafruit 5663]
    C --> D[ESP32-S3 Touch LCD 2.8]
    D --> E[ST7789 + CST3530/CST328]
    D --> F[MicroSD SDMMC]
    D --> G[Portale web CaskLogic]
    D -->|HTTPS / mTLS| H[Gestionale]
    D -->|MQTT TLS opzionale| H
    I[LiPo 1S] --> D
    D --> K[PCM5101 + speaker]
    D --> L[QMI8658 + PCF85063]
    M[Antenna Wi-Fi 2,4 GHz] -->|SMA + pigtail IPEX1| D
```

## 🔩 Hardware

| Componente | Quantità | Riferimento |
| --- | ---: | --- |
| Waveshare ESP32-S3-Touch-LCD-2.8, 16 MB flash / 8 MB PSRAM | 1 | [Documentazione ufficiale](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8) |
| Adafruit PCA9546 compatto a 4 canali, cod. 5663 (compatibile TCA9546A) | 1 | [Amazon.it · B0BSF13WD7](https://www.amazon.it/dp/B0BSF13WD7) |
| Kit moduli AS5600 a 12 bit con magneti | 1 kit | [Amazon.it · B0FH1Y3GLG](https://www.amazon.it/dp/B0FH1Y3GLG) |
| Cavetti micro JST-SH 1,0 mm, 4 pin | 1 confezione | [Amazon.it · B0BNCHC5Q4](https://www.amazon.it/dp/B0BNCHC5Q4) |
| Pigtail IPEX-1/U.FL verso SMA femmina, 15 cm | 1 confezione da 5 | [Amazon.it · B07YBYMBSV](https://www.amazon.it/dp/B07YBYMBSV) |
| Antenna Wi-Fi 2,4 GHz 2 dBi, SMA maschio | 1 confezione da 2 | [Amazon.it · B0CR5JPMNX](https://www.amazon.it/dp/B0CR5JPMNX) |
| Cavo dati Lapp LiYY 4 × 0,14 mm², nero | 1 spezzone da 10 m | [Amazon.it · B0C69CJYZT](https://www.amazon.it/dp/B0C69CJYZT) |
| Grani M5 × 12 mm con punta, inox A2, DIN 914 / ISO 4027 | 1 confezione da 20 | [Amazon.it · B0BZD8WXDQ](https://www.amazon.it/dp/B0BZD8WXDQ) |
| Speaker 8 Ω 2 W 2030 | 1 | Incluso con la scheda Waveshare |
| Batteria LiPo 3,7 V | 1 | Connettore MX1.25 2 pin; polarità da verificare |
| MicroSD FAT32 | 1 | La scheda dichiara supporto fino a 16 GB |

La nuova Waveshare integra gestione di carica, misura batteria, RTC, IMU,
codec PCM5101, amplificatore e slot microSD. Collegamenti e verifiche sono
descritti in [`docs/power-and-ups.md`](docs/power-and-ups.md).

Il PCA9546 cod. 5663 sostituisce il precedente modello STEMMA QT cod. 5664:
richiede intestazioni o fili saldati e non e compatibile con il relativo case
STL. Il pigtail e l'antenna esterna richiedono inoltre lo spostamento della
resistenza di selezione RF previsto da Waveshare; l'antenna IPEX1 non e attiva
semplicemente collegando il cavetto.

## 🚀 Avvio rapido

### Simulatore web

```powershell
cd firmware/production-gateway
npm test
npm run simulate
```

Il portale risponde su `http://127.0.0.1:4177`. Il simulatore usa gli stessi
file HTML, CSS e JavaScript incorporati nel firmware.

### Build ESP32-S3

```powershell
cd firmware/production-gateway
$env:PLATFORMIO_CORE_DIR = 'C:\pio'
py -m platformio run
```

| Risorsa | Utilizzo verificato |
| --- | ---: |
| RAM statica | `54.436 / 327.680 byte` · `16,6%` |
| Flash applicazione | `1.657.078 / 6.291.456 byte` · `26,3%` |
| Profili compilati | V2 `CST3530` e V1 `CST328`, entrambi con autodetect di fallback |
| Slot OTA | `6 MiB` ciascuno su flash da 16 MB |

Artefatti principali:

- `firmware.signed.bin`: aggiornamento accettato dal portale OTA;
- `firmware.factory.bin`: prima installazione completa, inclusa la tabella
  partizioni;
- `firmware.bin`: input non firmato, non caricabile dal portale.

## 🛡️ Sicurezza

<p>
  <img alt="ECDSA signed OTA" src="https://img.shields.io/badge/OTA-ECDSA--P256_signed-16875b?style=flat-square&logo=letsencrypt&logoColor=white">
  <img alt="TLS" src="https://img.shields.io/badge/backend-TLS_%2F_mTLS-28678e?style=flat-square&logo=openssl&logoColor=white">
  <img alt="HMAC SHA256" src="https://img.shields.io/badge/events-HMAC--SHA256-5b677a?style=flat-square&logo=databricks&logoColor=white">
  <img alt="HTTP Basic" src="https://img.shields.io/badge/portal-HTTP_Basic-c98b2e?style=flat-square&logo=auth0&logoColor=white">
</p>

- autenticazione HTTP Basic, rate limit, CSRF e header browser restrittivi;
- endpoint remoti esclusivamente TLS con CA obbligatoria e mTLS opzionale;
- segreto HMAC distinto per la firma delle pesate;
- token dedicato per `GET /api/metrics`;
- configurazione remota limitata a una whitelist di campi operativi;
- chiave privata OTA esterna al repository;
- pacchetto assistenza senza credenziali, SSID o indirizzi IP.

Il portale locale usa HTTP e deve restare su una VLAN tecnica, senza port
forwarding verso Internet. Secure Boot e Flash Encryption tramite eFuse
richiedono provisioning fisico irreversibile sulla scheda reale. Dettagli in
[`docs/security.md`](docs/security.md) e
[`docs/firmware-signing.md`](docs/firmware-signing.md).

## 🗂️ Struttura

| Percorso | Contenuto |
| --- | --- |
| [`firmware/production-gateway`](firmware/production-gateway/) | Gateway operativo ESP32-S3 touch e simulatore web |
| [`firmware/calibration-reader`](firmware/calibration-reader/) | Firmware diagnostico usato nelle prime prove hardware |
| [`docs`](docs/) | Cablaggio, alimentazione, sicurezza, test e contratto gestionale |
| [`stl`](stl/) | Case e supporti stampabili in 3D con licenze separate |

## 📚 Documentazione

- [Hardware e distinta materiali](docs/hardware.md)
- [Cablaggio dei sensori](docs/wiring.md)
- [Batteria e alimentazione della nuova scheda](docs/power-and-ups.md)
- [Collaudo hardware al ricevimento](docs/friday-hardware-validation.md)
- [Architettura operativa](docs/production-architecture.md)
- [Sicurezza del gateway](docs/security.md)
- [Firma e rilascio firmware](docs/firmware-signing.md)
- [Contratto futuro con CaskLogic](docs/casklogic-integration-contract.md)
- [Prompt per implementare il lato gestionale](docs/casklogic-integration-prompt.md)
- [Risultati delle verifiche](docs/test-results.md)

## Stato del progetto

- [x] Lettura AS5600 e multiplexer verificata sul prototipo
- [x] Gateway, portale web e simulatore implementati
- [x] Test host `24/24`, browser Chromium e build firmate ESP32-S3 V2/V1 completati
- [x] Doppia partizione OTA da 6 MiB e firma ECDSA generate per entrambi i profili
- [ ] Collaudo fisico del nuovo ST7789, touch, speaker, RTC, IMU e batteria
- [ ] Calibrazione meccanica completa delle quattro manopole
- [ ] Collaudo reale di microSD FAT32, batteria, commutazione e rollback forzato
- [ ] Implementazione del contratto nel gestionale CaskLogic

## Licenza e attribuzioni

Il codice e la documentazione sono distribuiti con licenza
[Creative Commons Attribution 4.0 International](LICENSE), salvo indicazioni
diverse nei singoli file. È richiesta l'attribuzione a **Piero Borgatta** e ai
contributori del progetto.

Gli STL di terzi mantengono licenza e attribuzione originali, documentate in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

<p align="center">
  <strong>CaskLogic Solutions</strong><br>
  CaskLogic PesaLink · preservare la meccanica, rendere misurabile il dato
</p>
