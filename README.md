# Invervalometer
Arduino based intervalometer for time lapse photography

Arduino based Intervalometer project.  There are a number of projects on the internets that provide this functionality but I wanted to add settings tor days and time periods in addition to a more traditional time lapse interval.

Original code is for Arduino Uno, but will eventually allow for compiling against other chips.  (Adafruit Feather for example)  

Development environment

- Visual Studio 2015 with Visual Micro (Arduino IDE V 1.6)

Libraries

- DS3232RTC       http://github.com/JChristensen/DS3232RTC
- TimeLib         http://www.arduino.cc/playground/Code/Time

Requirements

- Set interval between shutter/photo capture
- Set length of exposure
- Set number of exposures per capture 
- Allow day of week control - e.g. don't shoot photos on Sunday
- Allow on/off time period - e.g. only shoot between noon and 6pm

Current hardware

- ATmega328P/Arduino Uno (includes adafruit pro trinket and metro mini)  
- Real time clock DS3231  (https://learn.adafruit.com/adafruit-ds3231-precision-rtc-breakout/overview)
- Nikon 5100

Future Functionality

- Configure via wifi or BLE
- Logging events to SD card, wifi or mobile/3G/4G
- Upload photos to some where
- support other boards/processors and RTCs

Electronics

- It may be possible to use 2N2222 or PN2222 transistors, but I think we will use optocouplers
- Camera remote controller - for Nikon 5100 it is MC-DC2 

Configuration

To set up the code to compile and use the right settings for your board you need to edit the lib_customizaation.h file to set the #defines macros.

You can control a led indicator with 
<pre>#define _FLASH_LED_ON_TRIGGER</pre>


The first time you compile/run, set this flag to truw
  bool gForceClockSet = true;  
 Once the clock is set then change that flag to false.  

