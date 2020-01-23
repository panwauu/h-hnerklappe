// https://www.makerguides.com/tutorials/a4988-stepper-motor-driver-arduino/
// https://www.intorobotics.com/nema-17-arduino-a4988/
// https://playground.arduino.cc/Main/LM35HigherResolution/

/*
ANSCHLÜSSE MOTORBOX KABEL:
GRAUN: *
ROT: Endschalter oben
ORANGE: 12V
GELB: Endschalter unten
GRÜN: GND (12V)????????????
BLAU: Sleep 
LILA: 5V
GRAU: Direction
WEISS: Step
SCHWARZ: GND (5V)

ANSCHLÜSSE BOX ARDUINO INNEN:
ORANGE: 12V
GRAU: GND(12V)????????????????
ROT: 5V
SCHWARZ: GND
GELB TAPE: STEP
WEISS TAPE: ENDSCHALTER OBEN
WEISS: ENDSCHALTER UNTEN
GRAU TAPE: Sleep
GELB: Direction

*/

#include <AccelStepper.h>
#include <EEPROM.h>
#include <DS3231.h>
DS3231 rtc(SDA,SCL);

//Define stepper motor connections
#define dirPin 5
#define stepPin 6
#define sleepPin 7
#define GESCHWINDIGKEIT 400

//Define Endschalter Pins
#define EndOben A2
#define EndObenAnschlag 1 // Wert des EndOben bei oberem Anschlag
#define EndUnten A3
#define EndUntenAnschlag 1 // Wert des EndUnten bei unterem Anschlag

//Define motorState names
int motorState = 0;
#define RUHE 0
#define HOCHFAHREN 1
#define RUNTERFAHREN 2
#define HALTEN 3

//Define SOllwerte
#define OBEN 1
#define UNTEN 0
int Sollposition = 0;

//Define Temperatursensor
#define LM35 A1
int TemperaturWerte[4] = {0,0,0,0};
unsigned long prevTimeTemp;
float TempMittel;

//Define Photoresistor
#define PhotoPin A0
unsigned long prevTimeLicht;
#define LichtMessungInterval 1000
int PhotoSchwelle = 100;
int ZaehlerPhoto = 0;
boolean LichtZustand = 0;
#define HELL 1
#define DUNKEL 0

//Define Betriebsmodus
byte betriebsModus = 0;
#define AUS 0
#define AUTO_LICHT 1
#define MAN 2
#define AUTO_ZEIT 3

//Create stepper object
AccelStepper stepper(1,stepPin,dirPin); //motor interface type must be set to 1 when using a driver.


/*********** DISPLAY ***********/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
#define DS3231_I2C_ADDRESS 0x64

Time t;
byte h_frueh;
byte m_frueh;
byte h_spaet;
byte m_spaet;

int stateMenu = 0;
unsigned long prevTimeAction;
boolean DisplayChange = true;
boolean DisplayOn = true;
/********* DISPLAY ENDE ********/

/*********** ENCODER *********/
volatile boolean TurnDetected;
volatile boolean buttonDetected;
volatile int up;

unsigned long prevTimeBut;
unsigned long prevTimeEnc;

const int PinCLK=2;                   // Used for generating interrupts using CLK signal
const int PinDT=4;                    // Used for reading DT signal
const int PinSW=3;                    // Used for the push button switch

void isr_Enc (){
  if(millis()-prevTimeEnc > 300){
    if (digitalRead(PinDT)){
      up++;
    }
    else{
      up--;
    }
    prevTimeEnc = millis();
    prevTimeAction = millis();
    TurnDetected = true;
  }
}

void isr_But ()  {                    // Interrupt service routine is executed when a HIGH to LOW transition is detected on CLK
  if(!buttonDetected && ((millis()-prevTimeBut) < 300)){
    buttonDetected = false;
  }
  else{
    buttonDetected = true;
    prevTimeBut = millis();
    prevTimeAction = millis();
  }
}
/********** ENCODER ENDE *****/


