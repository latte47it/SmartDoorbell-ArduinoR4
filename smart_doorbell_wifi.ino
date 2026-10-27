#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "WiFiS3.h"
#include "config.h"       // Contiene WIFI_SSID, WIFI_PASSWORD, TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID

// Definizione Pin
#define BUTTON_PIN 2
#define RELAY_PIN 4
#define SD_CS_PIN 5

// Inizializzazione WebServer sulla porta 80
WiFiServer server(80);
bool wifiAvailable = false;

// Inizializzazione Display LCD I2C (Indirizzo 0x3F o 0x27)
LiquidCrystal_I2C lcd(0x3F, 16, 2);

int lastButtonState = HIGH;
bool sdAvailable = false;

// Funzione ausiliaria per aggiornare l'LCD e contestualmente stampare sulla Seriale
void mostraSuLCD(const char* riga1, const char* riga2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(riga1);
  lcd.setCursor(0, 1);
  lcd.print(riga2);

  Serial.print("[LCD] R1: ");
  Serial.print(riga1);
  Serial.print(" | R2: ");
  Serial.println(riga2);
}

void scriviLogSD(const char* messaggio) {
  if (!sdAvailable) return;

  File logFile = SD.open("LOG.TXT", FILE_WRITE);
  if (logFile) {
    logFile.print("[UPTIME: ");
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

// Funzione per l'invio diretto del messaggio Telegram via Wi-Fi (HTTPS)
void inviaMessaggioTelegram(String messaggio) {
  if (!wifiAvailable) {
    Serial.println("Wi-Fi non disponibile, impossibile inviare Telegram.");
    return;
  }

  WiFiSSLClient client;
  const char* serverName = "api.telegram.org";

  Serial.println("Connessione a Telegram in corso...");
  
  if (client.connect(serverName, 443)) {
    messaggio.replace(" ", "%20"); // Sostituisce gli spazi per l'URL
    String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage?chat_id=" + String(TELEGRAM_CHAT_ID) + "&text=" + messaggio;
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n");
    client.print(String("Host: ") + serverName + "\r\n");
    client.print("Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 5000) {
      while (client.available()) {
        client.read();
      }
    }
    
    client.stop();
    Serial.println("Messaggio Telegram inviato con successo!");
    scriviLogSD("Telegram: Notifica inviata correttamente.");
  } else {
    Serial.println("Connessione a Telegram fallita.");
    scriviLogSD("ERRORE: Connessione a Telegram fallita.");
  }
}

// Gestione delle richieste HTTP dal browser (visualizzazione e pulizia log)
void gestisciWebServer() {
  if (!wifiAvailable) return;

  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String requestString = ""; 
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        requestString += c;
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            bool isClearPage = (requestString.indexOf("GET /clear") != -1);

            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html; charset=utf-8");
            client.println("Connection: close");
            client.println();
            
            client.println("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Campanello Smart</title>");
            client.println("<style>");
            client.println("body{font-family:Arial, sans-serif; margin:40px; background:#f9f9f9; color:#333;}");
            client.println("h2{color:#2c3e50;}");
            client.println(".menu{margin-bottom:20px;}");
            client.println(".btn{background:#3498db; color:white; padding:10px 20px; text-decoration:none; border-radius:4px; margin-right:10px; display:inline-block;}");
            client.println(".btn-danger{background:#e74c3c;}");
            client.println(".btn:hover{opacity:0.9;}");
            client.println("pre{background:#fff; padding:15px; border:1px solid #ddd; border-radius:5px; max-height:400px; overflow-y:auto;}");
            client.println("</style></head><body>");
            
            client.println("<h2>Gestione Campanello IoT</h2>");
            client.println("<div class='menu'>");
            client.println("<a href='/' class='btn'>Visualizza Log</a>");
            client.println("<a href='/clear' class='btn btn-danger' onclick='return confirm(\"Sei sicuro di voler cancellare definitivamente tutti i log?\");'>Svuota Log</a>");
            client.println("</div><hr>");

            if (isClearPage) {
              if (sdAvailable) {
                if (SD.exists("LOG.TXT")) {
                  SD.remove("LOG.TXT");
                  Serial.println("File LOG.TXT cancellato con successo da web.");
                }
                client.println("<p style='color:green; font-weight:bold;'>Registro log svuotato con successo!</p>");
              } else {
                client.println("<p style='color:red;'>Errore: Scheda SD non disponibile.</p>");
              }
              client.println("<p><a href='/'>Torna alla visualizzazione log</a></p>");
            } else {
              client.println("<h3>Registro Eventi (LOG.TXT)</h3>");
              client.println("<pre>");
              
              if (sdAvailable && SD.exists("LOG.TXT")) {
                File logFile = SD.open("LOG.TXT", FILE_READ);
                while (logFile.available()) {
                  client.write(logFile.read());
                }
                logFile.close();
              } else {
                client.println("Nessun file LOG.TXT trovato sulla scheda SD (registro vuoto).");
              }
              
              client.println("</pre>");
            }
            
            client.println("</body></html>");
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);         
  digitalWrite(RELAY_PIN, LOW);        
  digitalWrite(LED_BUILTIN, LOW);      

  // 1. Inizializzazione Display LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  mostraSuLCD("Avvio Sistema", "Inizializzazione...");
  delay(1000);

  // 2. Inizializzazione Scheda SD (Pin 5)
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(50);

  if (SD.begin(SD_CS_PIN)) {
    sdAvailable = true;
    Serial.println("Scheda SD inizializzata con successo.");
  } else {
    sdAvailable = false;
    Serial.println("AVVISO: Scheda SD non rilevata o non funzionante.");
  }

  // 3. Connessione Wi-Fi
  Serial.println("Tentativo di connessione Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 7000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connesso!");
    Serial.print("Attendendo l'indirizzo IP dal router");

    unsigned long ipWait = millis();
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - ipWait < 10000) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();

    if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      wifiAvailable = true;
      Serial.print("Indirizzo IP WebServer SD: http://");
      Serial.println(WiFi.localIP());
      server.begin();
      mostraSuLCD("Wi-Fi OK", "Server SD attivo");
    } else {
      wifiAvailable = false;
      Serial.println("\nTimeout DHCP: Impossibile ottenere l'IP dal router.");
      mostraSuLCD("Errore IP", "Timeout DHCP");
    }

  } else {
    wifiAvailable = false;
    Serial.println("\nWi-Fi non disponibile.");
    mostraSuLCD("Wi-Fi Assente", "Modalita' Offline");
  }

  delay(1500);
  mostraSuLCD("Pronto", "Sistema Attivo");
}

void loop() {
  // Gestione continua richieste Web Server
  gestisciWebServer();

  // Lettura Pulsante (Sempre attiva e reattiva)
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50); // Debounce anti-rimbalzo
    if (digitalRead(BUTTON_PIN) == LOW) {
      
      Serial.println("PULSANTE PREMUTO!");
      mostraSuLCD("Campanello!", "Attivo rele'...");

      digitalWrite(RELAY_PIN, HIGH); // Attiva sirena/relè
      digitalWrite(LED_BUILTIN, HIGH); 
      Serial.println("RELE' E LED ATTIVATI");

      // Registrazione su SD
      scriviLogSD("EVENTO: Pulsante premuto -> Rele attivato.");

      // Invio notifica Telegram diretta tramite Wi-Fi
      inviaMessaggioTelegram("🔔 Campanello premuto! Qualcuno e' alla porta.");

      delay(1500); // Durata attivazione

      digitalWrite(RELAY_PIN, LOW); // Spegne sirena/relè
      digitalWrite(LED_BUILTIN, LOW); 
      Serial.println("RELE' E LED DISATTIVATI");

      mostraSuLCD("Pronto", "Sistema Attivo");
    }
  }
  lastButtonState = buttonState;
}