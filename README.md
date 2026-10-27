# 🔔 Smart IoT Doorbell (Arduino UNO R4 WiFi)

Un sistema di campanello intelligente basato su **Arduino UNO R4 WiFi**, progettato per essere estremamente stabile, reattivo e totalmente autonomo. Alla pressione del pulsante, il sistema attiva un relè (es. sirena o campanello), registra un log cronologico su scheda SD, aggiorna un display LCD I2C e invia una notifica istantanea direttamente tramite HTTPS al tuo bot di **Telegram**.

---

## 🚀 Caratteristiche Principali
- **Notifiche Telegram Dirette:** Sfrutta il modulo Wi-Fi integrato dell'Arduino R4 per inviare richieste HTTPS direttamente ad `api.telegram.org` (senza bisogno di ponti BLE o dispositivi intermediari).
- **Web Server Locale:** Un server integrato consente di visualizzare lo storico degli eventi direttamente da browser inserendo l'indirizzo IP locale dell'Arduino, con opzione per svuotare il registro log.
- **Registrazione su Scheda SD:** Ogni evento viene tracciato con uptime e timestamp su file `LOG.TXT`.
- **Stabilità Elevata:** Architettura snella e priva di conflitti hardware (rimosso il sottosistema BLE per azzerare i reset per brownout/sovraccarico).

---

## 🛠️ Hardware Utilizzato
- **Microcontrollore:** Arduino UNO R4 WiFi
- **Display:** LCD 16x2 con interfaccia I2C (indirizzo tipico `0x27` o `0x3F`)
- **Modulo SD Card:** Collegato tramite SPI (Pin CS su Pin 5)
- **Relè / Uscita:** Connesso al Pin 4
- **Pulsante:** Connesso al Pin 2 (con resistenza di pull-up interna)

---

## 📦 Librerie Richieste (Arduino IDE)
Assicurati di installare le seguenti librerie tramite il *Gestore Librerie* dell'IDE di Arduino:
- `WiFiS3` (nativa per Arduino UNO R4 WiFi)
- `SD` (per la gestione della scheda SD)
- `LiquidCrystal_I2C` (per il display LCD)

---

## ⚙️ Configurazione
1. Clona o scarica il repository.
2. Crea un file denominato `config.h` nella stessa cartella dello sketch e inserisci le tue credenziali:
   ```cpp
   #ifndef CONFIG_H
   #define CONFIG_H

   #define WIFI_SSID "IL_TUO_SSID_WIFI"
   #define WIFI_PASSWORD "TUA_PASSWORD_WIFI"

   #define TELEGRAM_BOT_TOKEN "IL_TUO_BOT_TOKEN_QUI"
   #define TELEGRAM_CHAT_ID "IL_TUO_CHAT_ID_QUI"

   #endif
