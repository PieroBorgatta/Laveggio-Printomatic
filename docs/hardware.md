# Hardware

## Componenti principali

### Waveshare ESP32-C6-LCD-1.47

Microcontrollore ESP32-C6 con Wi-Fi 6, Bluetooth LE e display LCD da 1,47
pollici. Nel prototipo alimenta il multiplexer a 3,3 V e usa GPIO 1/2 per il
bus I2C esterno.

Documentazione: <https://docs.waveshare.com/ESP32-C6-LCD-1.47?variant=ESP32-C6-LCD-1.47>

### Multiplexer Adafruit PCA9546/TCA9546A

Il multiplexer isola quattro rami I2C. È necessario perché tutti gli AS5600
usano lo stesso indirizzo `0x36`. Il dispositivo osservato risponde a `0x70`.

Pinout: <https://learn.adafruit.com/adafruit-pca9546-4-channel-stemma-qt-multiplexer/pinouts>

### Moduli AS5600

Sensori magnetici assoluti a 12 bit. Ogni sensore restituisce un valore da 0 a
4095 su un giro completo. I moduli del prototipo espongono `VCC`, `GND`, `SDA`,
`SCL`, `DIR`, `GPO` e `OUT`; per la lettura I2C standard sono usati soltanto i
primi quattro.

- `DIR`: può essere lasciato nel proprio stato previsto dal modulo; cambia il
  verso crescente/decrescente della misura.
- `GPO`: lasciare scollegato durante l'uso standard. Portarlo a massa abilita
  una modalità di programmazione del modulo descritto dal venditore.
- `OUT`: uscita analogica/PWM, non usata dal firmware I2C.

Riferimenti:

- <https://manuals.plus/asin/B0FH2G8PLS>
- <https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf>

## Magneti

L'AS5600 richiede un magnete magnetizzato **diametralmente**, centrato sull'asse
del chip. Il magnete deve ruotare sul proprio asse come una manopola. Un magnete
magnetizzato assialmente può essere rilevato ma non produrre una variazione
angolare corretta.

Usare un solo magnete durante le prove e tenere gli altri lontani dal sensore.
La distanza definitiva deve essere verificata sperimentalmente controllando i
flag `MD`, `ML`, `MH`, oltre alla ripetibilità dell'angolo.

## Alimentazione

La scheda ESP32 può essere alimentata tramite USB-C. Il prototipo alimenta
multiplexer e sensori dal pin 3,3 V. Tutti i dispositivi devono condividere la
massa.

Prima del montaggio definitivo verificare assorbimento, caduta di tensione,
continuità e assenza di cortocircuiti. Non collegare i moduli del prototipo al
pin 5 V senza averne verificato espressamente la compatibilità.
