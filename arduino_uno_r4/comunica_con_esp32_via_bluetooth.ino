#include <ArduinoBLE.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "ble_config.h"

// Inizializzazione Display LCD I2C (Indirizzo 0x3F)
LiquidCrystal_I2C lcd(0x3F, 16, 2);

int lastButtonState = HIGH;
bool sdAvailable = false;
char ultimaDataOraRilevata[30] = "N/D";

void attivaRele(unsigned long durata) {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("RELE' ATTIVATO");

  delay(durata);

  digitalWrite(RELAY_PIN, LOW);
  Serial.println("RELE' DISATTIVATO");
}

// Funzione ausiliaria per aggiornare l'LCD e contestualmente stampare sulla Seriale
void mostraSuLCD(const char* riga1, const char* riga2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(riga1);
  lcd.setCursor(0, 1);
  lcd.print(riga2);

  // Stampa equivalente su Serial Monitor
  Serial.print("[LCD] R1: ");
  Serial.print(riga1);
  Serial.print(" | R2: ");
  Serial.println(riga2);
}

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
    
    Serial.print("SD Log: ");
    Serial.println(messaggio);
  } else {
    Serial.println("Errore scrittura LOG.TXT su SD!");
  }
}

void gestisciConnessione(BLEDevice peripheral) {
  mostraSuLCD("Connessione...", "ESP32 Trovato");

  if (!peripheral.connect()) {
    mostraSuLCD("Errore Conn.", "Fallita!");
    delay(2000);
    return;
  }

  if (!peripheral.discoverAttributes()) {
    mostraSuLCD("Errore BLE", "Attr. non trovati");
    peripheral.disconnect();
    delay(2000);
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
      Serial.print("Data/Ora NTP ricevuta: ");
      Serial.println(ultimaDataOraRilevata);
    }
  }

  // 2. Recupero Caratteristica Pulsante
  BLECharacteristic buttonCharacteristic = peripheral.characteristic(CHARACTERISTIC_UUID);
  if (!buttonCharacteristic) {
    mostraSuLCD("Errore BLE", "Caratt. mancante");
    peripheral.disconnect();
    delay(2000);
    return;
  }

  mostraSuLCD("BLE Connesso!", "Pronto all'uso");
  scriviLogSD("Connessione BLE stabilita con ESP32.");

  while (peripheral.connected()) {
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {

    mostraSuLCD("Pulsante premuto", "Attivo rele'...");

    // =================================
    // ACCENDE IL RELE'
    // =================================
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("RELE' ATTIVATO");

    // =================================
    // INVIA COMANDO BLE
    // =================================
    boolean esitoInvio = buttonCharacteristic.writeValue((byte)0x01, true);

    if (!esitoInvio) {
      esitoInvio = buttonCharacteristic.writeValue((byte)0x01);
    }

    if (esitoInvio) {
      mostraSuLCD("Inviato OK!", ultimaDataOraRilevata);
      scriviLogSD("EVENTO: Pulsante premuto -> Rele attivato + comando BLE inviato.");
    } else {
      mostraSuLCD("Errore Invio", "Fallito BLE");
      scriviLogSD("ERRORE: Invio BLE fallito.");
    }

    // =================================
    // RELE' ACCESO PER 1,5 SECONDI
    // =================================
    delay(1500);

    // =================================
    // SPEGNE IL RELE'
    // =================================
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("RELE' DISATTIVATO");

    mostraSuLCD("BLE Connesso!", "Pronto all'uso");
  }

  lastButtonState = buttonState;
}

  mostraSuLCD("Disconnesso", "Riavvio scan...");
  scriviLogSD("Connessione BLE terminata.");
  delay(1500);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Relè inizialmente spento

  // 1. Inizializzazione BLE
  if (!BLE.begin()) {
    Serial.println("ERRORE CRITICO: Impossibile avviare ArduinoBLE!");
    Wire.begin();
    lcd.init();
    lcd.backlight();
    mostraSuLCD("ERRORE CRITICO", "BLE non avviato");
    while (1);
  }

  // 2. Inizializzazione Display LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  mostraSuLCD("Avvio Sistema", "BLE OK!");
  delay(1000);

  // 3. Inizializzazione Scheda SD (Pin 5)
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(50);

  if (SD.begin(SD_CS_PIN)) {
    sdAvailable = true;
    mostraSuLCD("SD Card: OK", "Scansione BLE...");
    scriviLogSD("Sistema avviato stand-alone.");
  } else {
    sdAvailable = false;

  
    mostraSuLCD("SD Card: NO", "Scansione BLE...");
    Serial.println("AVVISO: Scheda SD non rilevata o non funzionante.");
  }

  delay(1500);
  Serial.println("Avvio scansione periferiche BLE...");
  BLE.scan();
}

void loop() {
  // Messaggio di stato periodico durante la scansione
  static unsigned long ultimoAggiornamentoLCD = 0;
  if (millis() - ultimoAggiornamentoLCD > 4000) {
    ultimoAggiornamentoLCD = millis();
    mostraSuLCD("Scansione BLE...", "Cerca ESP32");
  }

  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    String localName = peripheral.localName();
    bool matchPerNome = (localName == TARGET_BLE_NAME || localName == "ESP32_Receiver");
    bool matchPerUUID = (peripheral.hasAdvertisedServiceUuid() && peripheral.advertisedServiceUuid() == SERVICE_UUID);

    if (matchPerNome || matchPerUUID) {
      Serial.println("MATCH BLE Trovato! Arresto scansione e avvio connessione...");
      BLE.stopScan();
      gestisciConnessione(peripheral);
      Serial.println("Ripristino scansione BLE...");
      BLE.scan();
    }
  }
}
