#include "config.h"

#include <Stc1000p.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h" //Provide the RTDB payload printing info and other helper functions.
//#include <HTTPClient.h>

/*
* Copyright 2022 Thomas André Transeth
*
* Written for ESP8266
*
* Write license and acknowledgements
*
* All personal credentials are stored in config.h. Please use the sample file in project to add personal configuration.
*
* This code uses only the first profile in the STC.
* TODO make use of full profile for temperature schema
* This code only set the set point so you have to manually set all temperatures 
* TODO include more profiles
*/

//use D0 pin since this has a build in pulldown resistor
Stc1000p stc1000p(D0, INPUT_PULLDOWN_16);

//define firebase data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//NTP server to request epoch time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

int sendIntervalMillis = 15000; //how often data is sent to db
int getIntervalMillis = 10000; //how often data is requested from db

unsigned long sendDataPrevMillis = 0; //tracker of time intervall for sending data
unsigned long getDataPrevMillis = 0; //tracker of time intervall for requesting data

bool signupOK = false; //is firebase connected?


//get current temperature on STC
float readTemp() {
  float temp;
  return (stc1000p.readTemperature(&temp)) ? temp :  false;  
}

//check if power for heating is on
bool readHeating() {
  bool heat;
  return (stc1000p.readHeating(&heat)) ? heat : false;
}

//check if power for cooling is on
bool readCooling() {
  bool cool;
  return (stc1000p.readCooling(&cool)) ? cool : false;
}

//read current setpoint on STC 
int readSetPoint() {
  int sp;
  return (stc1000p.readEeprom(114, &sp)) ? sp : false;
}

//write new set point
void writeSP(uint16_t spw) {
  //TODO change to accept decimals
  (stc1000p.writeEeprom(114, (spw*10)) ) 
  ? Serial.print("Writing new setpoint: " + spw) 
  : Serial.println("Failed to write setpoint");
}

void wifiConnetion() {
  //set up wifi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  //wait for connection
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    int resetTime = millis();
    delay(500);
    Serial.print(".");
    if(resetTime > millis() - 300000) {
      ESP.reset();
    }
  }

  //connected
  Serial.print("\nWiFi connected with IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  wifiConnetion();

  //firebase credentials
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  //callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  //assign user credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //start epoch
  timeClient.begin();
}
void loop() {

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnection WiFi");
    wifiConnetion();
  }
  else {

    if(Firebase.ready()) {

    //check if temperature setpoint has been altered
      if (millis() - getDataPrevMillis > getIntervalMillis || getDataPrevMillis == 0) {
        if(Firebase.RTDB.getInt(&fbdo, "STC1000get/setpoint")) {
          
          getDataPrevMillis = millis();

          int dbSP = fbdo.to<int>();
          int stcSP = readSetPoint();

          if(stcSP && dbSP != readSetPoint()) {
            writeSP(dbSP); //writing new set point
          }
        }
      }

      //send data to db
      if (millis() - sendDataPrevMillis > sendIntervalMillis || sendDataPrevMillis == 0) {
        
        sendDataPrevMillis = millis();
        FirebaseJson json;

        timeClient.update(); //update time
        int epochtime = timeClient.getEpochTime();
        float temp = readTemp();
        int setpoint = readSetPoint();
        bool heating = readHeating();
        bool cooling = readCooling();

        //only send data if temp and epoch are read
        if (temp && epochtime) {
          json.set("STC1000set/epochtime", epochtime);
          json.set("STC1000set/temperature", temp);
          json.set("STC1000set/setpoint", setpoint);
          json.set("STC1000set/isheating", heating);
          json.set("STC1000set/iscooling", cooling);

          json.toString(Serial, true);
          Serial.printf("Sending jSON: %s\n", Firebase.RTDB.setJSON(&fbdo, "STC1000set", &json) ? "ok" : fbdo.errorReason());
        }
        else {
          Serial.println("-- Missing data. Do not send to Firebase --");
        }
      }
    }
    else {
      Serial.println("-- Not able to connect to Firebase --");
    }
  }
}
