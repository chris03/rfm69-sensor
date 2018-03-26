//
// Author:
//   Chris03.com
//
// Purpose:
//   Read sensors and voltage.
//   Transmit all data via RFM69 radio and sleep for 1min.
//
// Hardware:
//   ATmega328 with RFM69W 915Mhz radio and SPI flash (Miniwireless or Moteino)
//   SI7021 Temperature & Humidity sensor
//   BMP180 Temperature & Pressure sensor (Optional)
//   Voltage divider to read battery voltage
//
// Libraries used:
//   https://github.com/LowPowerLab/RFM69
//   https://github.com/LowPowerLab/SPIFlash
//   https://github.com/LowPowerLab/LowPower
//   https://github.com/LowPowerLab/SI7021
//
#include <Wire.h>

#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>  
#include <LowPower.h>

// Sensors
#include <Adafruit_BMP085.h>
#include <SI7021.h>

#define SERIAL_EN
#ifdef SERIAL_EN
  #define SERIAL_BAUD   9600
  #define DEBUG(input)   {Serial.print(input);}
  #define DEBUGln(input) {Serial.println(input);}
  #define SERIALFLUSH() {Serial.flush();}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
  #define SERIALFLUSH();
#endif

#define NUM_SAMPLES 3
#define VOLTAGE_REF 3.27

#define LEDPIN 9

#define GATEWAY_ID    1
#define NODE_ID       11
#define NETWORK_ID    100
#define FREQUENCY     RF69_915MHZ
#define ENCRYPT_KEY   "xxxxxxxxxxxxxxxx"

// Send data this many sleep loops (15 loops of 8sec cycles = 120sec ~ 2 minutes)
#define SEND_LOOPS   7 

#define SLEEP_FASTEST SLEEP_15MS
#define SLEEP_FAST SLEEP_250MS
#define SLEEP_SEC SLEEP_1S
#define SLEEP_LONG SLEEP_2S
#define SLEEP_LONGER SLEEP_4S
#define SLEEP_LONGEST SLEEP_8S
period_t sleepTime = SLEEP_LONGEST;

SPIFlash flash(5);  
RFM69 radio;

Adafruit_BMP085 bmp;
bool bmpOk;

SI7021 si7021;
bool si7021Ok;

char buffer[50];
byte sendLoops=0;
float bmpTemp;
float bmpPres;
int siHum;
double siTemp;
  
void setup() {    
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);

#ifdef SERIAL_EN
  Serial.begin(SERIAL_BAUD);
  Serial.print("Radio at ");
  Serial.print((FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915));
  Serial.println("Mhz");
  Serial.print("NodeId: ");
  Serial.print(NODE_ID);
  Serial.print(" NetworkId: ");
  Serial.println(NETWORK_ID);
  delay(20);
#endif
  radio.initialize(FREQUENCY, NODE_ID, NETWORK_ID);
  radio.encrypt(0);
    
  bmpOk = bmp.begin();
  DEBUG("BMP180: ");
  DEBUGln(bmpOk ? "Ok" : "Fail");

  si7021Ok = si7021.begin();
  DEBUG("SI7021: ");
  DEBUGln(si7021Ok ? "Ok" : "Fail");

  sprintf(buffer, "Hi! BMP:%i SI:%i", bmpOk, si7021Ok);
  byte sendLen = strlen(buffer);
  DEBUGln(buffer);
  radio.send(GATEWAY_ID, buffer, sendLen); 

  flash.initialize();
  flash.sleep();
}

void loop() {
  if (sendLoops--<=0)   //send readings every SEND_LOOPS
  {
    sendLoops = SEND_LOOPS-1;
    run();
  }

  digitalWrite(LEDPIN, HIGH);
  DEBUGln("SLEEP RADIO");
  radio.sleep();
 // flash.sleep();
  DEBUGln("POWER DOWN");
  SERIALFLUSH();
  digitalWrite(LEDPIN, LOW);
  LowPower.powerDown(sleepTime, ADC_OFF, BOD_OFF);
  DEBUGln("WAKEUP");
  SERIALFLUSH();
}

void run()
{
    String msg = "";
    byte sendLen;
    float voltage;
    
    if(bmpOk)
    {
      bmpTemp = bmp.readTemperature();
      bmpPres = bmp.readSealevelPressure(32 /* meters altitude */) / 100.0;
      DEBUG("BMP180 Temp: ");DEBUG(bmpTemp);DEBUG(" Pressure: ");DEBUGln(bmpPres);
      msg += "(BMP|";
      msg += bmpTemp;
      msg += "|";
      msg += bmpPres;
      msg += ")";
    }  

    if(si7021Ok)
    {
      siTemp = si7021.getCelsiusHundredths() / 100.0;
      siHum = si7021.getHumidityPercent();

      DEBUG("SI7021 Temp: ");DEBUG(siTemp);DEBUG(" Hum: ");DEBUGln(siHum);

      msg += "(SI|";
      msg += siTemp;
      msg += "|";
      msg += siHum;
      msg += ")";
    }

    voltage = readVoltage();

    msg += "(V|";
    msg += voltage;
    msg += ")";
    
    msg.toCharArray(buffer, 50);
    sendLen = strlen(buffer);
    radio.send(GATEWAY_ID, buffer, sendLen);
    DEBUGln(buffer);
}

float readVoltage()
{
  float voltage;
  unsigned char sample_count = 0;
  int sum = 0;
  
  while (sample_count < NUM_SAMPLES) {
    sum += analogRead(A1);
    sample_count++;
    delay(10);
  }
  
  voltage = 2 * (((float)sum / (float)NUM_SAMPLES * VOLTAGE_REF) / 1024.0);

  DEBUG(voltage );
  DEBUGln(" V");

  return voltage;
}