void setup()
{
  // 1,1V INTERNAL AREF
  // analogReference(INTERNAL);
  pinMode(LM35,INPUT);
  
  // Configurate Stepper
  stepper.setMaxSpeed(1000); //maximum steps per second
  pinMode(sleepPin,OUTPUT); // Sleep Mode als Ausgang

  pinMode(EndOben,INPUT);// Endschalter als Input
  pinMode(EndUnten,INPUT);// Endschalter als Input
  digitalWrite(EndOben, HIGH); // turn on pullup resistors
  digitalWrite(EndUnten, HIGH); // turn on pullup resistors

  pinMode(PhotoPin,INPUT);

  // Set time
  prevTimeLicht = millis();
  prevTimeAction = millis();
  prevTimeEnc = millis();
  prevTimeTemp = millis();

  // AUS EEPROM lesen
  EEPROM.get(0, PhotoSchwelle);
  EEPROM.get(2, h_frueh);
  EEPROM.get(3, m_frueh);
  EEPROM.get(4, h_spaet);
  EEPROM.get(5, m_spaet);
  EEPROM.get(6, betriebsModus);

  /*********** ENCODER *********/
  prevTimeBut = millis();
  pinMode(PinCLK,INPUT);
  pinMode(PinDT,INPUT);  
  pinMode(PinSW,INPUT);
  digitalWrite(PinSW,HIGH);
  attachInterrupt (digitalPinToInterrupt(PinCLK),isr_Enc, FALLING);   // interrupt 0 is always connected to pin 2 on Arduino UNO
  attachInterrupt(digitalPinToInterrupt(3), isr_But, FALLING);
  /********** ENCODER ENDE *****/

  /*********** DISPLAY ***********/
  lcd.init();                      // initialize the lcd 
  /********* DISPLAY ENDE ********/

// Initialize the rtc object
  rtc.begin();
  //rtc.setTime(21,47,45);     // Set the time to 12:00:00 (24hr format)
  t = rtc.getTime();

  Serial.begin(9600);
}

// Sollposition bestimmen
void get_sollPosition(){
  // BETRIEBSMODI ABFRAGEN
  if (betriebsModus == AUTO_LICHT){
    Lichtauslesen();
    Sollposition = LichtZustand;
  }
  else if (betriebsModus == AUTO_ZEIT){
      // Zeit auslesen
      t = rtc.getTime();
      byte h = t.hour;
      byte m = t.min;
      
      if(  ((h == h_frueh && m >= m_frueh)||(h>h_frueh)) && ((h == h_spaet && m <= m_spaet)||(h<h_spaet))  ){
        Sollposition = OBEN;
      }
      else{
        Sollposition = UNTEN;
      }
  }
  else if (betriebsModus == AUS){
    Sollposition = -1;
  }
}


// Bestimmen des gewünschten Zustandes
void ZustandsBestimmung(){
  boolean SchalterObenWert = digitalRead(EndOben);
  boolean SchalterUntenWert = digitalRead(EndUnten);
  
  if (Sollposition == OBEN && SchalterObenWert != EndObenAnschlag){
    motorState = HOCHFAHREN;
  }
  else if (Sollposition == OBEN && SchalterObenWert == EndObenAnschlag){
    motorState = HALTEN;
  }
  else if (Sollposition == UNTEN && SchalterUntenWert != EndUntenAnschlag){
    motorState = RUNTERFAHREN;
  }
  else if (Sollposition == UNTEN && SchalterUntenWert == EndUntenAnschlag){
    motorState = RUHE;
  }
  else{
    motorState = RUHE;
  }
}

// Ansteuerung des Steppers
void motorAnsteuerung(){
  switch(motorState){
    case RUHE:
      digitalWrite(sleepPin,LOW);
    break;
    case HOCHFAHREN:
      digitalWrite(sleepPin,HIGH);
      stepper.setSpeed(GESCHWINDIGKEIT); //steps per second
      stepper.runSpeed(); //step the motor with constant speed as set by setSpeed()
    break;
    case RUNTERFAHREN:
      digitalWrite(sleepPin,HIGH);
      stepper.setSpeed(-GESCHWINDIGKEIT); //steps per second
      stepper.runSpeed(); //step the motor with constant speed as set by setSpeed()
    break;
    case HALTEN:
      digitalWrite(sleepPin,HIGH);
      stepper.setSpeed(0); //steps per second
      stepper.runSpeed(); //step the motor with constant speed as set by setSpeed()
    break;
    default: //Nicht vorgesehener Fall, Fail-Safe
      digitalWrite(sleepPin,LOW);
    break;
  }
}

