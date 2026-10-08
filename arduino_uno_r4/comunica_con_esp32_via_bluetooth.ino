#include <ArduinoBLE.h>
#include <SPI.h>
#include <SD.h>
#include "ble_config.h" // BUTTON_PIN e SD_CS_PIN sono già definiti qui!

int lastButtonState = HIGH;
bool sdAvailable = false;

// Buffer fisso per data e ora
char ultimaDataOraRilevata[30] = "N/D";

void scriviLogSD(const char* messaggio) {
  if (!sdAvailable) return;

  File logFile = SD.open("LOG.TXT", FILE_WRITE);

  if (logFile) {
    logFile.print("[");
    logFile.print(ultimaDataOraRilevata);
    logFile.print("] [UPTIME: ");
    logFile.print(millis() / 1000);
    logFile.print("s] ");
    logFile.println(messaggio);
    
    logFile.flush();
    logFile.close();
    
    Serial.print("SD Log: [");
    Serial.print(ultimaDataOraRilevata);
    Serial.print("] ");
    Serial.println(messaggio);
  } else {
    Serial.println("Errore apertura LOG.TXT!");
  }
}

void gestisciConnessione(BLEDevice peripheral) {
  Serial.println("Connessione all'ESP32 in corso...");

  if (!peripheral.connect()) {
    Serial.println("Connessione BLE fallita.");
    return;
  }

  if (!peripheral.discoverAttributes()) {
    Serial.println("Ricerca attributi fallita.");
    peripheral.disconnect();
    return;
  }

  // 1. Lettura Ora NTP dall'ESP32
  BLECharacteristic timeCharacteristic = peripheral.characteristic(TIME_CHARACTERISTIC_UUID);
  if (timeCharacteristic && timeCharacteristic.canRead()) {
    timeCharacteristic.read();
    int len = timeCharacteristic.valueLength();
    if (len > 0 && len < sizeof(ultimaDataOraRilevata)) {
      timeCharacteristic.readValue(ultimaDataOraRilevata, len);
      ultimaDataOraRilevata[len] = '\0';
      Serial.print("Data e Ora sincronizzate dall'ESP32: ");
      Serial.println(ultimaDataOraRilevata);
    }
  }

  // 2. Recupero Caratteristica Pulsante
  BLECharacteristic buttonCharacteristic = peripheral.characteristic(CHARACTERISTIC_UUID);
  if (!buttonCharacteristic) {
    Serial.println("Caratteristica pulsante non trovata!");
    peripheral.disconnect();
    return;
  }

  Serial.println("Connesso all'ESP32!");
  scriviLogSD("Connessione BLE stabilita con ESP32.");

  while (peripheral.connected()) {
    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState == LOW && lastButtonState == HIGH) {
      Serial.println("Pulsante premuto: invio comando BLE...");

      boolean esitoInvio = buttonCharacteristic.writeValue((byte)0x01, true);

      if (!esitoInvio) {
        esitoInvio = buttonCharacteristic.writeValue((byte)0x01);
      }

      if (esitoInvio) {
        Serial.println("Messaggio BLE inviato con successo!");
        scriviLogSD("EVENTO: Pulsante premuto -> Comando BLE inviato.");
      } else {
        Serial.println("Errore nell'invio del messaggio BLE.");
        scriviLogSD("ERRORE: Invio BLE fallito.");
      }

      delay(250);
    }
    lastButtonState = buttonState;
  }

  Serial.println("ESP32 disconnesso.");
  scriviLogSD("Connessione BLE terminata.");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 1. Inizializzazione BLE
  if (!BLE.begin()) {
    Serial.println("Errore inizializzazione ArduinoBLE!");
    while (1);
  }
  Serial.println("Arduino BLE avviato.");

  // 2. Inizializzazione Scheda SD
  Serial.print("Inizializzazione Scheda SD sul pin ");
  Serial.print(SD_CS_PIN);
  Serial.print("... ");

  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(50);

  if (SD.begin(SD_CS_PIN)) {
    sdAvailable = true;
    Serial.println("OK!");
    scriviLogSD("Sistema avviato con scheda SD presente.");
  } else {
    sdAvailable = false;
    Serial.println("FALLITA.");
  }

  Serial.println("Avvio scansione BLE...");
  BLE.scan();
}

void loop() {
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    String localName = peripheral.localName();
    bool matchPerNome = (localName == TARGET_BLE_NAME || localName == "ESP32_Receiver");
    bool matchPerUUID = (peripheral.hasAdvertisedServiceUuid() && peripheral.advertisedServiceUuid() == SERVICE_UUID);

    if (matchPerNome || matchPerUUID) {
      Serial.println(">>> MATCH TROVATO! Avvio connessione...");
      BLE.stopScan();
      gestisciConnessione(peripheral);
      Serial.println("Ripristino scansione BLE...");
      BLE.scan();
    }
  }
}
