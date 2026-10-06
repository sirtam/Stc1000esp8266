/*
 * STC-1000 ESP32 Professional Controller
 * Optimized for WROOM-32 with FreeRTOS Dual-Core & Native RTC
 */

#include "Config.h"
#include "STCManager.h"
#include "DataLogger.h"

#include <WiFi.h>
#include <WiFiManager.h>

// Custom RTC-based Double Reset Detector
#define DRD_MAGIC 0x44524401 // "DRD1"
RTC_DATA_ATTR uint32_t rtc_drd_flag;
bool doubleResetDetected = false;

void checkDoubleReset() {
    if (rtc_drd_flag == DRD_MAGIC) {
        doubleResetDetected = true;
        rtc_drd_flag = 0; // Clear it
    } else {
        rtc_drd_flag = DRD_MAGIC;
        // Start a timer to clear the flag after 10 seconds
        // On ESP32, we do this by letting the code run and 
        // clearing it later in setup or a task.
    }
}

const int CONFIG_PIN = 0; // ESP32 BOOT button
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Components
STCManager stc;
DataLogger logger;
WiFiManager wifiManager;
Preferences preferences;

// FreeRTOS Task Handles & Mutex
TaskHandle_t stcTaskHandle;
TaskHandle_t cloudTaskHandle;
SemaphoreHandle_t stcMutex;

// Firebase
FirebaseData fbdo;
FirebaseData streamFbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Global State
unsigned long lastLiveUpdate = 0;
unsigned long lastLogUpdate = 0;

// --- Time Helper ---
uint32_t getEpochTime() {
    time_t now;
    time(&now);
    return now; // Native high-accuracy RTC
}

