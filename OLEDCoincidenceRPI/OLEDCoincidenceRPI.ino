/*
  CosmicWatch Desktop Muon Detector Arduino Code

  This code is used to record data to the built in microSD card reader/writer.
  
  Questions?
  Spencer N. Axani
  saxani@mit.edu

  Requirements: Sketch->Include->Manage Libraries:
  SPI, EEPROM, SD, and Wire are probably already installed.
  1. Adafruit SSD1306     -- by Adafruit Version 1.0.1
  2. Adafruit GFX Library -- by Adafruit Version 1.0.2
  3. TimerOne             -- by Jesse Tane et al. Version 1.1.0
*/

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <TimerOne.h>
#include <Wire.h>
#include <SPI.h>
// #include <SD.h>
#include <EEPROM.h>

// #define SDPIN 10
// SdFile root;
// Sd2Card card;
// SdVolume volume;

// File myFile;
#define OLED_RESET 10
Adafruit_SSD1306 display(OLED_RESET);

#define TIMER_INTERVAL 1000000

const int SIGNAL_THRESHOLD    = 75;        // Min threshold to trigger on
const int RESET_THRESHOLD     = 25; 

const int LED_BRIGHTNESS      = 255;         // Brightness of the LED [0,255]

//Calibration fit data for 10k,10k,249,10pf; 20nF,100k,100k, 0,0,57.6k,  1 point
const long double cal[] = {-9.085681659276021e-27, 4.6790804314609205e-23, -1.0317125207013292e-19,
  1.2741066484319192e-16, -9.684460759517656e-14, 4.6937937442284284e-11, -1.4553498837275352e-08,
   2.8216624998078298e-06, -0.000323032620672037, 0.019538631135788468, -0.3774384056850066, 12.324891083404246};
   
const int cal_max = 1023;

//initialize variables
char detector_name[40];

unsigned long time_stamp                    = 0L;
unsigned long measurement_deadtime          = 0L;
unsigned long time_measurement              = 0L;      // Time stamp
unsigned long interrupt_timer               = 0L;      // Time stamp
int           start_time                    = 0L;      // Start time reference variable
long int      total_deadtime                = 0L;      // total time between signals
unsigned long waiting_t1 = 0L;

unsigned long measurement_t1;
unsigned long measurement_t2;

float temperatureC;


long int      count                         = 0L;         // A tally of the number of muon counts observed
long int      coincident_count = 0;
float         last_adc_value                = 0;
float last_sipm_voltage = 0;
// char          filename[]                    = "File_000.txt";
int           Mode                          = 1;

byte SLAVE;
byte MASTER;
byte coincident_pulse;
byte waiting_for_interrupt = 0;

#define COMMPIN 6

void setup() {
  analogReference (EXTERNAL);
  ADCSRA &= ~(bit (ADPS0) | bit (ADPS1) | bit (ADPS2));    // clear prescaler bits
  //ADCSRA |= bit (ADPS1);                                   // Set prescaler to 4  
  ADCSRA |= bit (ADPS0) | bit (ADPS1); // Set prescaler to 8
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);                               

  get_detector_name(detector_name);
  pinMode(3, OUTPUT); 
  pinMode(COMMPIN, INPUT);
  
  Serial.begin(9600);
  Serial.setTimeout(3000);

  if (digitalRead(COMMPIN) == HIGH){
     SLAVE = 1;
     MASTER = 0;
     // response: hold high for presence detection, then wait for low
    pinMode(COMMPIN, OUTPUT);
    digitalWrite(COMMPIN, HIGH);
    delay(1000);
    pinMode(COMMPIN, INPUT);
    while(digitalRead(COMMPIN) == HIGH) { continue; }
  }
  
  else{
     //delay(10);
     MASTER = 1;
    //  SLAVE = 0;
     pinMode(COMMPIN, OUTPUT);
     digitalWrite(COMMPIN,HIGH); // hold high for 1s
     delay(1000);
     pinMode(COMMPIN, INPUT); // check for presence of 2nd detector
     SLAVE = (digitalRead(COMMPIN) == HIGH) ? 1 : 0;
     pinMode(COMMPIN, OUTPUT); // prepare clock sync pulse
     digitalWrite(COMMPIN, HIGH);
     delay(1000);
     digitalWrite(COMMPIN, LOW); // clock sync on falling edge
    }
  
  start_time = millis();
  
  display.setRotation(2);         // Upside down screen (0 is right-side-up)
  OpeningScreen();                // Run the splash screen on start-up
  delay(2000);                    // Delay some time to show the logo, and keep the Pin6 HIGH for coincidence
  display.setTextSize(1);

  Timer1.initialize(TIMER_INTERVAL);             // Initialise timer 1
  Timer1.attachInterrupt(timerIsr);              // attach the ISR routine
  // if (!SD.begin(SDPIN)) {
  //   Serial.println(F("SD initialization failed!"));
  //   Serial.println(F("Is there an SD card inserted?"));
  //   panic();
  // }
  
  // get_Mode();
  // if (Mode == 2) read_from_SD();
  // else if (Mode == 3) remove_all_SD();
  // else{setup_files();}
  
  // if (MASTER == 1){digitalWrite(6,LOW);}
  analogRead(A0);
  
  total_deadtime += millis() - start_time;

}

