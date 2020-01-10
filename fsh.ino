#include "Wire.h"
#include "SD.h" //Lib to read SD card
#include "TMRpcm.h" //Lib to play auido
#include "SPI.h" //SPI lib for SD card
#include "SparkFun_MAG3110.h"

TMRpcm audio; //Lib object is named "audio"
MAG3110 mag = MAG3110();

//Setting up directory to access files on SD card
//Storing file names in program space to save memory (all these strings are quite large!)
const char zero[] PROGMEM = "0.wav";
const char one[] PROGMEM = "1.wav";
const char two[] PROGMEM = "2.wav";
const char three[] PROGMEM = "3.wav";
const char four[] PROGMEM = "4.wav";
const char five[] PROGMEM = "5.wav";
const char six[] PROGMEM = "6.wav";
const char seven[] PROGMEM = "7.wav";
const char eight[] PROGMEM = "8.wav";
const char nine[] PROGMEM = "9.wav";
const char feet[] PROGMEM = "feet.wav";
const char degree[] PROGMEM = "degree.wav";

const char* const file_table[] PROGMEM = {zero, one, two, three, four, five, six,
seven, eight, nine, feet, degree};

char filename[15];



int distance = 0;
int heading;



unsigned int IR_LOW = 1010; //constant value for IR sensor reading on wheel
unsigned int IR_HIGH=1022; //constant value for IR sensor reading on tape
unsigned int IR_NOISE_MARGIN = 3; //constant value to provide margin on IR readings

//IR_receiver is connected to pin A0
unsigned char* pDDRC = (unsigned char*) 0x27;
unsigned char* pADMUX = (unsigned char*) 0x7C;
unsigned char* pADCSRA = (unsigned char*) 0x7A;
unsigned char* pADCSRB = (unsigned char*) 0x7B;
unsigned char* pDIDR0 = (unsigned char*) 0x7E;

unsigned short* ADCData = (unsigned short*) 0x78; //ADC data register
unsigned short ADCVal; //Variable to hold ADCData

//Flags used to determine IR receiver has previously read Wheel and/or Tape
bool onWheel;
bool onTape;


void setup(){
audio.speakerPin = 9; //Audio out on pin 9
Serial.begin(9600); //Serial Com for debugging 
if (!SD.begin()) {
  Serial.println("SD fail");
  return;
}

audio.setVolume(5);    //   0 to 7. Set volume level
audio.quality(1);        //  Set 1 for 2x oversampling Set 0 for normal

mag.initialize();


//First pin change interupt                    
DDRD&=~(0x04); //set direction as inputs
PCICR=0x04; //0000 0100 setting the pinchange interupt for pins 16-23 
PCMSK2=0x04; //0000 0100 setting just pin18 (arduino pin 2)

//Second pin change interupt
DDRC&=~(0x02); //set direction as inputs
PCICR|=0x02; //0000 0010 setting the pinchange interupt for pins 8-14
PCMSK1=0x02; //0000 0010 setting just pin8 (arduino pin A1)

//Third pin change interupt
DDRB&=~(0x01); //set direction as inputs
PCICR|=0x01; //0000 0001 setting the pinchange interupt for pins 0-7 
PCMSK0=0x01; //0000 0001 setting just pin0 (arduino pin 8)

if(!mag.isCalibrated()) //If we're not calibrated
  {
   if(!mag.isCalibrating()) //And we're not currently calibrating
    {
 Serial.println("Entering calibration mode");
     mag.enterCalMode(); //This sets the output data rate to the highest possible and puts the mag sensor in active mode
   }
 else
   {
     //Must call every loop while calibrating to collect calibration data
     //This will automatically exit calibration
      //You can terminate calibration early by calling mag.exitCalMode();
 mag.calibrate(); 
   }
 }
 else
 {
   Serial.println("Calibrated!");
 } 


//set up ADC registers
*pDDRC &= (unsigned char) ~(0x01); //pin A0 set to INPUT
*pADMUX = (unsigned char) 0x40; // ADC input 0, Right adjusted
*pADCSRA =  (unsigned char) 0xE4; //Free Running mode, pre scaler /16
*pADCSRB = (unsigned char) 0x00; //Free Running Mode
*pDIDR0 &= (unsigned char) ~(0x01); //Disable digital input on pin A0



}


