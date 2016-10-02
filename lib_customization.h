//#pragma once

// put the #defines/macros in here
// set the real time clock, anything else
#define _DEBUG
#define _FLASH_LED_ON_TRIGGER


#define _RTC_DS3231_	
//#define _RTC_MILLIS

// there might be many others - but for now I am only testing with trinket
// no Serial defined for trinket
#ifdef ARDUINO_AVR_TRINKET3  
#define _NO_SERIAL
#endif

/*#ifdef ARDUINO_AVR_FEATHER32U4
#define _NO_SERIAL
#endif
*/