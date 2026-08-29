# Prompt per integrare CaskLogic

Usare questo prompt in un'attivita separata aperta sul repository CaskLogic:

```text
Nel repository C:\GESTIONALE-TESTA-E-CODA\CaskLogic-GestionaleSfuso implementa
l'integrazione del dispositivo Laveggio Printomatic. Prima leggi, senza
modificarlo, il contratto nel repository
C:\GESTIONALE-TESTA-E-CODA\Laveggio-Printomatic\docs\casklogic-integration-contract.md
e verifica i pattern gia presenti nel backend, nel realtime e nel kiosk.

Implementa POST /api/v1/scale-devices/events con autenticazione Bearer per
dispositivo, token conservati solo come hash, validazione schema version 1 e
deduplicazione atomica per device_id, boot_id e sequence. Separa rigorosamente
l'ultima misura live e la sua scadenza dalle pesate confermate e canoniche. Gli
eventi scale.snapshot e scale.heartbeat devono aggiornare il kiosk attraverso
il canale realtime esistente; device.power deve alimentare audit e notifiche
configurabili lato backend. Non usare la microSD del dispositivo come fonte
canonica e non implementare accesso NAS.

Esponi l'endpoint esclusivamente in HTTPS. Prevedi mTLS opzionale al reverse
proxy/backend, con associazione del certificato client al device_id e
rotazione/revoca amministrabile. L'heartbeat deve ricevere rapidamente una
risposta 2xx, aggiornare ultimo contatto e audit del dispositivo e non deve
attivare elaborazioni che possano far scattare impropriamente il watchdog del
gateway. Aggiungi test per CA/certificato client non valido, token revocato,
duplicati, heartbeat e timeout.

Nel kiosk mantieni sempre disponibile l'inserimento manuale. Un aggiornamento
automatico non deve sovrascrivere un campo mentre l'operatore lo sta editando e
non deve mai salvare automaticamente lordo, tara o una pesata. Per una nuova
pesata mostra il valore live come proposta di Lordo; per una bozza che possiede
gia il lordo proponilo come Tara senza toccare il lordo esistente. La conferma
dell'operatore resta obbligatoria. Mostra stato dispositivo, freschezza,
stabilita e fallback manuale quando il dato e assente o scaduto.

Aggiungi migrazioni, modelli, servizi, endpoint amministrativi per registrare e
revocare dispositivi, audit filtrabile, test backend, test frontend e verifica
browser autenticata. Riusa grafica, dialoghi e convenzioni gia presenti nel
gestionale. Non modificare flussi estranei e non eseguire deploy, commit o push
finche non te lo autorizzo esplicitamente. Alla fine elenca file modificati,
test eseguiti e decisioni ancora da confermare sull'hardware reale.
```
