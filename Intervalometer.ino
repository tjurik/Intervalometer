#include <RTClib.h>				// a real time clock lib

#include "boards.h"				// identifies the chip/board based on IDE's setting (we use it for logging)
#include"lib_customization.h"	// edit this file to define the real time clock and any other functionality-controlling parameters

// looks like we can use an NPN transistor - no problem
// use a 10k resistor?
// nikon pinout http://www.doc-diy.net/photo/remote_pinout/
// http://www.instructables.com/id/Nikon-D90-MC-DC2-Remote-Shutter-Hack/?ALLSTEPS


//////////////////////////////////////////////////////////////
// GLOBALS
//
// edit these values to set the interval for taking photos
//////////////////////////////////////////////////////////////

int		DELAY				= 0;	// delay from when program is started to when it should start taking photos
int		SHUTTER_TIME		= 250;	// (in milliseconds) I have no idea what this means.  what is the resolution of this? 
int		INTERVAL			= 2;	// number of seconds to wait between photos  - for 1 minute use 60, one hour use 3600, etc
int		NUMBER_OF_EXPOSURES = 1;	// number of photos to take for each 'loop'/command to take a photo
bool	VALID_DAYS[] = {			// true for yes, take photo, false for no - don't take photo
	false,// sunday
	true, // monday
	true, // tuesday
	true, // wednesday, etc
	true,
	false,
	true
};

// Set to all 0 if always take photo
// Otherwise set the beginning hour and minute to be before the stop hour and minute
int START_HOUR		= 0;	// 0 to 23
int START_MINUTE	= 3;	// 0 to 59
int STOP_HOUR		= 23;	// 0 to 23
int STOP_MINUTE		= 58;	// 0 to 59

//////////////////////////////////////////////////////////////
// End of user editable settings
//////////////////////////////////////////////////////////////

const int focusPin = 2;
const int shutterPin = 3;

DateTime now;
bool state = false;
bool newstate = false;
bool triggerPhoto = false;

// TO DO
// OPTOCOUPLER _ TO SET THE GROUND AND SHUTTER AND RELEASE
// http://www.martyncurrey.com/activating-the-shutter-release/

// We can make this more interesting and useful if there are multiple on and off times - 
// for now we only need one ON time - keep it simple and that was all that was in original request

// Allow a bluetooth or network re-config to change the settings.
// we could listen on some poert to details

// also allow some logging to a bluetooth phone or internet, or whatever.  Probably not worth the trouble for this little app

int start_time = 0;
int stop_time = 0;
int interval_counter = 0;


// set the type of clock - the default is no RTC - but set the macro for the one you are using.

#ifdef _RTC_DS3231_
RTC_DS3231 rtc; 
#elif _RTC_DS1307
RTC_DS1307 rtc;
#else
RTC_Millis rtc;
#endif

// this is just for logging
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

void LogEvent(char * str)
{
	logTime(now);
	Serial.print("  ");
	Serial.print(BOARD);
	Serial.print("  ");
	Serial.print(str);
	Serial.println();
}

/*
http://playground.arduino.cc/Main/DS1302
https://learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/understanding-the-code
*/

void setupRTClock()
{
	// https://learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/arduino-library
	// conditional compilation depending on RTC we are using
#ifdef _RTC_DS3231_	
	//if (!rtc.begin()) {		
	//Serial.println("RTC is NOT running!");	
	//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#elif _RTC_DS1307

#else
	rtc.begin(DateTime(F(__DATE__), F(__TIME__)));  // use this for the millis - not for the other chip
#endif

	// first call to set the time
	now = rtc.now();
}

void setupCameraPins()
{
	// set the focus and shutter pins
	pinMode(focusPin, OUTPUT);
	pinMode(shutterPin, OUTPUT);
	pinMode(13, OUTPUT);    // just for led - nothing important

	digitalWrite(focusPin, LOW);
	digitalWrite(shutterPin, LOW);
}

setupIntervalometerSettings()
{
	start_time = (60 * START_HOUR) + START_MINUTE;
	stop_time = (60 * STOP_HOUR) + STOP_MINUTE;
}

//#if defined(__AVR_Atmega32U4__) // Yun 16Mhz, Micro, Leonardo, Esplora

