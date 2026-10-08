#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>
#include "ble_config.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;       // Ora solare (+1h)
const int   daylightOffset_sec = 3600;  // Ora legale (+1h)

volatile bool inviaTelegramFlag = false;
bool deviceConnected = false;

BLECharacteristic *pTimeCharacteristic;
BLECharacteristic *pButtonCharacteristic;

// Callback per gestire i comandi in arrivo via BLE
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
      uint8_t* data = pCharacteristic->getData();
      size_t len = pCharacteristic->getLength();

      if (len > 0 && data[0] == 0x01) {
        Serial.println("-> EVENTO RILEVATO: Pulsante premuto su Arduino!");
        inviaTelegramFlag = true; // Attiva il flag per il loop principale
      }
    }
};

// Callback per la connessione/disconnessione BLE
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
      deviceConnected = true;
      Serial.println(">>> Arduino connesso via BLE!");

      // Sincronizza e invia l'ora NTP attuale ad Arduino
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeBuf[30];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        pTimeCharacteristic->setValue(timeBuf);
        Serial.println("Ora NTP inviata via BLE: " + String(timeBuf));
      }
    }

    void onDisconnect(BLEServer* pServer) override {
      deviceConnected = false;
      Serial.println(">>> Arduino disconnesso. Riavvio Advertising...");
      pServer->startAdvertising();
    }
};

void inviaMessaggioTelegram(String messaggio) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TELEGRAM] Wi-Fi non connesso!");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Ignora la verifica del certificato SSL per Telegram

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",\"text\":\"" + messaggio + "\"}";
  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("[TELEGRAM] Messaggio inviato! Risposta HTTP: %d\n", httpCode);
  } else {
    Serial.printf("[TELEGRAM] Errore invio: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  // Connessione Wi-Fi
  Serial.print("Connessione Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connesso! IP: " + WiFi.localIP().toString());

  // Sincronizzazione NTP
  Serial.println("Sincronizzazione orario NTP in corso...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuf[30];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.println("Ora attuale: " + String(timeBuf));
  }

  // Inizializzazione BLE
  BLEDevice::init(TARGET_BLE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pButtonCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ |
                            BLECharacteristic::PROPERTY_WRITE |
                            BLECharacteristic::PROPERTY_WRITE_NR
                          );
  pButtonCharacteristic->setCallbacks(new CharacteristicCallbacks());

  pTimeCharacteristic = pService->createCharacteristic(
                          TIME_CHARACTERISTIC_UUID,
                          BLECharacteristic::PROPERTY_READ
                        );

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("ESP32 pronto e sincronizzato.");
}

void loop() {
  if (inviaTelegramFlag) {
    inviaTelegramFlag = false;
    inviaMessaggioTelegram("🔔 Evento da Arduino: Pulsante Premuto!");
  }
  delay(10);
}