//isHeading indicates whether it is distance (0) or heading (1)
void playback(unsigned int value, bool isHeading) {
  int ones = 0;
  int tens = 0;
  int hundreds = 0;
  
  if (isHeading) {
    
    //Separate value into ones, tens, and hundreds places to be played back
    //independently
    ones = value % 10;
    tens = int(value / 10) % 10;
    hundreds = int(value / 100);

    //retrieve hundreds place filename from program memory and play
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[hundreds])));
    audio.play(filename);
    while(audio.isPlaying()) {}
    //wait until file is done playing before playing next one.
    
    //retrieve tens place filename from program memory
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[tens])));
    audio.play(filename);
    while(audio.isPlaying()) {}
    
    //retrieve ones place filename from program memory
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[ones])));
    audio.play(filename);
    while(audio.isPlaying()) {}
    
    //retrieve "degree" file from program memory
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[11])));
    audio.play(filename);
    while(audio.isPlaying()) {}
    
  } else { //value to be played back is a distance reading
    
    if (value > 99) {
      hundreds = int(value / 100);
      strcpy_P(filename, (char*)pgm_read_word(&(file_table[hundreds])));
      audio.play(filename);
      while(audio.isPlaying()) {}
    }
    
    if (value > 9) {
      tens = int(value / 10) % 10;
      strcpy_P(filename, (char*)pgm_read_word(&(file_table[tens])));
      audio.play(filename);
      while(audio.isPlaying()) {}
    }

    ones = value % 10;
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[ones])));
    audio.play(filename);
    while(audio.isPlaying()) {}

    //retrieve "feet" file from program memory
    strcpy_P(filename, (char*)pgm_read_word(&(file_table[10])));
    audio.play(filename);
    while(audio.isPlaying()) {}
     
  }
}


void loop() {

  int x, y, z;
  mag.readMag(&x, &y, &z);

  //This is not true magnetic compass heading, but can be
  //used to somewhat determine orientation
  heading = (abs(x)/10);

  
  
  measureDistance();
  
}

  

void measureDistance() {
  ADCVal = (*ADCData & 0x3FF); //Take ADC Reading

  if ( (ADCVal > (IR_LOW - IR_NOISE_MARGIN) && (ADCVal < IR_LOW + IR_NOISE_MARGIN) ) ) {
    onWheel = true;
  }
  if ( (ADCVal > (IR_HIGH - IR_NOISE_MARGIN) && (ADCVal < IR_HIGH + IR_NOISE_MARGIN) ) ) {
    onTape = true;
  }
  if ( (ADCVal > (IR_LOW - IR_NOISE_MARGIN) && (ADCVal < IR_LOW + IR_NOISE_MARGIN) ) && onWheel && onTape) {
    onWheel = false;
    onTape = false;
    distance++;
  }
  return;
}


//ISR Functions


//First (Reset button, setting distance to 0), (arduino pin 2)
ISR(PCINT2_vect){
  cli();
  if((PIND & 0x04) == 0x04){ //When pushing the button the ISR is only triggered once
    Serial.println("Reset");
    distance=0;
  }
  sei();
}

//Second (Distance button... Reads out the distance), (arduino pin A1)
ISR(PCINT1_vect){
  cli();
  if((PINC & 0x02) == 0x02){ //When pushing the button the ISR is only triggered once
    Serial.print("Distance ");
    Serial.println(distance);
    playback(distance, 0);
  }
  sei();
}

//Third (Reads out the Header (direction) from magtometer), (arduino pin 8)
ISR(PCINT0_vect){
  cli();
  if((PINB & 0x01) == 0x01){ //When pushing the button the ISR is only triggered once
    Serial.print("Heading: ");
    Serial.println(heading);
  
    Serial.println("--------");
    playback(heading, 1);
  }
  sei(); 
}

  




