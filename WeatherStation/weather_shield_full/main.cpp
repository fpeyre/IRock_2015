/* 
   Created by SAMBE Adji Ndeye Ndate and PEYRE Flavien
   on 04/03/2015
   SmartCampus2015 and irock2015 projects
   Code for the Sparkfun Weather shield
   Code created with the help of https://github.com/sparkfun/Weather_Shield/blob/master/firmware/Weather_Shield_with_GPS/Weather_Shield_with_GPS.ino
   Tested on stm32L152RE 

*/
#include "mbed.h"
#include "USBSerial.h"
#include "MPL3115A2.h" //Pressure + temperature sensor
#include "Temperature.h" //Needed for temperature
#include "HTU21D.h" //Humidity sensor
#include <TinyGPS.h> //GPS parsing

//timer for mesuring time
Timer t;
TinyGPS gps;

int id=1;

PinName RXPin = PB_4, TXPin = PC_14;//GPS is attached to pin PB_5(TX from GPS) and pin PB_4 (RX into GPS)
PinName sda = PB_9, scl = PB_8; 
PinName WSPEED = PC_15, RAIN = PA_10, STAT1= PA_8, STAT2 = PA_9, GPS_PWRCTL = PB_10 ; // digital I/O pins
PinName REFERENCE_3V3 = PB_0, LIGHT = PA_1, BATT = PA_4, WDIR = PA_0;// analog I/O pins

//USBSerial serial;
Serial serial(USBTX, USBRX);
I2C  i2c(sda, scl);

MPL3115A2 myPressure(&i2c); //Create an instance of the pressure sensor
HTU21D myHumidity(sda, scl); //Create an instance of the humidity sensor

InterruptIn inter_rain(RAIN);
InterruptIn inter_wspeed(WSPEED);

long lastSecond; //The millis counter to see when a second rolls by
byte seconds; //When it hits 60, increase the current minute
byte seconds_2m; //Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
byte minutes; //Keeps track of where we are in various arrays of data
byte minutes_10m; //Keeps track of where we are in wind gust/dir over last 10 minutes array of data

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;

//We need to keep track of the following variables:
//Wind speed/dir each update (no storage)
//Wind gust/dir over the day (no storage)
//Wind speed/dir, avg over 2 minutes (store 1 per second)
//Wind gust/dir over last 10 minutes (store 1 per minute)
//Rain over the past hour (store 1 per minute)
//Total rain over date (store one per day)

byte windspdavg[120]; //120 bytes to keep track of 2 minute average
int winddiravg[120]; //120 ints to keep track of 2 minute average
float windgust_10m[10]; //10 floats to keep track of 10 minute max
int windgustdirection_10m[10]; //10 ints to keep track of 10 minute max
volatile float rainHour[60]; //60 floating numbers to keep track of 60 minutes of rain

//These are all the weather values that wunderground expects:
int winddir = 0; // [0-360 instantaneous wind direction]
float windspeedmph = 0; // [mph instantaneous wind speed]
float windgustmph = 0; // [mph current wind gust, using software specific time period]
int windgustdir = 0; // [0-360 using software specific time period]
float windspdmph_avg2m = 0; // [mph 2 minute average wind speed mph]
int winddir_avg2m = 0; // [0-360 2 minute average wind direction]
float windgustmph_10m = 0; // [mph past 10 minutes wind gust mph ]
int windgustdir_10m = 0; // [0-360 past 10 minutes wind gust direction]
int humidity = 0; // [%]

Temperature* tempf = new Temperature(); // [temperature F]
Temperature *tempfara;
float rainin = 0; // [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
volatile float dailyrainin = 0; // [rain inches so far today in local time]
Pressure* pressure = new Pressure(0.0);

float batt_lvl = 11.8; //[analog value from 0 to 1023]
float light_lvl = 455; //[analog value from 0 to 1023]

long *latitude;
long *longitude;
float altitude = 0.0;
unsigned long satellite = 0;
int *year;
byte *month;
byte *day;
byte *hour;
byte *minute;
byte *second;
byte *hundredths = 0;
unsigned long *fix_age = 0;

float  tempReturn = 0.0;
// volatiles are subject to modification by IRQs
volatile unsigned long raintime, rainlast, raininterval, rain;

//Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
  raintime = t.read_ms(); // grab current time
  raininterval = raintime - rainlast; // calculate interval between this and last event

    if (raininterval > 10) // ignore switch-bounce glitches less than 10mS after initial edge
  {
    dailyrainin += 0.011; //Each dump is 0.011" of water
    rainHour[minutes] += 0.011; //Increase this minute's amount of rain

    rainlast = raintime; // set up for next event
  }
}

void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
  if (t.read_ms() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
  {
    lastWindIRQ = t.read_ms(); //Grab the current time
    windClicks++; //There is 1.492MPH for each click per second.
  }
}


