// https://www.makerguides.com/tutorials/a4988-stepper-motor-driver-arduino/

#include <AccelStepper.h>

//Define stepper motor connections
#define dirPin 2
#define stepPin 3

//Create stepper object
AccelStepper stepper(1,stepPin,dirPin); //motor interface type must be set to 1 when using a driver.


void setup()
{
  stepper.setMaxSpeed(1000); //maximum steps per second
}

void loop()
{
  stepper.setSpeed(400); //steps per second
  stepper.runSpeed(); //step the motor with constant speed as set by setSpeed()
}