// Funktion Temperatur auslesen
void Temperaturauslesen(){
  if ((millis()-prevTimeTemp > 1000) && (motorState != HOCHFAHREN) && (motorState != RUNTERFAHREN)){
      // Temperaturwert auslesen und mitteln über die letzten 5 Werte
    int WertTemp = analogRead(LM35);
    long Zwischen = WertTemp + TemperaturWerte[1] + TemperaturWerte[2] + TemperaturWerte[3] + TemperaturWerte[0];
    TempMittel = (float)Zwischen *100.0/1024.0;
    TemperaturWerte[0] = TemperaturWerte[1];
    TemperaturWerte[1] = TemperaturWerte[2];
    TemperaturWerte[2] = TemperaturWerte[3];
    TemperaturWerte[3] = WertTemp;
  
    DisplayChange = true;
    prevTimeTemp = millis();
  }
}

// Lichtlevel auslesen
void Lichtauslesen(){
  // Messung nur alle x Millisekunden
  if ((millis()-prevTimeLicht)> LichtMessungInterval){
    prevTimeLicht = millis();
    // SENSOR AUSLESEN - HELL niedrige Werte, DUNKEL hohe Werte
    int PhotoValue = analogRead(PhotoPin);
    if (PhotoValue < PhotoSchwelle && LichtZustand == HELL){ // DUNKEL
      ZaehlerPhoto++;
    }
    else if (PhotoValue > PhotoSchwelle && LichtZustand == DUNKEL){ // HELL
      ZaehlerPhoto++;
    }
    else{
      ZaehlerPhoto = 0;
    }
    
    if (ZaehlerPhoto > 6){
      LichtZustand = !LichtZustand;
      ZaehlerPhoto = 0;
    }
  }
}

