#include <Time.h>
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

int		DELAY				= 0;	// delay from when program is started to when it should start taking photos
int		SHUTTER_TIME		= 500;	// (in milliseconds) I have no idea what this means.  what is the resolution of this? 
int		INTERVAL			= 4;	// number of seconds to wait between photos  - for 1 minute use 60, one hour use 3600, etc
int		NUMBER_OF_EXPOSURES = 1;	// number of photos to take for each 'loop'/command to take a photo
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
int START_MINUTE	= 3;	// 0 to 59
int STOP_HOUR		= 23;	// 0 to 23
int STOP_MINUTE		= 58;	// 0 to 59

// set these as desired to control the shutter and focus
const int focusPin			= 6;
const int shutterPin		= 7;
const int rtcTimerIntPin	= 3;	// this is not working for some reason

bool gForceClockSet = false;  // = false  - forces setting the clock when compiling.  Do this for initial clock/board set up.
//////////////////////////////////////////////////////////////
// End of user editable settings
//////////////////////////////////////////////////////////////

volatile bool state = false;
volatile bool newstate = false;
volatile bool triggerPhoto = false;

int start_time = 0;
int stop_time = 0;
int interval_counter = 0;

// set the type of clock - the default is no RTC - but set the macro for the one you are using.



#ifdef _RTC_DS3231
//DS3232RTC rtc;
#elif _RTC_DS1307
RTC_DS1307 rtc;
#else
RTC_Millis rtc;  // this is pretty useless - deprecating the functionaliy if we don't have a rtc
#endif

// just for logging
char daysOfTheWeek[7][12] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

void setup()
{
	pinMode(rtcTimerIntPin, INPUT);
	attachInterrupt(rtcTimerIntPin, rtc_interrupt, RISING);
	setupRTClock();	
	setupCameraPins();
	setupIntervalometerSettings();
	logEvent("Started");
	setupOneHertzTimer();
	logSettings();
}

void loop()
{
	// ****NOTE****
	// If you set the interval to some value that will be "overstepped" by the shutter speed and number of exposures then that is user error and we can't help you --		
	// we could do some warning message though?

	// Since I could not get the 1hz square wave to work, we cheat in the loop and check the time.  
	// If we're not doing much work it's no big deal, but it would be better to have the ISR/trigger...
	static unsigned long previousTime = 0;	
	time_t timeNow = gRTC.get();	 

	if (timeNow != previousTime) {
		commonTimerFunction();
		previousTime = timeNow;             // remember previous time
	}

	if (triggerPhoto)
	{
		for (int i = 0; i < NUMBER_OF_EXPOSURES; i++)
		{
			exposure(SHUTTER_TIME);
		}
		triggerPhoto = false;
	}
	
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
	setSyncProvider(gRTC.get);   // the function to get the time from the RTC
	if (timeStatus() != timeSet)
		Serial.println("Unable to sync with the RTC");
	else
		Serial.println("RTC has set the system time");

	bool stopped = gRTC.oscStopped();
	
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

bool CheckIfWeShouldTakePhoto()
{	
	time_t theTime = gRTC.get();
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
#ifdef _DEBUG_TICK
	//logEvent("Tick");
#endif

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
			logEvent("Transitioned to taking photos");
		else
			logEvent("Transitioned to NOT taking photos");
	} 
}

// Parameter is how long to keep it open/take pic
void exposure(int duration)
{
	// Use the pins we set high to control transistor, optocoupler or relay.  Up to the user.
	// Set focus, then shutter - we may need to delay between focus and then shutter?
	digitalWrite(focusPin, HIGH);  
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
#ifdef _DEBUG_EXPOSURE
  logEvent("Exposure");
#endif
}
 