void setupOneHertzTimer()
{
	// standard UNO board
	// copied from examples
	// generates pulse wave of frequency 1Hz/2 = 0.5kHz (takes two cycles for full wave- toggle high then toggle low)
	cli();  // stop interrupts

			//set timer1 interrupt at 1Hz
	TCCR1A = 0;// set entire TCCR1A register to 0
	TCCR1B = 0;// same for TCCR1B
	TCNT1 = 0;//initialize counter value to 0
			  // set compare match register for 1hz increments
	OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
				  // turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set CS12 and CS10 bits for 1024 prescaler
	TCCR1B |= (1 << CS12) | (1 << CS10);
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);

	sei();//allow interrupts	

}

void setup()
{	
	setupRTClock();
	setupCameraPins();
	setupIntervalometerSettings();
	setupOneHertzTimer();
	
	LogEvent("Started");
	LogSettings();
}

void logTime(DateTime now)
{
	int dayOfWeek = now.dayOfTheWeek();
	int hour = now.hour();
	int minute = now.minute();
	int second = now.second();
	Serial.print(now.year(), DEC);	Serial.print('/');	Serial.print(now.month(), DEC);	Serial.print('/');	Serial.print(now.day(), DEC); Serial.print("  ");	Serial.print(hour, DEC);	Serial.print(':');
	if (minute < 10)
		Serial.print("0");
	Serial.print(minute, DEC);	Serial.print(':');
	if (second < 10)
		Serial.print("0");
	Serial.print(second, DEC); Serial.print("  (");	Serial.print(daysOfTheWeek[dayOfWeek]);	Serial.print(") ");
}

bool CheckIfWeShouldTakePhoto()
{
	now = rtc.now();
	int dayOfWeek = now.dayOfTheWeek();
	int hour = now.hour();
	int minute = now.minute();

	int current_time = (60 * hour) + minute;

	// check the day - return false if no
	if (!VALID_DAYS[dayOfWeek]) {
		return false;
	}
		
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
	/*
		Did we reach our interval?  (interval)		
		Get date and time from RTC
		Are we taking photos today
		Are we in a time slot that we are taking pics
		Did we reach our interval?
	*/

	interval_counter++;
	if (interval_counter % INTERVAL == 0) {
		// reset counter to 0 if we hit our interval % (to make division take less time?)
		interval_counter = 0;
		newstate = CheckIfWeShouldTakePhoto();
		triggerPhoto = newstate;
	}

	if (newstate != state) {
		state = newstate;
		if (state)
			LogEvent("Transitioned to taking photos");
		else
			LogEvent("Transitioned to NOT taking photos");
	}
}

ISR(TIMER1_COMPA_vect)
{
	commonTimerFunction();
}
 
// Parameter is how long to keep it open/take pic
void exposure(int duration)
{
	// trigger some relay or transistor or optocoupler in the circuit
	// I think we want focus then shutter - but do they need delay or same time is ok?
	digitalWrite(focusPin, HIGH);  // do we want a delay between focus and shutter?
#ifdef _FLASH_LED_ON_TRIGGER 	
	digitalWrite(LED_BUILTIN, HIGH); 
#endif
	digitalWrite(shutterPin, HIGH);	

	delay(duration);

	digitalWrite(shutterPin, LOW);
	digitalWrite(focusPin, LOW);
#ifdef _FLASH_LED_ON_TRIGGER
	digitalWrite(LED_BUILTIN, LOW); 
#endif
}
 
void loop()
{
	/*
	have a listener for getting new data?
	Configuration:
	--------------
	- delay - before it starts running (optional?)
	- interval (one second to hours)
	- duration of shutter
	- number of photos to take
	- days of the week to take photos
	- Valid time during day (thee is only one of these - though this could get more complex if we wished)
	****NOTE****
	if you set the interval to some value that will be "overstepped" by the shutter speed and number of exposures then that is user error and we can't help you --		we could do some waerning message though...
	*/

	if (triggerPhoto)
	{
		for (int i = 0; i < NUMBER_OF_EXPOSURES; i++)
		{
			exposure(SHUTTER_TIME);
		}
		triggerPhoto = false;
	}
	// log the transition - we may not need to do this.
	// we should email this info if possible?	
}

void LogSettings()
{
	Serial.print("Settings\n");
	Serial.print("-\tInterval (seconds): ");	Serial.print(INTERVAL, DEC); Serial.println();
	Serial.print("-\tShutter Time (ms):  ");	Serial.print(SHUTTER_TIME, DEC);	 Serial.println();
	Serial.print("-\tExposures:          ");		Serial.print(NUMBER_OF_EXPOSURES, DEC); Serial.println();
	Serial.print("-\tValid Days:         ");
	for (int d = 0; d < 7; d++)
	{
		if (VALID_DAYS[d]) {
			Serial.print(daysOfTheWeek[d]);	Serial.print(", ");
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
}
