#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Pins
const int STC_PIN = 14; // GPIO 14 (Stable with internal pull-down)
const bool MOCK_MODE = true; // Set to false when connecting real hardware!


// Intervals
const int DRD_TIMEOUT = 10;
const int DRD_ADDR = 350;
const unsigned long LIVE_UPDATE_MS = 15000;
const unsigned long LOG_INTERVAL_MS = 900000; // 15 mins
const int MAX_LOG_ENTRIES = 50; // ESP32 has more RAM, so we can buffer more!

// Storage Key-Value Names (Preferences)
const char* PREF_NAMESPACE = "stc-config";

// Structures
struct FirebaseCredentials {
  char api_key[50];
  char database_url[150];
  char username[100];
  char password[50];    
};

struct LogEntry {
  uint32_t epoch;
  float temp;
  bool heating;
  bool cooling;
};

struct ProfileStep {
  float temp;
  uint16_t duration; // in hours
};

#endif