void logSettings()
{
#ifndef _NO_SERIAL
	Serial.print("Settings\n");
	Serial.print("-\tInterval (seconds): ");	Serial.print(INTERVAL, DEC); Serial.println();
	Serial.print("-\tShutter Time (ms):  ");	Serial.print(SHUTTER_TIME, DEC);	 Serial.println();
	Serial.print("-\tExposures:          ");		Serial.print(NUMBER_OF_EXPOSURES, DEC); Serial.println();
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

// from https://forums.adafruit.com/viewtopic.php?f=57&t=103936
// Set timer TC4 to call the TC4_Handler every second
void setupZero()
{
#ifdef ARDUINO_SAMD_FEATHER_M0
	// Set up the generic clock (GCLK4) used to clock timers
	REG_GCLK_GENDIV = GCLK_GENDIV_DIV(1) |          // Divide the 48MHz clock source by divisor 1: 48MHz/1=48MHz
		GCLK_GENDIV_ID(4);            // Select Generic Clock (GCLK) 4
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

	REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |           // Set the duty cycle to 50/50 HIGH/LOW
		GCLK_GENCTRL_GENEN |         // Enable GCLK4
		GCLK_GENCTRL_SRC_DFLL48M |   // Set the 48MHz clock source
		GCLK_GENCTRL_ID(4);          // Select GCLK4
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

													// Feed GCLK4 to TC4 and TC5
	REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN |         // Enable GCLK4 to TC4 and TC5
		GCLK_CLKCTRL_GEN_GCLK4 |     // Select GCLK4
		GCLK_CLKCTRL_ID_TC4_TC5;     // Feed the GCLK4 to TC4 and TC5
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

	REG_TC4_COUNT16_CC0 = 0xB71A;                   // Set the TC4 CC0 register as the TOP value in match frequency mode
	while (TC4->COUNT16.STATUS.bit.SYNCBUSY);       // Wait for synchronization

													//NVIC_DisableIRQ(TC4_IRQn);
													//NVIC_ClearPendingIRQ(TC4_IRQn);
	NVIC_SetPriority(TC4_IRQn, 0);    // Set the Nested Vector Interrupt Controller (NVIC) priority for TC4 to 0 (highest)
	NVIC_EnableIRQ(TC4_IRQn);         // Connect TC4 to Nested Vector Interrupt Controller (NVIC)

	REG_TC4_INTFLAG |= TC_INTFLAG_OVF;              // Clear the interrupt flags
	REG_TC4_INTENSET = TC_INTENSET_OVF;             // Enable TC4 interrupts
													// REG_TC4_INTENCLR = TC_INTENCLR_OVF;          // Disable TC4 interrupts

	REG_TC4_CTRLA |= TC_CTRLA_PRESCALER_DIV1024 |   // Set prescaler to 1024, 48MHz/1024 = 46.875kHz
		TC_CTRLA_WAVEGEN_MFRQ |        // Put the timer TC4 into match frequency (MFRQ) mode
		TC_CTRLA_ENABLE;               // Enable TC4
	while (TC4->COUNT16.STATUS.bit.SYNCBUSY);       // Wait for synchronization

	PORT->Group[g_APinDescription[LED_BUILTIN].ulPort].DIRSET.reg = (uint32_t)(1 << g_APinDescription[LED_BUILTIN].ulPin);
	PORT->Group[g_APinDescription[LED_BUILTIN].ulPort].OUTSET.reg = (uint32_t)(1 << g_APinDescription[LED_BUILTIN].ulPin);
#endif
}

#ifdef ARDUINO_SAMD_FEATHER_M0
void TC4_Handler()                              // Interrupt Service Routine (ISR) for timer TC4
{
	// Check for overflow (OVF) interrupt
	if (TC4->COUNT16.INTFLAG.bit.OVF && TC4->COUNT16.INTENSET.bit.OVF)
	{
		// Put your timer overflow (OVF) code here:     
		commonTimerFunction();
		REG_TC4_INTFLAG = TC_INTFLAG_OVF;         // Clear the OVF interrupt flag
	}
}

#endif

#ifdef __AVR_ATmega328P__
ISR(TIMER1_COMPA_vect)
{
	// copied from examples
	// generates pulse wave of frequency 1Hz/2 = 0.5kHz (takes two cycles for full wave- toggle high then toggle low)
	commonTimerFunction();
}
#endif

void setupAtmega328()
{
#ifdef __AVR_ATmega328P__
	
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
#else
	setupAtmega328();		// Uno, etc
	setupZero();			// e.g. Adafruit Feather M0	
#endif
}

void setup3231OneHzTimer()
{
	// http://arduino.stackexchange.com/questions/29873/how-to-set-up-one-second-interrupt-isr-for-ds3231-rtc/29881#29881
	
	logEvent("Setting RTC timer pin"); 		
	gRTC.squareWave(SQWAVE_1_HZ);
}

void rtc_interrupt(void)
{
	commonTimerFunction();
	//logEvent("rtcInterrupt()");
	triggerPhoto = true;
}