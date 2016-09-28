# Invervalometer-ATmega328P
Arduino based intervalometer for time lapse photography

Arduino based Intervalometer project.  There are a number of projects on the internets that provide this functionality but I wanted to add settings tor days and time periods in addition to a more traditional time lapse interval.

Original code is for Arduino Uno, but will eventually allow for compiling against other chips.  (Adafruit Feather for example)  

Development environment

- Visual Studio 2015 with Visual Micro (Arduino IDE V 1.6)

Libraries

- RTClib - You need to get the RTCLib library (https://github.com/adafruit/RTClib)

Requirements

- Set interval between shutter/photo capture
- Set length of exposure
- Set number of exposures per capture 
- Allow day of week control - e.g. don't shoot photos on Sunday
- Allow on/off time period - e.g. only shoot between noon and 6pm

Future Functionality

- Configure via wifi or BLE
- Logging events to SD card, wifi or mobile/3G/4G
- Upload photos to some where

Current hardware

- Arduino Uno (includes adafruit pro trinket and metro min)  Support for other hardware not available yet until we make the interrup/timer code platform agnostic
- Nikon 5100

Optional Hardware

- Real time clock (we're going to use DS3231)
- wifi
- bluetooth

Planned hardware support

- Adafruit Feather
There is another repo for this for now.  When that code is working Will abandon that repo and merge into this one.  

Electronics

- It may be possible to use 2N2222 or PN2222 transistors, but I think we will use optocouplers
- Camera remote controller - for Nikon 5100 it is MC-DC2 
