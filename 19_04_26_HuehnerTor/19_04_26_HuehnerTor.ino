// https://www.makerguides.com/tutorials/a4988-stepper-motor-driver-arduino/
// https://www.intorobotics.com/nema-17-arduino-a4988/
// https://playground.arduino.cc/Main/LM35HigherResolution/

/*
ANSCHLÜSSE MOTORBOX:
GRAUN: *
ROT: Endschalter oben
ORANGE: 12V
GELB: Endschalter unten
GRÜN: GND (12V)
BLAU: Sleep 
LILA: 5V
GRAU: Direction
WEISS: Step
SCHWARZ: GND (5V)
*/

#include <AccelStepper.h>

//Define stepper motor connections
#define dirPin 2
#define stepPin 3
#define sleepPin 4
#define GESCHWINDIGKEIT 400

//Define Endschalter Pins
#define EndOben 8
#define EndObenAnschlag 1 // Wert des EndOben bei oberem Anschlag
#define EndUnten 7
#define EndUntenAnschlag 1 // Wert des EndUnten bei unterem Anschlag

//Define Pin für TM35

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
//#define LM35 A0
//int TemperaturWerte[] = {0,0,0,0};

//Define Photoresistor
#define PhotoPin A0
unsigned long prevTimeLicht;
#define LichtMessungInterval 1000
int PhotoSchwelle = 100;
int ZaehlerPhoto = 0;
boolean LichtZustand = 0;
#define HELL 1
#define DUNKEL 0

//Define Steuerschalter
#define SchalterManPin A3
#define SchalterDirPin A5


//Create stepper object
AccelStepper stepper(1,stepPin,dirPin); //motor interface type must be set to 1 when using a driver.


void setup()
{
  // Configurate Stepper
  stepper.setMaxSpeed(1000); //maximum steps per second
  pinMode(sleepPin,OUTPUT); // Sleep Mode als Ausgang

  pinMode(EndOben,INPUT);// Endschalter als Input
  pinMode(EndUnten,INPUT);// Endschalter als Input
  digitalWrite(EndOben, HIGH); // turn on pullup resistors
  digitalWrite(EndUnten, HIGH); // turn on pullup resistors

  pinMode(PhotoPin,INPUT);

  pinMode(SchalterManPin,INPUT);
  digitalWrite(SchalterManPin, HIGH);
  pinMode(SchalterDirPin,INPUT); //PULLUP
  digitalWrite(SchalterDirPin, HIGH); //PULLUP

  // Set time
  prevTimeLicht = millis();

  Serial.begin(9600);
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

/* Funktion Temperatur auslesen
void Temperaturauslesen(){
  // Temperaturwert auslesen und mitteln über die letzten 5 Werte
  int WertTemp = analogRead(LM35);
  int Zwischen = WertTemp + TemperaturWerte[1] + TemperaturWerte[2] + TemperaturWerte[3] + TemperaturWerte[4];
  float TempMittel = (float)Zwischen *500.0/1024.0;
  TemperaturWerte[1] = TemperaturWerte[2];
  TemperaturWerte[2] = TemperaturWerte[3];
  TemperaturWerte[3] = TemperaturWerte[4];
  TemperaturWerte[4] = WertTemp;  
}*/

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

void loop()
{
  boolean SchalterManValue = digitalRead(SchalterManPin);
  if (SchalterManValue == 0){// AutoModus
    Sollposition = LichtZustand;
    Lichtauslesen();
  }
  else{ // MANUELLLER MODUS
    ZaehlerPhoto = 0;
    boolean SchalterDirValue = digitalRead(SchalterDirPin);
    if (SchalterDirValue == 0){
      Sollposition = OBEN;
    }
    else{
      Sollposition = UNTEN;
    }
  }
  
  ZustandsBestimmung();
  motorAnsteuerung();
}
