/*
 * STC-1000 ESP8266 Professional Controller
 * Copyright 2026 Refactor
 */

#include "Config.h"
#include "STCManager.h"
#include "DataLogger.h"

#include <ESP_DoubleResetDetector.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Components
STCManager stc;
DataLogger logger;
WiFiManager wifiManager;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDR);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Firebase
FirebaseData fbdo;
FirebaseData streamFbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Global State
unsigned long lastLiveUpdate = 0;
unsigned long lastLogUpdate = 0;

// --- Firebase Callbacks ---

void streamCallback(FirebaseStream data) {
    Serial.printf("[Stream] Path: %s | Payload: %s\n", data.dataPath().c_str(), data.payload().c_str());
    
    if (data.dataPath() == "/setpoint") {
        stc.writeSetpoint(data.to<float>());
    }
    // Handle other parameters here
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) Serial.println("[Stream] Timed out, resuming...");
}

// --- Setup Helpers ---

FirebaseCredentials loadCredentials() {
    FirebaseCredentials cred;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(FB_CRED_ADDR, cred);
    return cred;
}

void saveCredentials(const char* key, const char* url, const char* user, const char* pass) {
    FirebaseCredentials cred;
    strlcpy(cred.api_key, key, 50);
    strlcpy(cred.database_url, url, 150);
    strlcpy(cred.username, user, 100);
    strlcpy(cred.password, pass, 50);
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(FB_CRED_ADDR, cred);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("[System] Credentials saved to EEPROM.");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[System] Starting STC-1000 ESP...");

    stc.begin();

    // 1. WiFi Manager Configuration
    char fb_api[50] = {0}, fb_url[150] = {0}, fb_user[100] = {0}, fb_pass[50] = {0};
    WiFiManagerParameter p_api("api", "Firebase API Key", fb_api, 50);
    WiFiManagerParameter p_url("url", "Firebase URL", fb_url, 150);
    WiFiManagerParameter p_user("user", "Firebase Username", fb_user, 100);
    WiFiManagerParameter p_pass("pass", "Firebase Password", fb_pass, 50);

    wifiManager.addParameter(&p_api);
    wifiManager.addParameter(&p_url);
    wifiManager.addParameter(&p_user);
    wifiManager.addParameter(&p_pass);

    if (drd.detectDoubleReset()) {
        Serial.println("[System] Double Reset Detected! Entering Config Portal...");
        wifiManager.startConfigPortal("STC1000_Setup");
        saveCredentials(p_api.getValue(), p_url.getValue(), p_user.getValue(), p_pass.getValue());
        ESP.restart();
    } else {
        wifiManager.autoConnect("STC1000_Setup");
    }

    // 2. Firebase Initialization
    FirebaseCredentials creds = loadCredentials();
    config.api_key = creds.api_key;
    config.database_url = creds.database_url;
    config.token_status_callback = tokenStatusCallback;
    auth.user.email = creds.username;
    auth.user.password = creds.password;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // 3. Start Streaming
    if (Firebase.RTDB.beginStream(&streamFbdo, "STC1000get")) {
        Firebase.RTDB.setStreamCallback(&streamFbdo, streamCallback, streamTimeoutCallback);
    }

    // 4. Other Services
    timeClient.begin();
    ArduinoOTA.setHostname("STC1000-ESP");
    ArduinoOTA.begin();
    
    Serial.println("[System] Initialization Complete.");
}

void loop() {
    drd.loop();
    ArduinoOTA.handle();

    if (WiFi.status() != WL_CONNECTED) return;

    // Task 1: Periodic Live Status Update (Every 15s)
    if (millis() - lastLiveUpdate > LIVE_UPDATE_MS) {
        lastLiveUpdate = millis();
        float temp;
        if (stc.getTemperature(temp)) {
            FirebaseJson json;
            json.set("temperature", temp);
            json.set("setpoint", stc.getSetpoint());
            json.set("heating", stc.isHeating());
            json.set("cooling", stc.isCooling());
            json.set("epoch", timeClient.getEpochTime());
            
            Firebase.RTDB.setJSON(&fbdo, "STC1000set", &json);
        }
    }

    // Task 2: Periodic Log Capture (Every 15m)
    if (millis() - lastLogUpdate > LOG_INTERVAL_MS) {
        lastLogUpdate = millis();
        float temp;
        if (stc.getTemperature(temp)) {
            logger.addEntry(timeClient.getEpochTime(), temp, stc.isHeating(), stc.isCooling());
        }
    }

    // Task 3: Sync Logs to Firebase (When buffer has data)
    if (logger.getCount() > 0 && Firebase.ready()) {
        logger.flush(fbdo);
    }
}
