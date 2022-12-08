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

#define ESP_DRD_USE_EEPROM true
#define DOUBLERESETDETECTOR_DEBUG true

#include <ESP_DoubleResetDetector.h>

//number of seconds to consider double reset
#define DRD_TIMEOUT 10

//EEPROM Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 350

//EEPROM Memory Address for Firebase credentials
#define FB_ADDRESS 0

#include <Stc1000p.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h" //Provide the RTDB payload printing info and other helper functions.

//use D0 pin since this has a built in pulldown resistor
Stc1000p stc1000p(D0, INPUT_PULLDOWN_16);

//responsible for connecting to a WiFi
WiFiManager wifiManager;
WiFiClient client;

//define firebase data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//values for setting Firebase credentials
char api_key[50] = {0};
char database_url[150] = {0};
char username[100] = {0};
char password[50] = {0};

//double reset detector
DoubleResetDetector* drd;

//NTP server to request epoch time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

int sendIntervalMillis = 15000; //how often data is sent to db
int getIntervalMillis = 10000; //how often data is requested from db

unsigned long sendDataPrevMillis = 0; //tracker of time intervall for sending data
unsigned long getDataPrevMillis = 0; //tracker of time intervall for requesting data

int connectRetry = 5; //number of reconnects before setting up new configuration
int wifiRetries = 0; //WiFi counter
int fbRetries = 0; //Firebase counter

bool signupOK = false; //is firebase connected?

//making handleling Firebase credentials easier
struct FirebaseCredentials {
  char api_key[50];
  char database_url[150];
  char username[100];
  char password[50];    
};

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

FirebaseCredentials getCredentials() {
  FirebaseCredentials cred;
  EEPROM.get(FB_ADDRESS, cred);
  return cred;
}

void setup() {

  Serial.begin(115200);

  //set STC to profile 0
  stc1000p.writeRunMode(Stc1000pRunMode::TH);

  //congigure Firebase input fields in config
  WiFiManagerParameter fb_api_key("fb_api_key", "Firebase API key", api_key, 50);
  WiFiManagerParameter fb_database_url("fb_database_url", "Firebase URL", database_url, 150);
  WiFiManagerParameter fb_user("fb_user","Firebase username", username, 100);
  WiFiManagerParameter fb_pass("fb_pass","Firebase password", password, 50);

  //add parameters
  wifiManager.addParameter(&fb_api_key);
  wifiManager.addParameter(&fb_database_url);
  wifiManager.addParameter(&fb_user);
  wifiManager.addParameter(&fb_pass);

  drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);
  wifiManager.setConfigPortalTimeout(600);

  //set up "AutoConnectAP" and handles WiFi SSID and PASS
  if (drd->detectDoubleReset()) 
  {
    //start portal after double reset
    wifiManager.startConfigPortal("STC1000_setup");

    //copy Firebase credentials from config to global variables
    strcpy(api_key, fb_api_key.getValue());
    strcpy(database_url, fb_database_url.getValue());
    strcpy(username, fb_user.getValue());
    strcpy(password, fb_pass.getValue());
    
    //do not change Firebase credentials if empty
    if(strlen(api_key) != 0 && strlen(database_url) != 0 && strlen(username) != 0 && strlen(password) != 0) {
      FirebaseCredentials cred;
      strcpy(cred.api_key, api_key);
      strcpy(cred.database_url, database_url);
      strcpy(cred.username, username);
      strcpy(cred.password, password);
    
      //store Firebase credentials in EEPROM
      EEPROM.begin(350);
      EEPROM.put(FB_ADDRESS, cred);
      EEPROM.end();
      Serial.println("EEPROM written");

      //for some reason ESP needs to be reset to get input correct. FIX THIS
      delay(5000);
      ESP.restart();
    }
    else {
      Serial.println("Do not write to EEPROM");
    }
  } 
  else 
  {
    //start autoconnect
    wifiManager.autoConnect("STC1000_setup");
  }

  //get Firebase credentials from EEPROM  
  FirebaseCredentials fb_credentials = getCredentials();

  //set firebase credentials
  config.api_key = fb_credentials.api_key;
  config.database_url = fb_credentials.database_url;

  //callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  //assign user credentials
  auth.user.email = fb_credentials.username;
  auth.user.password = fb_credentials.password;

  //start connection
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  //start epoch
  timeClient.begin();
}

void loop() {
  
  drd->loop(); //check double reset consideration timer

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnect WiFi");

    if(wifiManager.getWiFiIsSaved()) {
      wifiManager.setEnableConfigPortal(false);
    }

    wifiManager.autoConnect("STC1000_setup");
    delay(5000); //wait 5 seconds before retry
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
      fbRetries++;
      delay(5000); //wait 5 seconds before retry

      if(fbRetries >= connectRetry) {
        ESP.restart();
      }
    }
  }
}
