#include <Stc1000p.h>

// For NodeMCU
Stc1000p stc1000p(D0, INPUT_PULLDOWN_16);
uint16_t spw = 0;



void setup() {
}

void loop() {
  Serial.begin(115200);
  
//  float temp;
//  if( stc1000p.readTemperature(&temp) ) {
//    Serial.print("Temperature: ");
//    Serial.println(temp);
//  }
//  else {v
//    Serial.println("Failed to read temperature");
//  }
  
  int sp;
  if( stc1000p.readEeprom(114, &sp) ) {
    Serial.print("Setpoint: ");
    Serial.println((float)sp/10);
  }
  else {
    Serial.println("Failed to read setpoint");
  }

//  bool heat;
//  if( stc1000p.readHeating(&heat) ) {
//    Serial.print("Heating: ");
//    Serial.println(heat);
//  }
//  else {
//    Serial.println("Failed to read heating");
//  }

//  bool cool;
//  if( stc1000p.readCooling(&cool) ) {
//    Serial.print("Cooling: ");
//    Serial.println(cool);
//  }
//  else {
//    Serial.println("Failed to read cooling");
//  }

//  Serial.println("Ready to read");
//  delay(3000);
//  if (Serial.available()) {
//    
//    Serial.println(Serial.read());
//  }
//  else {
//    Serial.println("Nothing to read");
//  }
//  
//  Serial.println("Done reading");


  if( stc1000p.writeEeprom(114, spw) ) {
    //Serial.print("Setpoint: ");
    //Serial.println((float)sp/10);
    spw = spw +1;
  }
  else {
    Serial.println("Failed to write setpoint");
  }
  
  delay(100);
  

}