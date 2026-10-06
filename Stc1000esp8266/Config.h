#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Hardware Pins
const int STC_PIN = D0;

// Intervals
const int DRD_TIMEOUT = 10;
const unsigned long LIVE_UPDATE_MS = 15000;
const unsigned long LOG_INTERVAL_MS = 900000; // 15 mins
const int MAX_LOG_ENTRIES = 20;

// Storage Addresses
const int EEPROM_SIZE = 512;
const int FB_CRED_ADDR = 0;
const int DRD_ADDR = 350;

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

#endif
