#include <Stc1000p.h>

/*
* Copyright 2022 Thomas André Transeth
*
* Written for ESP8266
*
* Write license and acknowledgements
*/

//use D0 pin since this has a build in pulldown resistor
Stc1000p stc1000p(D0, INPUT_PULLDOWN_16);

float readTemp() {
  float temp = 0;
  if( stc1000p.readTemperature(&temp) ) {
    return temp;
  }
  return false;
}

bool readHeating() {
 bool heat;
  if( stc1000p.readHeating(&heat) ) {
    return heat;
  }
}

bool readCooling() {
  bool cool;
  if( stc1000p.readCooling(&cool) ) {
    return cool;
  }
}

int readSetPoint() {
  int sp;
  if( stc1000p.readEeprom(114, &sp) ) {
    return sp;
  }
  else {
    return false;
  }
}

void readData() {

  float temp = readTemp();
  if(temp) {
    Serial.print("Temperature: ");
    Serial.println(temp);
  }
  else {
    Serial.println("Failed to read temperature");
  }

  bool heat = readHeating();
  Serial.print("Heating: ");
  Serial.println(heat);

  bool cool = readCooling();
  Serial.print("Cooling: ");
  Serial.println(cool);


  int sp = readSetPoint();
  if(sp) {
    Serial.print("Setpoint: ");
    Serial.println((float)sp/10);
  }
  else {
    Serial.println("Failed to read setpoint");
  }
}

void writeData(uint16_t spw) {
  if( stc1000p.writeEeprom(114, (spw*10)) ) { //TODO change to accept decimals
      Serial.print("Writing new setpoint: ");
      Serial.println(spw);
  }
  else {
    Serial.println("Failed to write setpoint");
  }
}


void setup() {
  Serial.begin(9600);
}

void loop() {
  Serial.println("\n--- Concept STC1000+ communication ---");
  Serial.println("Use the following commands:");
  Serial.println("r (read all data)");
  Serial.println("s (write setpoint with random number)");
  Serial.println("\nWaiting for input...");

  while(Serial.available() == 0) { }  //do nothing
  //String input = Serial.readString(); 
  char input = Serial.read();
  
  switch(input) {
    case 'r':
      Serial.println("-- Reading all data from STC --");
      readData();
    break;
    case 's':
      writeData(random(100));
    default: //TODO this is triggered every rerun, find a way to avoid this
      Serial.print(input);
      Serial.println(" is not a valid command");
  }
  delay(50); //making ready for new run
}
