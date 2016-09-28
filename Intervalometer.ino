#include <RTClib.h>  

//////////////////////////////////////////////////////////////
// edit these values to set the interval for taking photos
//////////////////////////////////////////////////////////////
int		DELAY = 0;	// delay from when program is started to when it should start taking photos
int		SHUTTER_TIME = 250;	// (in milliseconds) I have no idea what this means.  what is the resolution of this? 
int		INTERVAL = 2;	// number of seconds to wait between photos
int		NUMBER_OF_EXPOSURES = 1;	// number of photos to take for each 'loop'/command to take a photo
bool	VALID_DAYS[] = {		// true for yes, take photo, false for no - don't take photo
	false,  // sunday
	true, // monday
	true, // etc
	true,
	true,
	false,
	true
};

// Set the se to all 0 if you want to take photos at any time, otherwise set the beginning hour and minute to be before the stop hour and minute
// Hours should be 0 to 23
// minutes 0 to 59
int START_HOUR = 0;
int START_MINUTE = 3;
int STOP_HOUR = 23;
int STOP_MINUTE = 58;

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

//RTC_DS3231 rtc;
RTC_Millis rtc;

boolean toggle1 = 0;   // this is used only for example - can probably remove

					   // this is just for logging
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };


void LogEvent(char * str)
{
	logTime(now);
	Serial.print("  ");
	Serial.print(str);
	Serial.println();
}

/*
http://playground.arduino.cc/Main/DS1302
https://learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/understanding-the-code
*/
void setup()
{
	/*
	https://learn.adafruit.com/ds1307-real-time-clock-breakout-board-kit/arduino-library
	*/
	/* add setup code here */

	// depending on what RTC (if any) you need to call the correct one.
	//if (!rtc.begin()) {		
	//Serial.println("RTC is NOT running!");
	// following line sets the RTC to the date & time this sketch was compiled
	//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
	rtc.begin(DateTime(F(__DATE__), F(__TIME__)));  // use this for the millis - not for the other chip
													//}

													//Serial.begin(9600);
													// set the focus and shutter pins
	pinMode(focusPin, OUTPUT);
	pinMode(shutterPin, OUTPUT);
	pinMode(13, OUTPUT);    // just for led - nothing important

	digitalWrite(focusPin, LOW);
	digitalWrite(shutterPin, LOW);

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

	start_time = (60 * START_HOUR) + START_MINUTE;
	stop_time = (60 * STOP_HOUR) + STOP_MINUTE;

	now = rtc.now();
	LogEvent("Started");
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

	//logTime(now);	

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

	// this is all just debugging crap
	return true;
}

//timer1 interrupt 1Hz toggles pin 13 (LED)
//generates pulse wave of frequency 1Hz/2 = 0.5kHz (takes two cycles for full wave- toggle high then toggle low)
ISR(TIMER1_COMPA_vect)
{
	/*
	Order of checking:
	Did we reach our interval?
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

// Parameter is how long to keep it open/take pic
void exposure(int duration)
{
	// *********TO DO*********
	// trigger some relay or transistor or optocoupler in the circuit

	// I don't know what order these have to be in (focus then shutter - but do they need delay or same time is ok?)
	digitalWrite(focusPin, HIGH);  // do we want a delay between focus and shutter?
	digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
	digitalWrite(shutterPin, HIGH);

	delay(duration);

	digitalWrite(shutterPin, LOW);
	digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
	digitalWrite(focusPin, LOW);

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
