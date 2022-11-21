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

#include "config.h"

#include <Stc1000p.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h" //Provide the RTDB payload printing info and other helper functions.
//#include <HTTPClient.h>

//use D0 pin since this has a built in pulldown resistor
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

//connect to WiFi
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

//get current runmode TODO: can't return bool
String readRunmode() {
  Stc1000pRunMode runMode;
  bool success = stc1000p.readRunMode(&runMode);

  switch(runMode) {
    case Stc1000pRunMode::PR0:
      return "PR0";
      break;
    case Stc1000pRunMode::PR1:
     return "PR1";
      break;
    case Stc1000pRunMode::PR2:
      return "PR2";
      break;
    case Stc1000pRunMode::PR3:
      return "PR3";
      break;
    case Stc1000pRunMode::PR4:
      return "PR4";
      break;
    case Stc1000pRunMode::PR5:
      return "PR5";
      break;
    case Stc1000pRunMode::TH:
      return "TH";
      break;
    default:
      return "None";
  }
}

//get current temperature on STC
float readTemp() {
  float temp;
  return (stc1000p.readTemperature(&temp)) ? temp : false;  
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
float readSetPoint() {
  float sp;
  return (stc1000p.readSetpoint(&sp)) ? sp : false;
  //return (stc1000p.readEeprom(114, &sp)) ? sp : false;
}

//read current hysteris
float readHysteresis() {
  float hysteresis;
  return (stc1000p.readHysteresis(&hysteresis)) ? hysteresis : false;
}

//read current temp correction
float readTempCorrection() {
  float tempCorrection;
  return (stc1000p.readTemperatureCorrection(&tempCorrection)) ? tempCorrection : false;
}

//read current setpoint alarm
float readSetpointAlarm() {
  float setpointAlarm;
  return (stc1000p.readSetpointAlarm(&setpointAlarm)) ? setpointAlarm : false;
}

//read current step
int readCurrentStep() {
  int currentStep;
  return (stc1000p.readCurrentStep(&currentStep)) ? currentStep : false;
}

//read current duration
int readCurrentDuration() {
  int currentDuration;
  return (stc1000p.readCurrentDuration(&currentDuration)) ? currentDuration : false;
}

//read cooling delay
int readCoolingDelay() {
  int coolingDelay;
  return (stc1000p.readCoolingDelay(&coolingDelay)) ? coolingDelay : false;
}

//read heating delay
int readHeatingDelay() {
  int heatingDelay;
  return (stc1000p.readHeatingDelay(&heatingDelay)) ? heatingDelay : false;
}

//read current ramping
bool readRamping() {
  bool ramping;
  return (stc1000p.readRamping(&ramping)) ? ramping : false;
}


//write new setpoint
void writeSetPoint(float sp) {
  (stc1000p.writeSetpoint(sp)) 
  ? Serial.print("Writing new setpoint: "), Serial.println(sp, 1)
  : Serial.println("Failed to write setpoint");
}

//write hysteris value
void writeHysteris(float hysteresis) {
  (stc1000p.writeHysteresis(hysteresis)) 
  ? Serial.print("Writing new hysteresis: "), Serial.println(hysteresis, 1)
  : Serial.println("Failed to write hysteresis");
}

//write temperature correction
void writeTempCorrection(float tempCorrection) {
  (stc1000p.writeTemperatureCorrection(tempCorrection)) 
  ? Serial.print("Writing new tempCorrection: "), Serial.println(tempCorrection, 1)
  : Serial.println("Failed to write tempCorrection");  
}

//write setpoint alarm
void writeSetpointAlarm(float setpointAlarm) {
  (stc1000p.writeSetpointAlarm(setpointAlarm)) 
  ? Serial.print("Writing new setpointAlarm: "), Serial.println(setpointAlarm, 1)
  : Serial.println("Failed to write setpointAlarm");  
}

//write cooling delay
void writeCoolingDelay(int coolingDelay) {
  (stc1000p.writeCoolingDelay(coolingDelay)) 
  ? Serial.print("Writing new coolingDelay: "), Serial.println(coolingDelay, 1)
  : Serial.println("Failed to write coolingDelay");
}

//write heating delay
void writeHeatingDelay(int heatingDelay) {
  (stc1000p.writeHeatingDelay(heatingDelay)) 
  ? Serial.print("Writing new heatingDelay: "), Serial.println(heatingDelay, 1)
  : Serial.println("Failed to write heatingDelay");
}

//write ramping
void writeRamping(bool ramping) {
  (stc1000p.writeRamping(ramping)) 
  ? Serial.print("Writing ramping: "), Serial.println(ramping)
  : Serial.println("Failed to write ramping");
}

//this function needs to be rewritten to be more effective
void readFromDatabase() {
  bool sendToDB = false;
  
  if(Firebase.RTDB.getInt(&fbdo, "STC1000get/setpoint")) {
    float dbSP = fbdo.to<float>(); //sp from database

    if(dbSP != readSetPoint()) {
        Serial.println("Setting setpoint");
        writeSetPoint(dbSP); //writing new set point
        sendToDB = true;
    }
  }

  if(Firebase.RTDB.getInt(&fbdo, "STC1000get/coolingDelay")) {
    float dbCD = fbdo.to<float>(); //sp from database

    if(dbCD != readCoolingDelay()) {
        Serial.println("Setting coolingDelay");
        writeCoolingDelay(dbCD); //writing new cooling delay
        sendToDB = true;
    }
  }

  if(Firebase.RTDB.getInt(&fbdo, "STC1000get/heatingDelay")) {
    float dbHD = fbdo.to<float>(); //sp from database

    if(dbHD != readHeatingDelay()) {
        Serial.println("Setting heatingDelay");
        writeHeatingDelay(dbHD); //writing new heating delay
        sendToDB = true;
    }
  }

  if(Firebase.RTDB.getInt(&fbdo, "STC1000get/tempCorrection")) {
    float dbTC = fbdo.to<float>(); //sp from database

    if(dbTC != readTempCorrection()) {
        Serial.println("Setting tempCorrection");
        writeTempCorrection(dbTC); //writing new temperature correction
        sendToDB = true;
    }
  }

  if(sendToDB) {
    writeToDatabase(); //force new send to db
  }
}

void writeToDatabase() {
  FirebaseJson json;
  timeClient.update(); //update time

  int epochtime = timeClient.getEpochTime();
  float temp = readTemp();

  //only send data if temp and epoch are read
  if (temp && epochtime) {
    json.set("epochtime", epochtime);
    json.set("temperature", temp);
    json.set("setpoint", readSetPoint());
    json.set("isheating", readHeating());
    json.set("iscooling", readCooling());
    json.set("hysteresis", readHysteresis());
    json.set("tempCorrection", readTempCorrection());
    json.set("setpointAlarm", readSetpointAlarm());
    json.set("currentStep", readCurrentStep());
    json.set("currentDuration", readCurrentDuration());
    json.set("coolingDelay", readCoolingDelay());
    json.set("heatingDelay", readHeatingDelay());
    json.set("ramping", readRamping());
    json.set("runMode", readRunmode());

    Serial.printf("Sending jSON: %s\n", Firebase.RTDB.setJSON(&fbdo, "STC1000set", &json) ? "ok" : fbdo.errorReason());
  }
  else {
    //json.toString(Serial, true);
    Serial.println("-- Missing data. Do not send to Firebase --");
  }
}

void setup() {

  Serial.begin(115200);

  //set STC to profile 0
  stc1000p.writeRunMode(Stc1000pRunMode::TH);

  wifiConnetion();

  //firebase credentials
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  //callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  //assign user credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  //start connection
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //start epoch
  timeClient.begin();
}

void loop() {

  //Serial.print("WiFi status ");
  //Serial.println(WiFi.status());
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnect WiFi");
    wifiConnetion();
  }
  else {
    if(Firebase.ready()) {
    //check if temperature setpoint has been altered
      if (millis() - getDataPrevMillis > getIntervalMillis || getDataPrevMillis == 0) {
        getDataPrevMillis = millis();
        readFromDatabase();
      }
      //send data to db
      if (millis() - sendDataPrevMillis > sendIntervalMillis || sendDataPrevMillis == 0) {
        sendDataPrevMillis = millis();
        writeToDatabase();
      }
    }
    else {
      Serial.println("-- Not able to connect to Firebase --");
    }
  }
}
