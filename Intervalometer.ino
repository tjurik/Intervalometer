//#include <Time.h>
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)
#include "boards.h"				// identifies the chip/board based on IDE's setting (we use it for logging)
#include"lib_customization.h"	// edit this file to define the real time clock and any other functionality-controlling parameters
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <TimeLib.h>         //http://www.arduino.cc/playground/Code/Time  

/*
Configuration:
--------------
- delay - before it starts running (optional?)
- interval (one second to hours)
- duration of shutter
- number of photos to take
- days of the week to take photos
- Valid time during day (thee is only one of these - though this could get more complex if we wished)
*/

//////////////////////////////////////////////////////////////
// GLOBALS
//
// edit these values to set the interval for taking photos
//////////////////////////////////////////////////////////////

const int	SHUTTER_TIME_MS			= 200;	// (in milliseconds) I have no idea what this means.  what is the resolution of this? 
const int	INTERVAL_SECS		    = 20;		// number of seconds to wait between photos  - for 1 minute use 60, one hour use 3600, etc
const int	NUMBER_OF_EXPOSURES     = 1;		// number of photos to take for each 'loop'/command to take a photo
const int	KEEP_ALIVE_PERIOD_SECS  = 1500;		// In seconds  - set to 0 if we want no keep alive  (25 minutes * 60 sec)
const int	FOCUS_LEAD_TIME_MS      = 2;    // in milliseconds  - 

bool	VALID_DAYS[] = {			// true for yes, take photo, false for no - don't take photo
	true,	// sunday
	true,	// monday
	true,	// tuesday
	true,	// wednesday
	true,	// thursday
	true,	// friday
	true	// satudrday
};

// Set to all 0 if always take photo
// Otherwise set the beginning hour and minute to be before the stop hour and minute
int START_HOUR		= 0;	// 0 to 23
int START_MINUTE	= 2;	// 0 to 59
int STOP_HOUR		= 23;	// 0 to 23
int STOP_MINUTE		= 58;	// 0 to 59

// set these as desired to control the shutter and focus
const int focusPin			= 4;
const int shutterPin		= 5;
const int rtcTimerIntPin	= 3;	// this is not working for some reason

bool gForceClockSet = false;  // = false  - forces setting the clock when compiling.  Do this for initial clock/board set up.
//////////////////////////////////////////////////////////////
// End of user editable settings
//////////////////////////////////////////////////////////////

volatile bool triggerPhoto = false;
volatile bool keepAlive = false;

int start_time = 0;
int stop_time = 0;
volatile int interval_counter = 0;
volatile int keep_alive_counter = 0;

// set the type of clock - the default is no RTC - but set the macro for the one you are using.
#ifdef _RTC_DS3231
DS3232RTC rtc;
#elif _RTC_DS1307
RTC_DS1307 rtc;
#else
RTC_Millis rtc;  // this is pretty useless - deprecating the functionaliy if we don't have a rtc
#endif

// just for logging
char daysOfTheWeek[7][12] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void setup()
{
	setupRTClock();	
	setupCameraPins();
	setupIntervalometerSettings();
	logEvent("Started");	
	logSettings();
}

void loop()
{
	// Since I could not get the 1hz square wave to work, we cheat in the loop and check the time.  
	// If we're not doing much work it's no big deal, but it would be better to have the ISR/trigger...
	static unsigned long previousTime = 0;	
	time_t timeNow = RTC.get();	 

	if (timeNow != previousTime) {
		commonTimerFunction();
		previousTime = timeNow;             // remember previous time		
	}

	if (triggerPhoto)
	{    
    keepAlive = false;
    triggerPhoto = false;
    keep_alive_counter = 0;
		
		for (int i = 0; i < NUMBER_OF_EXPOSURES; i++)
		{
			exposure();			
		}				
	}
	else if (keepAlive)
	{
		keepAlive = false;
		pingCamera();
	}	
}

bool CheckIfWeShouldTakePhoto()
{		
	time_t theTime = RTC.get();
	int dayOfWeek = weekday(theTime) - 1;
	int theHour = hour(theTime);
	int theMinute = minute(theTime);

	int current_time = (60 * theHour) + theMinute;

	// check the day - return false if no
	if (!VALID_DAYS[dayOfWeek]) {
		return false;
	}
		
	// now check if we are in the 'active' time slot
	if (start_time != 0 || stop_time != 0)
	{
		if (start_time > current_time || current_time >= stop_time)
		{
			return false;
		}
	}	

	return true;
}

