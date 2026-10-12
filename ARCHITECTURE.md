# STC-1000 ESP32 Architecture

This document outlines the professional firmware architecture and system flow for the STC-1000 ESP32 IoT controller.

## 1. System Topology
The system uses a Star Topology with Firebase as the central synchronization hub.

```mermaid
graph LR
    STC[STC-1000 Hardware] -- GPIO 14 / One-Wire --- ESP32[ESP32 Firmware]
    ESP32 -- WiFi / TLS 1.3 --- FB[(Firebase RTDB)]
    FB -- WebSockets --- Dash[Web Dashboard]
    User((User)) -- Auth --- Dash
```

## 2. Firmware Class Diagram
The firmware is built using a modular, encapsulated approach to separate hardware concerns from cloud logic.

```mermaid
classDiagram
    class Config {
        <<struct>>
        +FirebaseCredentials creds
        +LogEntry buffer
        +int STC_PIN
        +int LOG_INTERVAL
    }

    class STCManager {
        -Stc1000p _stc
        -fixSign(float val) float
        +begin()
        +getTemperature(float &t) bool
        +getSetpoint() float
        +writeSetpoint(float sp)
        +isHeating() bool
        +isCooling() bool
    }

    class DataLogger {
        -LogEntry _buffer[50]
        -int _count
        +addEntry(epoch, temp, heat, cool)
        +flush(FirebaseData &fbdo) bool
        +getCount() int
    }

    class Main_Orchestrator {
        -stcMutex Semaphore
        +setup()
        +loop()
        +cloudTaskCode()
        +stcTaskCode()
        +streamCallback()
    }

    Main_Orchestrator --> STCManager : "Controls Hardware"
    Main_Orchestrator --> DataLogger : "Logs Telemetry"
    Main_Orchestrator --> Config : "Shares Constants"
    STCManager --> Config : "Uses Pins"
    DataLogger --> Config : "Uses Structs"
```

## 3. Real-Time Command Sequence
This illustrates how a setpoint change on your phone reaches the hardware in milliseconds via the Firebase Stream.

```mermaid
sequenceDiagram
    participant User as Web Dashboard
    participant FB as Firebase RTDB
    participant C0 as ESP32 Core 0 (CloudTask)
    participant Mutex as Mutex (Traffic Light)
    participant C1 as ESP32 Core 1 (STCTask)
    participant HW as STC-1000 Hardware

    User->>FB: set("/setpoint", 18.5)
    FB-->>C0: [Push Notification] Stream Data
    C0->>Mutex: xSemaphoreTake()
    activate Mutex
    C0->>HW: _stc.writeSetpoint(18.5)
    HW-->>C0: Success Ack
    C0->>Mutex: xSemaphoreGive()
    deactivate Mutex
    
    Note over C1, HW: Core 1 continues reading temp independently
    C1->>Mutex: xSemaphoreTake()
    activate Mutex
    C1->>HW: _stc.readTemperature()
    HW-->>C1: 18.6°C
    C1->>Mutex: xSemaphoreGive()
    deactivate Mutex
```

## 4. Device State Machine
The lifecycle of the device from power-on to steady-state operation.

```mermaid
stateDiagram-v2
    [*] --> PowerOn
    PowerOn --> CheckReset: Load Preferences
    
    CheckReset --> ConfigPortal: Double-Reset Detected
    ConfigPortal --> PowerOn: Save & Restart
    
    CheckReset --> WiFiConnect: Single Reset / Normal Boot
    WiFiConnect --> TimeSync: WiFi OK
    TimeSync --> InitTasks: RTC Calibrated
    
    state InitTasks {
        [*] --> SpawnCloudTask
        [*] --> SpawnSTCTask
    }
    
    InitTasks --> Running
    
    state Running {
        [*] --> Idle
        Idle --> UpdateFirebase: Every 15s
        Idle --> BufferData: Every 15m
        Idle --> Reconnect: WiFi Lost
        Reconnect --> Idle: WiFi Restored
    }
    
    Running --> ConfigPortal: Reset Button x2
```
