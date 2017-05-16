
#include <stdint.h>

#include "board.h"
#include "fsl_dac_driver.h"


int majorKey[] = {0,2,4,5,7,9,11};

enum gamestate {
	//playingsound,
	gameover,
	awaitingresponse,
	awaitingnextround
} currentstate;

int randomPitchGenerator(void) {
	int pitchnum = majorKey[rand() % 7];
	return pitchnum;
}

//Configures DAC
void device_config(void);

void device_deinit(void);

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