void commonTimerFunction()
{
	interval_counter++;
	keep_alive_counter++;
	if (interval_counter % INTERVAL_SECS == 0) {
		// reset counter to 0 if we hit our interval % (to make division take less time?)
		interval_counter = 0;
		triggerPhoto = CheckIfWeShouldTakePhoto();
	}	

	// if we don't trigger photo we may need to keep alive the camera
	if (!triggerPhoto && KEEP_ALIVE_PERIOD_SECS != 0)
	{
		if (keep_alive_counter % KEEP_ALIVE_PERIOD_SECS == 0) {
			keepAlive = true;
			keep_alive_counter = 0;
		}		
	}		
}

void pingCamera()
{
   const int pingdelay = 90;
	digitalWrite(focusPin, HIGH);
#ifdef _FLASH_LED_ON_TRIGGER 	
	digitalWrite(LED_BUILTIN, HIGH);
#endif	
  delay(pingdelay);
	digitalWrite(focusPin, LOW);
#ifdef _FLASH_LED_ON_TRIGGER
	digitalWrite(LED_BUILTIN, LOW);
#endif
#ifdef _DEBUG_EXPOSURE
	logEvent("Keep Alive");
#endif

#ifdef _FLASH_LED_ON_TRIGGER   
   delay(pingdelay);
  digitalWrite(LED_BUILTIN, HIGH);
   delay(pingdelay);
  digitalWrite(LED_BUILTIN, LOW);
#endif
}

void exposure()
{  
	// Use the pins we set high to control transistor, optocoupler or relay.  Up to the user.
	// Set focus, then shutter - we may need to delay between focus and then shutter?
	digitalWrite(focusPin, HIGH);  
#ifdef _FLASH_LED_ON_TRIGGER 	
	digitalWrite(LED_BUILTIN, HIGH); 
#endif
  if (FOCUS_LEAD_TIME_MS > 0)
    delay(FOCUS_LEAD_TIME_MS);  // may need time to focus before shutter.
	digitalWrite(shutterPin, HIGH);	

	delay(SHUTTER_TIME_MS);

	digitalWrite(shutterPin, LOW);
	digitalWrite(focusPin, LOW);
#ifdef _FLASH_LED_ON_TRIGGER
	digitalWrite(LED_BUILTIN, LOW); 
#endif
#ifdef _DEBUG_EXPOSURE
  logEvent("Exposure");
#endif  
}
 
void logSettings()
{
#ifndef _NO_SERIAL
	Serial.print("Settings\n");
	Serial.print("-\tInterval (seconds): ");			Serial.print(INTERVAL_SECS, DEC); Serial.println();
	Serial.print("-\tShutter Time (ms):  ");			Serial.print(SHUTTER_TIME_MS, DEC);	 Serial.println();
	Serial.print("-\tExposures:          ");			Serial.print(NUMBER_OF_EXPOSURES, DEC); Serial.println();
	Serial.print("-\tFocus lead Time (ms):");			Serial.print(FOCUS_LEAD_TIME_MS, DEC); Serial.println();
	Serial.print("-\tKeep Alive Period (seconds):");	Serial.print(KEEP_ALIVE_PERIOD_SECS, DEC); Serial.println();

	Serial.print("-\tValid Days:         ");
	for (int d = 0; d < 7; d++)
	{
		if (VALID_DAYS[d]) {
			Serial.print(daysOfTheWeek[d]);	
			if (d < 6)
				Serial.print(", ");
		}
	}
	Serial.println();
	Serial.print("-\tValid Period:       ");
	if (START_HOUR < 10)
		Serial.print("0");
	Serial.print(START_HOUR, DEC); 
	Serial.print(":");
	if (START_MINUTE < 10)
		Serial.print("0");
	Serial.print(START_MINUTE, DEC);
	Serial.print(" to ");
	if (STOP_HOUR < 10)
		Serial.print("0");
	Serial.print(STOP_HOUR, DEC);
	Serial.print(":");
	if (STOP_MINUTE < 10)
		Serial.print("0");
	Serial.print(STOP_MINUTE, DEC);
	Serial.println();
	Serial.print("-\tFocus Pin:          ");	Serial.print(focusPin, DEC); Serial.println();
	Serial.print("-\tShutter Pin:        ");	Serial.print(shutterPin, DEC);	Serial.println();
	Serial.print("-\tRTC Interrupt Pin:  ");	Serial.print(rtcTimerIntPin, DEC);	Serial.println();
#endif
}