// FUNKTION FUER LCD MENU
void display_Anzeige(){
  // DISPLAY AUS
  if (!DisplayOn && (buttonDetected || TurnDetected)){
    DisplayOn = true;
    buttonDetected = false;
    TurnDetected = false;
    lcd.noBacklight();
  }
  else if(DisplayOn){ // DISPLAY AN
    lcd.backlight();
    if(millis() - prevTimeAction > 10000){
      DisplayOn = false;
      stateMenu = 0;
    }
    switch(stateMenu){
      case 0:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print("Temp:           ");
          lcd.setCursor(12,0);
          char LCDmsg[4];
          dtostrf(TempMittel, 4, 1, LCDmsg);
          lcd.print(LCDmsg);
          lcd.setCursor(0,1);
          lcd.print("Mode:           ");
          DisplayChange = false;
          if (betriebsModus == AUTO_LICHT){
            lcd.setCursor(6,1);
            lcd.print("AUTO_LICHT");
          }
          else if (betriebsModus == AUTO_ZEIT){
            lcd.setCursor(7,1);
            lcd.print("AUTO_ZEIT");
          }
          else if (betriebsModus == MAN){
            lcd.setCursor(8,1);
            lcd.print("MAN(   )");
            if (Sollposition == OBEN){
              lcd.setCursor(12,1);
              lcd.print("AUF");
            }
            else if(Sollposition == UNTEN){
              lcd.setCursor(12,1);
              lcd.print(" ZU");
            }
          }
          else{
            lcd.setCursor(13,1);
            lcd.print("AUS");
          }
        }
        if (buttonDetected){
          stateMenu = 1;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          if (betriebsModus == MAN){
            Sollposition = OBEN;
            DisplayChange = true; 
           }
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
           if (betriebsModus == MAN){
            Sollposition = UNTEN;
            DisplayChange = true; 
           }
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 1:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Betriebsmodus  ");
          lcd.setCursor(0,1);
          lcd.print(" Lichtsettings  ");
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 21;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          DisplayChange = false; 
          TurnDetected = false;  
          up = 0;      
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 2;
          DisplayChange = true;  
          TurnDetected = false;
          up = 0;       
        }
      break;
      case 2:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Lichtsettings  ");
          lcd.setCursor(0,1);
          lcd.print(" Zeitsettings   ");
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 31;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 1;
          DisplayChange = true;
          TurnDetected = false;
          up = 0;   
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 3;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 3:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Zeitsettings   ");
          lcd.setCursor(0,1);
          lcd.print(" ZURUECK        ");
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 41;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 2;
          DisplayChange = true;
          TurnDetected = false;
          up = 0;   
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 4;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 4:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(" Zeitsettings   ");
          lcd.setCursor(0,1);
          lcd.print(">ZURUECK        ");
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 3;
          DisplayChange = true;
          TurnDetected = false;  
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          DisplayChange = false;
          TurnDetected = false; 
          up = 0;     
        }
      break;
      case 21:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Lichtautomatik ");
          lcd.setCursor(0,1);
          lcd.print(" Zeitautomatik  ");
          DisplayChange = false;
        }
        if (buttonDetected){
          betriebsModus = AUTO_LICHT;
          EEPROM.put(6, betriebsModus);
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          DisplayChange = false; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 22;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 22:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Zeitautomatik  ");
          lcd.setCursor(0,1);
          lcd.print(" Handbetrieb    ");
          DisplayChange = false;
        }
        if (buttonDetected){
          betriebsModus = AUTO_ZEIT;
          EEPROM.put(6, betriebsModus);
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 21;
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 23;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 23:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Handbetrieb    ");
          lcd.setCursor(0,1);
          lcd.print(" AUS            ");
          DisplayChange = false;
        }
        if (buttonDetected){
          betriebsModus = MAN;
          EEPROM.put(6, betriebsModus);
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 22;
          DisplayChange = true; 
          TurnDetected = false;
          up = 0;        
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 24;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 24:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(" Handbetrieb    ");
          lcd.setCursor(0,1);
          lcd.print(">AUS            ");
          DisplayChange = false;
        }
        if (buttonDetected){
          betriebsModus = AUS;
          EEPROM.put(6, betriebsModus);
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 23;
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          DisplayChange = false;  
          TurnDetected = false;   
          up = 0;    
        }
      break;
      case 31:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print("Lichtlevel:     ");
          lcd.setCursor(12,0);
          lcd.print(analogRead(PhotoPin));
          lcd.setCursor(0,1);
          lcd.print("                ");
          lcd.setCursor(6,1);
          lcd.print(String(PhotoSchwelle));
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 0;
          buttonDetected = false;
          DisplayChange = true;  
          //// DAUERHAFT IN EEPROM SPEICHERN   
          EEPROM.put(0, PhotoSchwelle);
        }
        else if (TurnDetected && up >= 1){
          if(PhotoSchwelle + abs(up)*10 <= 1023){
            PhotoSchwelle += abs(up)*10;
          }
          else{
            PhotoSchwelle = 1023;
          }
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;   
        }
        else if (TurnDetected && up <= -1){
          if(PhotoSchwelle - abs(up)*10 >= 0){
            PhotoSchwelle -= abs(up)*10;
            DisplayChange = true;
          }
          else{
            PhotoSchwelle = 0;
          }
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;   
        }
      break;
      case 41:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Uhrzeit        ");
          lcd.setCursor(0,1);
          lcd.print(" Zeit morgens   ");
          DisplayChange = false;
        }
        if (buttonDetected){
          buttonDetected = false;
          if( (motorState != HOCHFAHREN) && (motorState != RUNTERFAHREN) ){
            t = rtc.getTime();
            byte stunde_temp = t.hour;
            byte minute_temp = t.min;
            Zeitabfrage(&stunde_temp , &minute_temp);
            rtc.setTime(stunde_temp,minute_temp,00);     // Set the time to 12:00:00 (24hr format)
          }
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          DisplayChange = false; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 42;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 42:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Zeit morgens   ");
          lcd.setCursor(0,1);
          lcd.print(" Zeit abends    ");
          DisplayChange = false;
        }
        if (buttonDetected){
          buttonDetected = false;
          if( (motorState != HOCHFAHREN) && (motorState != RUNTERFAHREN) ){
            Zeitabfrage(&h_frueh , &m_frueh);
            EEPROM.put(2, h_frueh);
            EEPROM.put(3, m_frueh);

          }
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 41;
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 43;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 43:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(">Zeit abends    ");
          lcd.setCursor(0,1);
          lcd.print(" ZURUECK        ");
          DisplayChange = false;
        }
        if (buttonDetected){
          buttonDetected = false;
          if( (motorState != HOCHFAHREN) && (motorState != RUNTERFAHREN) ){
            Zeitabfrage(&h_spaet , &m_spaet);
            EEPROM.put(4, h_spaet);
            EEPROM.put(5, m_spaet);
          }
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 42;
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          stateMenu = 44;
          DisplayChange = true;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
      case 44:
        if (DisplayChange){
          lcd.setCursor(0,0);
          lcd.print(" Zeit abends    ");
          lcd.setCursor(0,1);
          lcd.print(">ZURUECK        ");
          DisplayChange = false;
        }
        if (buttonDetected){
          stateMenu = 3;
          buttonDetected = false;
          DisplayChange = true;        
        }
        else if (TurnDetected && up >= 1){
          stateMenu = 43;
          DisplayChange = true; 
          TurnDetected = false; 
          up = 0;       
        }
        else if (TurnDetected && up <= -1){
          DisplayChange = false;  
          TurnDetected = false;  
          up = 0;     
        }
      break;
    }
  }
  else{
    lcd.noBacklight();
  }
}

