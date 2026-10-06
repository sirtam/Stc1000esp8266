#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "Config.h"
#include <Firebase_ESP_Client.h>

class DataLogger {
private:
    LogEntry _buffer[MAX_LOG_ENTRIES];
    int _count = 0;

public:
    void addEntry(uint32_t epoch, float temp, bool heating, bool cooling) {
        if (epoch == 0) return;

        int index = _count % MAX_LOG_ENTRIES;
        _buffer[index] = {epoch, temp, heating, cooling};
        
        if (_count < MAX_LOG_ENTRIES) _count++;
        Serial.printf("[Logger] Buffered point at %u. Count: %d\n", epoch, _count);
    }

    bool flush(FirebaseData &fbdo) {
        if (_count == 0 || !Firebase.ready()) return false;

        Serial.println("[Logger] Flushing logs to Firebase...");
        for (int i = 0; i < _count; i++) {
            FirebaseJson json;
            json.set("temperature", _buffer[i].temp);
            json.set("heating", _buffer[i].heating);
            json.set("cooling", _buffer[i].cooling);

            String path = "STC1000logs/" + String(_buffer[i].epoch);
            if (!Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
                Serial.printf("[Logger] Flush failed: %s\n", fbdo.errorReason().c_str());
                return false;
            }
        }

        Serial.println("[Logger] Sync complete.");
        _count = 0;
        return true;
    }

    int getCount() { return _count; }
};

#endif