void loop() {
  Serial.println(F("##########################################################################################"));
  Serial.println(F("### CosmicWatch: The Desktop Muon Detector"));
  Serial.println(F("### Questions? saxani@mit.edu"));
  Serial.println(F("### Comp_date Comp_time Event Ardn_time[ms] ADC[0-1023] SiPM[mV] Deadtime[ms] Temp[C] Name"));
  Serial.println(F("##########################################################################################"));
  Serial.println("Device ID: " + (String)detector_name);
   
  while (1){
    if (analogRead(A0) > SIGNAL_THRESHOLD){
      int adc = analogRead(A0);
      count++;

      if (MASTER == 1) {
          digitalWrite(COMMPIN, HIGH);
      }
      
      analogRead(A3);
      
      if (MASTER == 0) {
          if (digitalRead(COMMPIN) == HIGH) {
              coincident_pulse = 1;
              pinMode(COMMPIN, OUTPUT);
              digitalWrite(COMMPIN, HIGH);
          }
      }
      
      analogRead(A3);
      
      if (MASTER == 1 && SLAVE == 1){
          pinMode(COMMPIN, INPUT);
          if(digitalRead(COMMPIN) == HIGH) {
            coincident_pulse = 1;
          }
          pinMode(COMMPIN, OUTPUT);
      }

      analogRead(A3);

      if(MASTER == 0) {
        pinMode(COMMPIN, INPUT);
      }
      
      if(coincident_pulse == 1){
        int bitrate = 40 * 1000;
        int pulseLength = 1000000 / bitrate;
        //assemble 32 bit number to send to RPI
        /*
        32 bit int looks like:
        0000 0000 0000 0000 0000 0000 0000 0000
        
        For testing purposes, we will send this 32 bit number:
        4000, 357, 1000.
        12 bits, 10 bits, 10 bits,
        */
        uint32_t testInt1 = 4000<<20; //111110100000
        uint32_t testInt2 = 357<<10; //101100101
        uint32_t testInt3 = 1000; // 1111101000
        uint32_t dataOut = testInt3 | testInt2 | testInt1; //
        pinMode(COMMPIN, OUTPUT);
        digitalWrite(COMMPIN, 1);
        delayMicroseconds(100);
        pinMode(COMMPIN, INPUT);
        while(digitalRead(COMMPIN) == HIGH){continue;}
        //Send Data
        pinMode(COMMPIN, OUTPUT);
        for(int i = 0; i<32; i++){
          digitalWrite(COMMPIN, dataOut>>(31-i) & 1);
          delayMicroseconds(pulseLength);
        }
      }

      measurement_deadtime = total_deadtime;
      time_stamp = millis() - start_time;
      measurement_t1 = micros();  

      coincident_count += coincident_pulse;

      temperatureC = (((analogRead(A3)+analogRead(A3)+analogRead(A3))/3. * (3300./1024)) - 500)/10. ;

      if((interrupt_timer + 1000 - millis()) < 15){ 
          waiting_t1 = millis();
          waiting_for_interrupt = 1;
          delay(30);
          waiting_for_interrupt = 0;}

      digitalWrite(6, LOW); 
      analogWrite(3, LED_BRIGHTNESS);
      Serial.println((String)count + " " + time_stamp+ " " + adc+ " " + get_sipm_voltage(adc)+ " " + measurement_deadtime+ " " + temperatureC + " " + coincident_pulse);
      // myFile.println((String)count + " " + time_stamp+ " " + adc+ " " + get_sipm_voltage(adc)+ " " + measurement_deadtime+ " " + temperatureC + " " + coincident_pulse);
      // myFile.flush();
      
      last_adc_value = adc;
      last_sipm_voltage = get_sipm_voltage(adc);
              
      coincident_pulse = 0;
      digitalWrite(3, LOW);
      while(analogRead(A0) > RESET_THRESHOLD){continue;}
      
      total_deadtime += (micros() - measurement_t1) / 1000.;
    }
  }
}