void setup()
{
  
  serial.baud(9600); //Begin listening to GPS over software serial at 9600. This should be the default baud of the module.
  serial.printf("Weather Shield");

  DigitalOut stat1(STAT1); //Status LED Blue
  DigitalOut stat2(STAT2); //Status LED Green
  
  DigitalOut gps_pwrctl(GPS_PWRCTL);
  gps_pwrctl = true; //Pulling this pin low puts GPS to sleep but maintains RTC and RAM
  
  DigitalIn wspeed(WSPEED);//pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  DigitalIn rain(RAIN) ;//pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor
  
  AnalogIn reference_3v3(REFERENCE_3V3);
  AnalogIn light(LIGHT);

  //Configure the pressure sensor
  myPressure.init(); // Get sensor online
  myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
  myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
  myPressure.enableEventFlags(); // Enable all three pressure and temp event flags 

  seconds = 0;
  lastSecond = t.read_ms();

 // turn on interrupts
  inter_rain.enable_irq();
  inter_wspeed.enable_irq();
  
  // attach external interrupt pins to IRQ functions;
  inter_rain.fall(&rainIRQ);
  inter_wspeed.fall(&wspeedIRQ);
 
  serial.printf("Weather Shield online!");
}

//Returns the instataneous wind speed
float get_wind_speed()
{
  float deltaTime = t.read_ms() - lastWindCheck; //750ms

  deltaTime /= 1000.0; //Covert to seconds

  float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

  windClicks = 0; //Reset and start watching for new wind
  lastWindCheck = t.read_ms();

  windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

  return(windSpeed);
}

//Read the wind direction sensor, return heading in degrees
int get_wind_direction() 
{
  //unsigned int adc;
  float adc;

  AnalogIn wdir(WDIR); // get the current reading from the sensor
  adc = wdir.read();

  // The following table is ADC readings for the wind direction sensor output, sorted from low to high.
  // Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
  // Note that these are not in compass degree order! See Weather Meters datasheet for more information.

  if (adc < 380) return (113);
  if (adc < 393) return (68);
  if (adc < 414) return (90);
  if (adc < 456) return (158);
  if (adc < 508) return (135);
  if (adc < 551) return (203);
  if (adc < 615) return (180);
  if (adc < 680) return (23);
  if (adc < 746) return (45);
  if (adc < 801) return (248);
  if (adc < 833) return (225);
  if (adc < 878) return (338);
  if (adc < 913) return (0);
  if (adc < 940) return (293);
  if (adc < 967) return (315);
  if (adc < 990) return (270);
  return (-1); // error, disconnected?
}


//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
  AnalogIn readRef(REFERENCE_3V3);
  float operatingVoltage = readRef.read();

  AnalogIn readLight(LIGHT);
  float lightSensor = readLight.read();
  
  operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V
  
  lightSensor = operatingVoltage * lightSensor;
  
  return(lightSensor);
}


//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float get_battery_level()
{
  AnalogIn read(REFERENCE_3V3);
  float operatingVoltage = read.read();

  AnalogIn batt(BATT);
  float rawVoltage = batt.read();
  
  operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V
  
  rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin
  
  rawVoltage *= 4.90; //(3.9k+1k)/1k - multiple BATT voltage by the voltage divider to get actual system voltage
  
  return(rawVoltage);
}


//Calculates each of the variables that wunderground is expecting
void calcWeather()
{
  //Calc winddir
  winddir = get_wind_direction();

  //Calc windspeed
  windspeedmph = get_wind_speed();

  //Calc windgustmph
  //Calc windgustdir
  //Report the largest windgust today
  windgustmph = 0;
  windgustdir = 0;

  //Calc windspdmph_avg2m
  float temp = 0;
  for(int i = 0 ; i < 120 ; i++)
    temp += windspdavg[i];
  temp /= 120.0;
  windspdmph_avg2m = temp;

  //Calc winddir_avg2m
  temp = 0; //Can't use winddir_avg2m because it's an int
  for(int i = 0 ; i < 120 ; i++)
    temp += winddiravg[i];
  temp /= 120;
  winddir_avg2m = temp;

  //Calc windgustmph_10m
  //Calc windgustdir_10m
  //Find the largest windgust in the last 10 minutes
  windgustmph_10m = 0;
  windgustdir_10m = 0;
  //Step through the 10 minutes  
  for(int i = 0; i < 10 ; i++)
  {
    if(windgust_10m[i] > windgustmph_10m)
    {
      windgustmph_10m = windgust_10m[i];
      windgustdir_10m = windgustdirection_10m[i];
    }
  }

  //Calc humidity
  humidity = myHumidity.sample_humid();

  //Calc tempf from pressure sensor
  int f = myHumidity.sample_ctemp();
  tempfara = myPressure.readTemperature(tempf);

  //Total rainfall for the day is calculated within the interrupt
  //Calculate amount of rainfall for the last 60 minutes
  rainin = 0;  
  for(int i = 0 ; i < 60 ; i++)
    rainin += rainHour[i];

  //Calc pressure
  pressure = myPressure.readPressure(pressure);

  //Calc dewptf

  //Calc light level
  light_lvl = get_light_level();

  //Calc battery level
  batt_lvl = get_battery_level();
  
}

