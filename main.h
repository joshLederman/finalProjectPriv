
#include <stdint.h>

#include "board.h"
#include "fsl_dac_driver.h"


int majorKey[] = {0,2,4,5,7,9,11};

//Sets up DAC
void setupDAC(void);

//Sets up the timer
void setupTimer(void);

//Sets up GPIO pins
void setupPins(void);

//Plays a tone
void play(int pitchNum);

//Flashes an LED
void blink(int pitchNum);

//Plays appropriate response to user input
void respondToAnswer(int pitchNum);

//Plays new tone
void nextRound(void);

//Runs demonstration at beginning
void runDemo(void);

//Generates Random Pitch
int randomPitchGenerator(void);

//Standard manual delay
void delay(void);