void timerIsr() 
{
  interrupts();
  interrupt_timer = millis();
  if (waiting_for_interrupt == 1) {
      total_deadtime += (millis() - waiting_t1);}
  waiting_for_interrupt = 0;
  get_time();
}

void get_time() 
{
  unsigned long int OLED_t1             = micros();
  float count_average                   = 0;
  float count_std                       = 0;
  float lcount = SLAVE ? coincident_count : count;

  if (lcount > 0.) {
      count_average   = lcount / ((interrupt_timer - start_time - total_deadtime) / 1000.);
      count_std       = sqrt(lcount) / ((interrupt_timer - start_time - total_deadtime) / 1000.);}
  else {
      count_average   = 0;
      count_std       = 0;}
  
  display.setCursor(0, 0);
  display.clearDisplay();
  if(SLAVE) {
    display.print(F("Count: "));
    display.print(coincident_count);
    display.print(F(" of "));
    display.println(count);
  } else {
  display.print(F("Total Count: "));
  display.println(count);
  display.print(F("Uptime: "));
  }

  int minutes                 = ((interrupt_timer - start_time) / 1000 / 60) % 60;
  int seconds                 = ((interrupt_timer - start_time) / 1000) % 60;
  char min_char[4];
  char sec_char[4];
  
  sprintf(min_char, "%02d", minutes);
  sprintf(sec_char, "%02d", seconds);

  display.println((String) ((interrupt_timer - start_time) / 1000 / 3600) + ":" + min_char + ":" + sec_char);

  if (count == 0) {
    display.println("Hi, I'm "+(String)detector_name);
    }
      //if (MASTER == 1) {display.println(F("::---  MASTER   ---::"));}
      //if (SLAVE  == 1) {display.println(F("::---   SLAVE   ---::"));}}
      
  else{
      if (last_sipm_voltage > 180){
          display.print(F("===---- WOW! ----==="));}
      else{
            if (MASTER == 1) {display.print(F("M"));}
            else {display.print(F("S"));}
            for (int i = 1; i <=  (last_sipm_voltage + 10) / 10; i++) {display.print(F("-"));}}
      display.println(F(""));}

  char tmp_average[4];
  char tmp_std[4];

  int decimals = 2;
  if (count_average < 10) {decimals = 3;}
  
  dtostrf(count_average, 1, decimals, tmp_average);
  dtostrf(count_std, 1, decimals, tmp_std);
   
  display.print(F("Rate: "));
  display.print((String)tmp_average);
  display.print(F("+/-"));
  display.println((String)tmp_std);
  display.display();
  
  total_deadtime                      += (micros() - OLED_t1 +73)/1000.;
}

void OpeningScreen(void) 
{
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(8, 0);
    display.clearDisplay();
    display.print(F("Cosmic \n     Watch"));
    display.display();
    display.setTextSize(1);
    display.clearDisplay();
}

float get_sipm_voltage(float adc_value){
  float voltage = 0;
  for (int i = 0; i < (sizeof(cal)/sizeof(float)); i++) {
    voltage += cal[i] * pow(adc_value,(sizeof(cal)/sizeof(float)-i-1));
    }
    return voltage;
}

boolean get_detector_name(char* det_name) 
{
    byte ch;                              // byte read from eeprom
    int bytesRead = 0;                    // number of bytes read so far
    ch = EEPROM.read(bytesRead);          // read next byte from eeprom
    det_name[bytesRead] = ch;               // store it into the user buffer
    bytesRead++;                          // increment byte counter

    while ( (ch != 0x00) && (bytesRead < 40) && ((bytesRead) <= 511) ) 
    {
        ch = EEPROM.read(bytesRead);
        det_name[bytesRead] = ch;           // store it into the user buffer
        bytesRead++;                      // increment byte counter
    }
    if ((ch != 0x00) && (bytesRead >= 1)) {det_name[bytesRead - 1] = 0;}
    return true;
}

void panic(void) {
  analogWrite(3, LED_BRIGHTNESS);
  while(1) { continue; }
}