//Prints the various variables directly to the port
//I don't like the way this function is written but Arduino doesn't support floats under sprintf
void printWeather()
{
  calcWeather(); //Go calc all the various sensors

  serial.printf("\n");
  /*serial.printf("$,winddir=");
  serial.printf("%d", winddir);
  serial.printf(",windspeedmph=");
  serial.printf("%f",windspeedmph);
  Serial.print(",windgustmph=");
  Serial.print(windgustmph, 1);
  Serial.print(",windgustdir=");
  Serial.print(windgustdir);
  Serial.print(",windspdmph_avg2m=");
  Serial.print(windspdmph_avg2m, 1);
  Serial.print(",winddir_avg2m=");
  Serial.print(winddir_avg2m);
  Serial.print(",windgustmph_10m=");
  Serial.print(windgustmph_10m, 1);
  Serial.print(",windgustdir_10m=");
  Serial.print(windgustdir_10m);
  serial.printf(",humidity=");
  serial.printf("%d",humidity, 2);
  serial.printf(",tempf=");
  serial.printf(tempfara->print(), 2);
  serial.printf(",rainin=");
  serial.printf("%f",rainin, 2);
  serial.printf(",dailyrainin=");
  serial.printf("%f",dailyrainin, 2);
  serial.printf(",pressure=");
  serial.printf("%f",pressure, 2);
  serial.printf(",batt_lvl=");
  serial.printf("%f",batt_lvl, 2);
  serial.printf(",light_lvl=");
  serial.printf("%f",light_lvl, 2);

  gps.get_position(latitude,longitude,0);
  altitude = gps.f_altitude();
  satellite = gps.f_altitude();
  serial.printf(",lat=");
  serial.printf("%ld",latitude, 6);
  serial.printf(",long=");
  serial.printf("%ld",longitude, 6);
  serial.printf(",altitude=");
  serial.printf("%f",altitude);
  serial.printf(",sats=");
  serial.printf("%f",satellite);

  char sz[32];
  gps.crack_datetime(year, month, day,hour,minute,second, hundredths,fix_age);*/
  
  //serial.printf(",date=");
  //sprintf(sz, "%02d/%02d/%02d", month,day,year);
  //serial.printf(sz);

  //serial.printf(",time=");
  //sprintf(sz, "%02d:%02d:%02d",hour, minute,second);
  //serial.printf(sz);

  /*serial.printf(",");
  serial.printf("# ","\n");*/
  serial.printf("%d/%f/%d/%d/%ld/%f/%f/%f",id,windspdmph_avg2m,winddir_avg2m,humidity,tempf->print(),rainin,pressure,batt_lvl);
  

}

//While we delay for a given amount of time, gather GPS data
static void smartdelay(unsigned long ms)
{
  unsigned long start = (long)t.read_ms();
  do 
  {
    while (serial.readable() == 1) {}// readable retourne 1 si il y a quelque chose à lire
      //gps.encode(ps.read());
  } while ((long)t.read_ms() - start < ms);
}

void loop()
{
  //Keep track of which minute it is
  unsigned long ecouleTemps = (long) t.read_ms();
  if(ecouleTemps - lastSecond >= 1000)
  {
    DigitalOut stat1(STAT1);
    stat1 = true; //Blink stat LED
    
    lastSecond += 1000;

    //Take a speed and direction reading every second for 2 minute average
    if(++seconds_2m > 119) seconds_2m = 0;

    //Calc the wind speed and direction every second for 120 second to get 2 minute average
    float currentSpeed = get_wind_speed();
    //float currentSpeed = random(5); //For testing
    int currentDirection = get_wind_direction();
    windspdavg[seconds_2m] = (int)currentSpeed;
    winddiravg[seconds_2m] = currentDirection;
    //if(seconds_2m % 10 == 0) displayArrays(); //For testing

    //Check to see if this is a gust for the minute
    if(currentSpeed > windgust_10m[minutes_10m])
    {
      windgust_10m[minutes_10m] = currentSpeed;
      windgustdirection_10m[minutes_10m] = currentDirection;
    }

    //Check to see if this is a gust for the day
    if(currentSpeed > windgustmph)
    {
      windgustmph = currentSpeed;
      windgustdir = currentDirection;
    }
    
    if(++seconds > 59)
    {
      seconds = 0;

      if(++minutes > 59) minutes = 0;
      if(++minutes_10m > 9) minutes_10m = 0;

      rainHour[minutes] = 0; //Zero out this minute's rainfall amount
      windgust_10m[minutes_10m] = 0; //Zero out this minute's gust
    }

    //Report all readings every second
    printWeather();

    stat1 = false; //Turn off stat LED
  }

  smartdelay(800); //Wait 1 second, and gather GPS data
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


int main(void) {
    t.start();
    setup();
    while(1) 
    { 
       loop();
    } 
    
}
