# Stc1000esp8266

The project goal is to make a STC1000 configurable from a server so I can control the fermentation from wherever I am.

The STC1000 is flashed with the excellent firmware from https://github.com/matsstaff/stc1000p

The communication protocol is taken from https://github.com/fredericsureau/arduino-stc1000p

The code is also inspired by https://github.com/vieuxsinge/stc1000esp, but I never managed to get telnet to work so now I'm making my own project

Some notes:
STC1000+ is flashed with the picprog_com script. This is needed to have the half duplex communication between ESP and STC

Firebase:
https://randomnerdtutorials.com/esp32-firebase-realtime-database/#create-firebase-project

Libraries:
https://github.com/fredericsureau/arduino-stc1000p
https://www.arduino.cc/reference/en/libraries/firebase-arduino-client-library-for-esp8266-and-esp32/

Pin configuration:

STC1000 | NodeMCU
------- | -------
ICSPCLK | D0
ICSPDAT | Unused
GND     | GND
5V      | VIN
mCLR    | Unused