// --- Firebase Callbacks ---
void streamCallback(FirebaseStream data) {
    Serial.printf("[Stream] Path: %s | Payload: %s\n", data.dataPath().c_str(), data.payload().c_str());
    
    String path = data.dataPath();
    
    if (xSemaphoreTake(stcMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        if (path == "/setpoint") {
            stc.writeSetpoint(data.to<float>());
        } 
        else if (path == "/ramping") {
            stc.setRamping(data.to<bool>());
        } 
        else if (path == "/runMode") {
            stc.setRunMode(data.to<int>());
        }
        else if (path.startsWith("/profiles/")) {
            // Path format: /profiles/0/steps/1
            // We can parse this to program a specific step
            // Example simplified logic for now:
            Serial.println("[Stream] Profile update received. Please use manual programming for now or expand this parser.");
        }
        xSemaphoreGive(stcMutex);
    }
}

void streamTimeoutCallback(bool timeout) {
    if (timeout) Serial.println("[Stream] Timed out, resuming...");
}

// --- Setup Helpers ---
FirebaseCredentials loadCredentials() {
    FirebaseCredentials cred = {0};
    preferences.begin(PREF_NAMESPACE, true);
    
    // Read individual strings for maximum reliability
    String apiKey = preferences.getString("api", "");
    String dbUrl = preferences.getString("url", "");
    String user = preferences.getString("user", "");
    String pass = preferences.getString("pass", "");
    
    preferences.end();
    
    if (apiKey.length() > 0) {
        strlcpy(cred.api_key, apiKey.c_str(), 50);
        strlcpy(cred.database_url, dbUrl.c_str(), 150);
        strlcpy(cred.username, user.c_str(), 100);
        strlcpy(cred.password, pass.c_str(), 50);
        
        Serial.println("[System] Credentials loaded from Preferences.");
        Serial.printf("  > DB URL: %s\n", cred.database_url);
    } else {
        Serial.println("[System] No credentials found in memory.");
    }
    return cred;
}

void saveCredentials(const char* key, const char* url, const char* user, const char* pass) {
    // URL Validation: Ensure it starts with https://
    String validatedUrl = String(url);
    if (!validatedUrl.startsWith("http")) {
        validatedUrl = "https://" + validatedUrl;
    }
    
    preferences.begin(PREF_NAMESPACE, false);
    preferences.putString("api", key);
    preferences.putString("url", validatedUrl);
    preferences.putString("user", user);
    preferences.putString("pass", pass);
    preferences.end();
    
    Serial.printf("[System] Credentials saved to Preferences. URL: %s\n", validatedUrl.c_str());
}

// --- FreeRTOS Core 1: Hardware Task ---
// Dedicated exclusively to the STC-1000 hardware
void stcTaskCode(void* parameter) {
    for (;;) {
        // Periodic logging check
        if (millis() - lastLogUpdate > LOG_INTERVAL_MS) {
            lastLogUpdate = millis();
            uint32_t epoch = getEpochTime();
            
            if (epoch > 1000000000) { // Valid NTP sync
                if (xSemaphoreTake(stcMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    float temp;
                    if (stc.getTemperature(temp)) {
                        logger.addEntry(epoch, temp, stc.isHeating(), stc.isCooling());
                    }
                    xSemaphoreGive(stcMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Yield core to system
    }
}

// --- FreeRTOS Core 0: Cloud & Network Task ---
// Dedicated to heavy SSL operations and web sockets
void cloudTaskCode(void* parameter) {
    // Enable Watchdog for this task (10-second timeout)
    esp_task_wdt_add(NULL); 

    for (;;) {
        // "Pet the dog" - Reset the watchdog timer
        esp_task_wdt_reset();

        ArduinoOTA.handle();

        if (WiFi.status() == WL_CONNECTED) {
            // Live Status Update
            if (millis() - lastLiveUpdate > LIVE_UPDATE_MS) {
                lastLiveUpdate = millis();
                float temp, sp = 0;
                bool heating = false, cooling = false, ramping = false, success = false;
                int runMode = 0;
                
                // Safely grab the latest data from the STC
                if (xSemaphoreTake(stcMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    if (stc.getTemperature(temp)) {
                        sp = stc.getSetpoint();
                        heating = stc.isHeating();
                        cooling = stc.isCooling();
                        ramping = stc.getRamping();
                        runMode = stc.getRunMode();
                        success = true;
                    }
                    xSemaphoreGive(stcMutex);
                }
                
                if (success) {
                    FirebaseJson json;
                    json.set("temperature", temp);
                    json.set("setpoint", sp);
                    json.set("heating", heating);
                    json.set("cooling", cooling);
                    json.set("ramping", ramping);
                    json.set("runMode", runMode);
                    json.set("epoch", getEpochTime());
                    Firebase.RTDB.setJSON(&fbdo, "STC1000set", &json);
                }
            }

            // Flush offline logs to Firebase
            if (logger.getCount() > 0 && Firebase.ready()) {
                if (xSemaphoreTake(stcMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                    logger.flush(fbdo);
                    xSemaphoreGive(stcMutex);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Small yield prevents network stack starvation
    }
}


void setup() {
    // 1. Immediate DRD Check
    checkDoubleReset();

    Serial.begin(115200);
    Serial.println("\n[System] Starting STC-1000 ESP32 (Dual Core)...");

    // Initialize Mutex
    stcMutex = xSemaphoreCreateMutex();
    
    stc.begin();

    // 2. Load existing credentials
    FirebaseCredentials currentCreds = loadCredentials();

    // 3. WiFi Configuration (Static to ensure they persist during callback)
    static WiFiManagerParameter p_api("api", "Firebase API Key", currentCreds.api_key, 50);
    static WiFiManagerParameter p_url("url", "Firebase URL", currentCreds.database_url, 150);
    static WiFiManagerParameter p_user("user", "Firebase Username", currentCreds.username, 100);
    static WiFiManagerParameter p_pass("pass", "Firebase Password", currentCreds.password, 50);

    wifiManager.addParameter(&p_api);
    wifiManager.addParameter(&p_url);
    wifiManager.addParameter(&p_user);
    wifiManager.addParameter(&p_pass);

    // Disable auto-reboot so we can manually save and log everything
    wifiManager.setBreakAfterConfig(true); 

    // Check for manual config trigger
    pinMode(CONFIG_PIN, INPUT_PULLUP);
    bool manualConfig = (digitalRead(CONFIG_PIN) == LOW);

    if (manualConfig || doubleResetDetected) {
        Serial.println("[System] Config Trigger Detected! Opening Portal...");
        wifiManager.startConfigPortal("STC1000_ESP32_Setup");
    } else {
        wifiManager.autoConnect("STC1000_ESP32_Setup");
        
        // After 10s, clear the RTC flag
        xTaskCreate([](void* p){
            vTaskDelay(pdMS_TO_TICKS(10000));
            rtc_drd_flag = 0;
            Serial.println("[System] Reset window closed.");
            vTaskDelete(NULL);
        }, "DRD_Clear", 2000, NULL, 1, NULL);
    }

    // --- Post-Portal Capture ---
    // This runs after startConfigPortal OR autoConnect if params were entered
    if (strlen(p_api.getValue()) > 0) {
        // Only save if different from what we have to prevent unnecessary flash wear
        if (strcmp(p_api.getValue(), currentCreds.api_key) != 0 || 
            strcmp(p_url.getValue(), currentCreds.database_url) != 0) {
            
            Serial.println("[System] New portal data detected. Saving...");
            saveCredentials(p_api.getValue(), p_url.getValue(), p_user.getValue(), p_pass.getValue());
            
            // Re-load to ensure Firebase gets the new data
            currentCreds = loadCredentials();
        }
    }

    // 4. Native RTC Time Sync
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("[System] Syncing RTC time");
    int retry = 0;
    while (time(nullptr) < 1000000000 && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    Serial.println("\n[System] Time synchronized!");

    // Firebase Setup (Only if we have a URL)
    FirebaseCredentials creds = loadCredentials();
    if (strlen(creds.database_url) > 10) {
        config.api_key = creds.api_key;
        config.database_url = creds.database_url;
        config.token_status_callback = tokenStatusCallback;
        auth.user.email = creds.username;
        auth.user.password = creds.password;

        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);

        if (Firebase.RTDB.beginStream(&streamFbdo, "STC1000get")) {
            Firebase.RTDB.setStreamCallback(&streamFbdo, streamCallback, streamTimeoutCallback);
        }
    } else {
        Serial.println("[System] WARNING: Firebase credentials are empty. Cloud sync disabled.");
    }

    ArduinoOTA.setHostname("STC1000-ESP32");
    ArduinoOTA.begin();

    // Initialize Task Watchdog Timer (TWDT)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000, // 10 seconds
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // Monitor both IDLE tasks
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&twdt_config);

    // Start Dual-Core FreeRTOS Tasks
    xTaskCreatePinnedToCore(
        cloudTaskCode,   // Task function
        "CloudTask",     // Name
        10000,           // Stack size (heavy for SSL)
        NULL,            // Parameters
        1,               // Priority
        &cloudTaskHandle,// Handle
        0                // Pinned to Core 0
    );
    
    xTaskCreatePinnedToCore(
        stcTaskCode,     // Task function
        "STCTask",       // Name
        4000,            // Stack size
        NULL,            // Parameters
        1,               // Priority
        &stcTaskHandle,  // Handle
        1                // Pinned to Core 1
    );

    Serial.println("[System] Initialization Complete. Handing off to FreeRTOS.");
    
    // Delete the setup() / loop() default Arduino task to save RAM
    vTaskDelete(NULL); 
}

void loop() {
    // This loop is never reached because we deleted the task in setup()
}