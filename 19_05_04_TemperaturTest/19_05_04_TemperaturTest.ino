//Define Temperatursensor
#define LM35 A1
int TemperaturWerte[4] = {0,0,0,0};
unsigned long prevTimeTemp;
float TempMittel;


void setup() {
  // put your setup code here, to run once:
  // 1,1V INTERNAL AREF
  // analogReference(INTERNAL);
  pinMode(LM35,INPUT);
  prevTimeTemp = millis();

  Serial.begin(9600);
}

void Temperaturauslesen(){
  if ((millis()-prevTimeTemp > 1000)){
      // Temperaturwert auslesen und mitteln über die letzten 5 Werte
    int WertTemp = analogRead(LM35);
    long Zwischen = WertTemp + TemperaturWerte[1] + TemperaturWerte[2] + TemperaturWerte[3] + TemperaturWerte[0];
    TempMittel = (float)Zwischen *100.0/1024.0;
    TemperaturWerte[0] = TemperaturWerte[1];
    TemperaturWerte[1] = TemperaturWerte[2];
    TemperaturWerte[2] = TemperaturWerte[3];
    TemperaturWerte[3] = WertTemp;

    prevTimeTemp = millis();
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println(analogRead(LM35)*500.0/1024.0);
}