void Zeitabfrage(byte* stunde, byte* minute){
  byte neu_stunde = *stunde;
  byte neu_minute = *minute;
  unsigned long prevTimeSet = millis();
  boolean flag_SetTime = false;

  
  lcd.setCursor(0,0);
  lcd.print(">Stunde:        ");
  lcd.setCursor(0,1);
  lcd.print(" Minute:        ");
   
  while(buttonDetected == false && ((millis()-prevTimeSet)<15000) ){ 
    if (neu_stunde < 10){
      lcd.setCursor(14,0);
      lcd.print(" ");  
      lcd.setCursor(15,0);
      lcd.print(neu_stunde);        
    }
    else{
      lcd.setCursor(14,0);
      lcd.print(neu_stunde);      
    }
    if (neu_minute < 10){
      lcd.setCursor(14,1);
      lcd.print(" ");  
      lcd.setCursor(15,1);
      lcd.print(neu_minute);        
    }
    else{
      lcd.setCursor(14,1);
      lcd.print(neu_minute);      
    }
    if (TurnDetected && up <= -1){
      if(neu_stunde < 23){
        neu_stunde++;
      }
      else{
        neu_stunde = 0;
      }
      TurnDetected = false; 
      up = 0;       
    }
    else if (TurnDetected && up >= 1){
      if(neu_stunde > 0){
        neu_stunde--;
      }
      else{
        neu_stunde = 23;
      }
      TurnDetected = false;  
      up = 0;     
    }  
  }
  if (buttonDetected == true){
    buttonDetected = false;
    flag_SetTime = true;
  }
  else{
    flag_SetTime = false;
  }

  lcd.setCursor(0,0);
  lcd.print(" Stunde:        ");
  lcd.setCursor(0,1);
  lcd.print(">Minute:        ");
  prevTimeSet = millis();
  while(buttonDetected == false && ((millis()-prevTimeSet)<15000) && flag_SetTime){ 
    if (neu_stunde < 10){
      lcd.setCursor(14,0);
      lcd.print(" ");  
      lcd.setCursor(15,0);
      lcd.print(neu_stunde);        
    }
    else{
      lcd.setCursor(14,0);
      lcd.print(neu_stunde);      
    }
    if (neu_minute < 10){
      lcd.setCursor(14,1);
      lcd.print(" ");  
      lcd.setCursor(15,1);
      lcd.print(neu_minute);        
    }
    else{
      lcd.setCursor(14,1);
      lcd.print(neu_minute);      
    }
    if (TurnDetected && up <= -1){
      if(neu_minute < 55){
        neu_minute = neu_minute+5;
      }
      else{
        neu_minute = 0;
      }
      TurnDetected = false; 
      up = 0;       
    }
    else if (TurnDetected && up >= 1){
      if(neu_minute >= 5){
        neu_minute = neu_minute - 5;
      }
      else{
        neu_minute = 55;
      }
      TurnDetected = false;  
      up = 0;     
    }  
  }
  if (buttonDetected == true){
    buttonDetected = false;
    flag_SetTime = true;
  }
  else{
    flag_SetTime = false;
  }

  if (flag_SetTime == true){
    *stunde = neu_stunde;
    *minute = neu_minute;
  }
}

void loop()
{
  // Sollposition bestimmen
  get_sollPosition();

  // Motor Bewgung aufgrund der Sollposition Bestimmen
  ZustandsBestimmung();
  motorAnsteuerung();
  
  // Temperatursensor abfragen
  Temperaturauslesen();

  //Serial.println(analogRead(LM35) *500.0/1024.0);
  

  // Display bedienen
  display_Anzeige();

}
