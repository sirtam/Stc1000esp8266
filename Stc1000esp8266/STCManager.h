#ifndef STC_MANAGER_H
#define STC_MANAGER_H

#include <Stc1000p.h>
#include "Config.h"

class STCManager {
private:
    Stc1000p _stc;

    float fixSign(float val) {
        if (val > 3000.0) {
            return (float)((int16_t)(val * 10)) / 10.0;
        }
        return val;
    }

public:
    STCManager() : _stc(STC_PIN, INPUT_PULLDOWN_16) {}

    void begin() {
        _stc.writeRunMode(Stc1000pRunMode::TH);
    }

    bool getTemperature(float &temp) {
        if (_stc.readTemperature(&temp)) {
            temp = fixSign(temp);
            return true;
        }
        return false;
    }

    float getSetpoint() {
        float sp;
        return _stc.readSetpoint(&sp) ? fixSign(sp) : 0.0;
    }

    bool isHeating() {
        bool status;
        return _stc.readHeating(&status) ? status : false;
    }

    bool isCooling() {
        bool status;
        return _stc.readCooling(&status) ? status : false;
    }

    void writeSetpoint(float sp) {
        if (_stc.writeSetpoint(sp)) {
            Serial.printf("[STC] Setpoint updated to %.1f\n", sp);
        }
    }

    // Add other read/write proxies as needed...
    Stc1000p& getRaw() { return _stc; }
};

#endif