void logTime(time_t theTime)
{
#ifndef _NO_SERIAL  
	
	// maybe we can find another way to log?  that is not serial
	// we should make strings - and log to either oled, usb, serial, BT, etc
	// refactor code and allow coder to specify logging type(s) when building
	
	int dayOfWeek = weekday(theTime) - 1;
	int theHour = hour(theTime);
	int theMinute = minute(theTime);
	int theSecond = second(theTime);
	int theYear = year(theTime);
	Serial.print(theYear, DEC);	 	Serial.print('/');		Serial.print(month(theTime), DEC);		Serial.print('/');		Serial.print(day(theTime), DEC); 	Serial.print("  ");		Serial.print(theHour, DEC);		Serial.print(':');	
	if (theMinute < 10)
		Serial.print("0");
	Serial.print(theMinute, DEC);	Serial.print(':');
	if (theSecond < 10)
		Serial.print("0");
	Serial.print(theSecond, DEC); Serial.print("  (");	Serial.print(daysOfTheWeek[dayOfWeek]);	Serial.print(") ");
#endif
}

void logEvent(char * str)
{
#ifndef _NO_SERIAL
	logTime();
	Serial.print("  ");
	Serial.print(BOARD);
	Serial.print("  ");
	Serial.print(str);
	Serial.println();
#endif
}

void setupCameraPins()
{
	// set the focus and shutter pins
	pinMode(focusPin, OUTPUT);
	pinMode(shutterPin, OUTPUT);
	pinMode(13, OUTPUT);    // just for led

	digitalWrite(focusPin, LOW);
	digitalWrite(shutterPin, LOW);
}

void setupRTClock()
{
	// https://learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/arduino-library
	// conditional compilation depending on RTC we are using
	setupRTC3231();
	setupRTC1307();	
}

void setupRTC1307()
{
}

void setupIntervalometerSettings()
{
	start_time = (60 * START_HOUR) + START_MINUTE;
	stop_time = (60 * STOP_HOUR) + STOP_MINUTE;
}

void setupOneHertzTimer()
{
	// Add specific boards/chips here
	// to keep this part clean we ifdef the code in the function so if the board is not defined it just returns/no code to run	
#ifdef _RTC_DS3231
	// use rtc of some kind 
	setup3231OneHzTimer();
#elif _RTC_1307
	setup1307OneHzTimer();
#endif
}

void setup3231OneHzTimer()
{
	// http://arduino.stackexchange.com/questions/29873/how-to-set-up-one-second-interrupt-isr-for-ds3231-rtc/29881#29881
	logEvent("Setting RTC timer pin"); 		
	RTC.squareWave(SQWAVE_1_HZ);
}

void rtc_interrupt(void)
{
	commonTimerFunction();	
}


void traceDebug(char* s)
{
#ifdef _TRACE
	Serial.print("TRACE - ");
	Serial.print(s);
	Serial.print("\n");
#endif
}

void setupRTC3231()
{
	Serial.begin(9600);
	setSyncProvider(RTC.get);   // the function to get the time from the RTC
	if (timeStatus() != timeSet)
		Serial.println("Unable to sync with the RTC");
	else
		Serial.println("RTC has set the system time");

	bool stopped = RTC.oscStopped();

	if (stopped || gForceClockSet)
	{
		//  we should probably beep or flash or something here - since we're not in good shape!
		// this sets the time to be hardcoded to the time we compiled/uploaded to the board.  Not really useful except when initially building
		Serial.println("Detected clock power loss - resetting RTC date");
		time_t newTime = cvt_date(__DATE__, __TIME__);
		adjustTime(newTime);
		setTime(newTime);
	}
	else {
		Serial.println("Clock did not lose power");
	}
}

time_t cvt_date(char const *date, char const *time)
{
	char s_month[5];
	int year;
	tmElements_t t;
	static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	sscanf(date, "%s %hhd %d", s_month, &t.Day, &year);
	sscanf(time, "%2hhd %*c %2hhd %*c %2hhd", &t.Hour, &t.Minute, &t.Second);
	t.Month = (strstr(month_names, s_month) - month_names) / 3 + 1;
	if (year > 99) t.Year = year - 1970;
	else t.Year = year + 30;
	return makeTime(t);
}

void logTime() {
	time_t theTime = now();
	logTime(theTime);
}
