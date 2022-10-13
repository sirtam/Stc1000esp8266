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
unsigned long sendDataPrevMillis = 0; //tracker of time intervall for sending data
bool signupOK = false; //is firebase connected?

//get current temperature on STC
float readTemp() {
  float temp = 0;
  if (stc1000p.readTemperature(&temp)) { 
    return temp;
  }
  return false;
}

//check if power for heating is on
bool readHeating() {
  bool heat;
  if (stc1000p.readHeating(&heat)) {
    return heat;
  }
}

//check if power for cooling is on
bool readCooling() {
  bool cool;
  if (stc1000p.readCooling(&cool)) {
    return cool;
  }
}

//read current setpoint on STC 
int readSetPoint() {
  int sp;
  if (stc1000p.readEeprom(114, &sp)) {
    return sp;
  } else {
    return false;
  }
}

//write new set point
void writeSP(uint16_t spw) {
  if( stc1000p.writeEeprom(114, (spw*10)) ) { //TODO change to accept decimals
      Serial.print("Writing new setpoint: ");
      Serial.println(spw);
  }
  else {
    Serial.println("Failed to write setpoint");
  }
}

void setup() {
  Serial.begin(115200);

  //set up wifi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  //wait for connection
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //connected
  Serial.print("\nWiFi connected with IP: ");
  Serial.println(WiFi.localIP());

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

  //check if temperature setpoint has been altered
  if (Firebase.ready() && (millis() - sendDataPrevMillis > sendIntervalMillis || sendDataPrevMillis == 0)) {
    if(Firebase.RTDB.getInt(&fbdo, "STC1000get/setpoint")) {

      int dbSP = fbdo.to<int>();
      Serial.print("PASSED, SP in db is " + dbSP);
      Serial.println("STC sp: "+ readSetPoint());

      if(dbSP != readSetPoint()) {

        writeSP(dbSP); //writing new set point
      }
    }
    else {
      Serial.println("FAILED ");
      Serial.print("REASON: " + fbdo.errorReason());
    }
    Serial.println();
  }

  //send data to db
  if (Firebase.ready() && (millis() - sendDataPrevMillis > sendIntervalMillis || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    //write temperature data to database
    if (Firebase.RTDB.setFloat(&fbdo, "STC1000set/temperature", readTemp())) {
      Serial.println("PASSED, PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED, REASON: " + fbdo.errorReason());
    }
    
    //write setpoint data to database
    if (Firebase.RTDB.setInt(&fbdo, "STC1000set/setpoint", readSetPoint())) {
      Serial.println("PASSED, PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED, REASON: " + fbdo.errorReason());
    }

    //write bool heating data to database
    if (Firebase.RTDB.setBool(&fbdo, "STC1000set/isheating", readHeating())) {
      Serial.println("PASSED, PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED, REASON: " + fbdo.errorReason());
    }

    //write bool cooling data to database
    if (Firebase.RTDB.setBool(&fbdo, "STC1000set/iscooling", readCooling())) {
      Serial.println("PASSED, PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED, REASON: " + fbdo.errorReason());
    }

    //write time data to database
    timeClient.update(); //update time
    Serial.println(timeClient.getEpochTime());
    if (Firebase.RTDB.setInt(&fbdo, "STC1000set/epochtime", timeClient.getEpochTime())) {
      Serial.println("PASSED, PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED, REASON: " + fbdo.errorReason());
    }
    Serial.println();
  }
}
