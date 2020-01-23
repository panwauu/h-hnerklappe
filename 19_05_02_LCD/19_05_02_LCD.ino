#include <Wire.h>                  // Library für I2C aufgerufen
#include <LiquidCrystal_I2C.h>     // Library für LCD aufgerufen
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD I2C address

void setup()
{
lcd.init();                        // Initialisierung des LCD
                                   // Hier folgt die Ausgabe auf das LCD
lcd.backlight();                   // LCD Hintergrundbeleuchtung aktivieren
lcd.setCursor(5, 0);               // Cursor setzen an Stelle 5, Zeile 1
lcd.print("Hallo");                // Text für die erste Zeile
lcd.setCursor(0, 1);               // Cursor setzen an Stelle 0, Zeile 2
lcd.print("arduinoforum.de");      // Text für die zweite Zeile

}
void loop()
{
}
