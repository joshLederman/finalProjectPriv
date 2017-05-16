
#include <stdint.h>
#include <time.h>

#include "board.h"
#include "fsl_dac_driver.h"


int majorKey[] = {0,2,4,5,7,9,11};

enum gamestate {
	//playingsound,
	gameover,
	awaitingresponse,
	awaitingnextround
} currentstate;

srand(time(NULL));

int randomPitchGenerator(void) {
	int pitchnum = majorKey[rand() % 7];
	return pitchnum;
}

//Sets up DAC
void setupDAC(void);

//Plays a tone
void play(int pitchNum);

//Sets up the timer
void setupTimer(void);

//Sets up GPIO pins
void setupPins(void);

void blink(int pitchNum);

void startGame(void);

void indicateGameEnding(void);

void respondToAnswer(int pitchNum);

void nextRound(void);

void runDemo(void);

void delay(void);
