#ifndef STC_MANAGER_H
#define STC_MANAGER_H

#include <Stc1000p.h>
#include "Config.h"
class STCManager {
private:
    Stc1000p _stc;
    float _mockTemp = 18.5;
    float _mockSetpoint = 18.0;
    bool _mockHeating = false;
    bool _mockCooling = false;
    unsigned long _lastMockUpdate = 0;

    float fixSign(float val) {
        if (val > 3000.0) return (float)((int16_t)(val * 10)) / 10.0;
        return val;
    }

    void updateMockPhysics() {
        if (millis() - _lastMockUpdate < 5000) return;
        _lastMockUpdate = millis();

        // Simulate temperature drift towards ambient (22.0°C)
        float ambient = 22.0;
        _mockTemp += (ambient - _mockTemp) * 0.01;

        // Simple Thermostat Logic for simulation
        _mockHeating = (_mockTemp < _mockSetpoint - 0.5);
        _mockCooling = (_mockTemp > _mockSetpoint + 0.5);

        if (_mockHeating) _mockTemp += 0.2; // Heating speed
        if (_mockCooling) _mockTemp -= 0.2; // Cooling speed

        // Add some noise
        _mockTemp += (random(-10, 10) / 100.0);
    }

public:
    STCManager() : _stc(STC_PIN, INPUT_PULLDOWN) {}

    void begin() {
        if (MOCK_MODE) {
            Serial.println("[STC] Running in MOCK MODE (Simulation)");
            return;
        }
        pinMode(STC_PIN, INPUT_PULLDOWN);
        _stc.writeRunMode(Stc1000pRunMode::TH);
    }

    bool getTemperature(float &temp) {
        if (MOCK_MODE) {
            updateMockPhysics();
            temp = _mockTemp;
            return true;
        }
        if (_stc.readTemperature(&temp)) {
            temp = fixSign(temp);
            return true;
        }
        return false;
    }

    float getSetpoint() {
        if (MOCK_MODE) return _mockSetpoint;
        float sp;
        return _stc.readSetpoint(&sp) ? fixSign(sp) : 0.0;
    }

    bool isHeating() {
        if (MOCK_MODE) return _mockHeating;
        bool status;
        return _stc.readHeating(&status) ? status : false;
    }

    bool isCooling() {
        if (MOCK_MODE) return _mockCooling;
        bool status;
        return _stc.readCooling(&status) ? status : false;
    }

    void writeSetpoint(float sp) {
        if (MOCK_MODE) {
            _mockSetpoint = sp;
            Serial.printf("[STC-MOCK] New Setpoint received: %.1f\n", sp);
            return;
        }
        if (_stc.writeSetpoint(sp)) {
            Serial.printf("[STC] Setpoint updated to %.1f\n", sp);
        }
    }

    void setRamping(bool enabled) {
        if (MOCK_MODE) return;
        if (_stc.writeRamping(enabled)) {
            Serial.printf("[STC] Ramping (rP) set to: %s\n", enabled ? "ON" : "OFF");
        }
    }

    void setRunMode(int mode) {
        if (MOCK_MODE) return;
        // 0=TH, 1=PR0, 2=PR1...
        if (_stc.writeRunMode((Stc1000pRunMode)mode)) {
            Serial.printf("[STC] Run Mode set to: %d\n", mode);
        }
    }

    void programStep(int profile, int step, float temp, int duration) {
        if (MOCK_MODE) return;
        if (_stc.writeProfileStep(profile, step, temp, duration)) {
            Serial.printf("[STC] Programmed PR%d Step %d: %.1fC for %dh\n", profile, step, temp, duration);
        }
    }

    bool getRamping() {
        if (MOCK_MODE) return false;
        bool ramping;
        return _stc.readRamping(&ramping) ? ramping : false;
    }

    int getRunMode() {
        if (MOCK_MODE) return 0;
        Stc1000pRunMode mode;
        return _stc.readRunMode(&mode) ? (int)mode : 0;
    }

    Stc1000p& getRaw() { return _stc; }
};


#endif
