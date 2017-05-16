#include <stdio.h>
#include <string.h>

#include "fsl_clock_manager.h"
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_os_abstraction.h"
#include "board.h"

#include "main.h"

//Constant references

//Buffer values needed to form a sinusoidal wave
uint16_t bufferVal[] = {0x000,0x02D,0x0B1,0x187,0x2A6,0x400,0x587,0x72A,0x8D6,0xA79,0xC00,0xD54,0xE79,0xF4F,0xFD3,0xFFF};

//C, C#, D, D#, E, F, F#, G, G#, A, A#, B

//Frequencies
uint16_t pitch[] = {262,277,294,311,330,349,370,392,415,440,466,494};

//Converts pitch index to output pin number
uint8_t pitchToLED[] = {11,0,10,0,3,2,0,20,0,18,0,19,9};

//Converts output pin number to pitch index
uint8_t buttonToPitch[] = {11,7,0,0,0,0,0,0,9,11,0,2,0,0,0,0,4,5};

//A 2 MHz PIT clock produces the appropriate output frequency
uint32_t clockFreq = 2000000;

//Represents pins used for input and output on a bit level
//For example, the least significant C in Port B - 0xC -> 1110 -> pin 1, 2, and 3 (but not 0)
uint32_t PortBUsed = 0x9C0E0C;
uint32_t PortCUsed = 0x030F83;
	
//Global Variables

//Buffer confiuration struct
dac_buffer_config_t *g_dacConfig;

//Represents number of PIT interrupts before next DAC output value
uint32_t interPeriod = 0;
	
//Length of time in seconds for tone to play (float allows it to be less than 1)
float duration = 1; 
	
//Remaining number of PIT interrupts before a tone is complete
uint32_t timeRemain = 0; //Keeps track of length tone has played

//Records last pitch played (to compare to user input)
uint8_t mostrecent = 0;

//Records the number of incorrect responses
uint16_t strikes = 0;

//CODE

int main (void) {
  // KSDK provided init function - primarily sets up clocks
  hardware_init();
	
	//Sets up the PIT timer used to precisely time the sound output
	setupTimer();
	
	//Sets up the GPIO pins used to input from the buttons and output to the LEDs
	setupPins();
	
	//Runs the introductory demo of the device to show the pitches
	runDemo();
	
	//Primary loop - each loop represents one round (one pitch, one button press)
	while(1) {
		//Plays the pitch
		nextRound();
		
		//Waits for user input
		while((~(PTC->PDIR) & PortCUsed) == 0x00);
		//Records the input when it occurs
		uint32_t response = (~(PTC->PDIR) & PortCUsed);
		
		//Finds the port from which the response came
		uint8_t buttonNum = 100; //100 represents no response - should not occur
		for (int i = 0; i < 32; i++) {
			if ((response >> i) & 1) {
				buttonNum = i;
				break;
			}
		}
		if (buttonNum != 100)
			//Runs the LED output
			respondToAnswer(buttonToPitch[buttonNum]);
	}
	
	//Test of LEDs
	/*
	while(1) {
		//PTB->PDOR = (((PTC->PDOR >> {PTC Pin Num}) & 1) << {PTB Pin Num})
		PTB->PDOR = ((~(PTC->PDIR >> 10) & 1) << 11) | 
								((~(PTC->PDIR >> 11) & 1) << 10) |
								((~(PTC->PDIR >> 16) & 1) << 3)  |
								((~(PTC->PDIR >> 17) & 1) << 2)	 |
								((~(PTC->PDIR >> 1) & 1) << 20)  |
								((~(PTC->PDIR >> 8) & 1) << 18)  |
								((~(PTC->PDIR >> 9) & 1) << 19)  |
								((~(PTC->PDIR >> 0) & 1) << 9)
		;
		//PTB->PDOR &= ~((~(PTC->PDIR >> 10) & 1) << 11);
		//PTB->PDOR |= ((~(PTC->PDIR >> 11) & 1) << 11); //PTC11, PTB10
		//PTB->PDOR &= ~((~(PTC->PDIR >> 11) & 1) << 10);
		
	}*/
	
	//device_deinit();
}

void device_config() {
	//Allocates memory for a struct representing the buffer configuration
	//g_dacConfig = (dac_buffer_config_t *)OSA_MemAlloc(sizeof(dac_buffer_config_t));

	dac_converter_config_t dacConfigStruct;
	dac_buffer_config_t dacBuffConfigStruct;
	
	DAC_DRV_StructInitUserConfigNormal(&dacConfigStruct);
	
	DAC_DRV_Init(BOARD_DAC_DEMO_DAC_INSTANCE, &dacConfigStruct);
	
	//Configures buffer settings in g_dacConfig
	//Enables buffer
	dacBuffConfigStruct.bufferEnable = true; 
	//Software moves to the next buffer entry
	dacBuffConfigStruct.triggerMode = kDacTriggerBySoftware;
	//Buffer works in swing mode - it goes up and down along buffer
	dacBuffConfigStruct.buffWorkMode = kDacBuffWorkAsSwingMode;
	//Uses full buffer (16 elements, represented as a value 0-15)
	dacBuffConfigStruct.upperIdx = 15;
	//Turns off everything else
	dacBuffConfigStruct.idxStartIntEnable = false;
	dacBuffConfigStruct.idxUpperIntEnable = false;
	dacBuffConfigStruct.idxWatermarkIntEnable = false;
	dacBuffConfigStruct.dmaEnable = false;
	dacBuffConfigStruct.watermarkMode = kDacBuffWatermarkFromUpperAs1Word;
	
	DAC_DRV_ConfigBuffer(BOARD_DAC_DEMO_DAC_INSTANCE, &dacBuffConfigStruct);
	
	DAC_DRV_SetBuffValue(BOARD_DAC_DEMO_DAC_INSTANCE, 0, 16, bufferVal);
	
	/*for (i = 0; i < 16; i++) {
		DAC_DRV_SetBuffValue(BOARD_DAC_DEMO_DAC_INSTANCE, i, 16, &bufferVal[i]);
	}*/

	//Trigger first value
	DAC_DRV_SoftTriggerBuffCmd(BOARD_DAC_DEMO_DAC_INSTANCE);
	
	/*
	//The full length of the buffer (16 elements - length(0:15)) will be used
	g_dacConfig->upperIdx = 15; 
	
	//Set Buffer config registers to reset state
	DAC_HAL_Init(DAC0);
	
	
	
	//Assign configuration values set above to appropriate registers
	DAC_DRV_ConfigBuffer(BOARD_DAC_DEMO_DAC_INSTANCE,g_dacConfig);
	
	//Sets the 16 appropriate buffer values in the DAC
	DAC_DRV_SetBuffValue(BOARD_DAC_DEMO_DAC_INSTANCE,0,16,bufferVal);
	
	//Enables DAC
	DAC_HAL_Enable(DAC0);*/
}

void device_deinit() {
    // De initialize DAC
    DAC_DRV_Deinit(BOARD_DAC_DEMO_DAC_INSTANCE);
}

void blink(int pitchNum) {
	PTB->PDOR = (1 << pitchToLED[pitchNum]);
	delay();
	PTB->PDOR = 0x00;
}

void play(int pitchNum) {
	double period_s = 1/(double)pitch[pitchNum];
	interPeriod = (int)clockFreq*period_s;
	
	timeRemain = duration*pitch[pitchNum]*30;
	
	device_config(); //Configure DAC and enables it
	
	//Turns on timer and enables interrupts
	PIT_TFLG0 = 1;
	PIT->CHANNEL[0].LDVAL = interPeriod;
	PIT->CHANNEL[0].TCTRL = 3;
	
	while (timeRemain != 0); //Plays tone
	
	PIT->CHANNEL[0].TCTRL = 1; //Disable interrupts to turn off timer
	
	device_deinit(); //Quits DAC
}
	
void setupTimer(void) {
	SIM->SCGC6 = SIM_SCGC6_PIT_MASK;
	PIT_MCR = 0;
	PIT->CHANNEL[0].LDVAL = 0;
	PIT->CHANNEL[0].TCTRL = 1; //Disable Interrupts - sound is off
	NVIC_EnableIRQ(PIT0_IRQn);
}

void setupPins(void) {
	SIM->SCGC5    |= (1 <<  10) | (1 <<  11);  /* Enable Clock to Port B & C */ 
  //Ports B pins are output
	//Used: pins 2,3,10,11,20,18,19,9,23
	
	//Ports C pins are input
	//Used: pin 10,11,1,8,9,0,7,16,17
	
	
	//Sets all appropriate pins as GPIO
	PORTB->PCR[2] = (1 <<  8) ;
	PORTB->PCR[3] = (1 <<  8) ;
	PORTB->PCR[9] = (1 <<  8) ;
	PORTB->PCR[10] = (1 <<  8) ;
	PORTB->PCR[11] = (1 <<  8) ;
	PORTB->PCR[18] = (1 <<  8) ;
	PORTB->PCR[19] = (1 <<  8) ;
	PORTB->PCR[20] = (1 <<  8) ;
	PORTB->PCR[23] = (1 <<  8) ;
	
	PORTC->PCR[0] = (1 <<  8) ;
	PORTC->PCR[1] = (1 <<  8) ;
	PORTC->PCR[7] = (1 <<  8) ;
	PORTC->PCR[8] = (1 <<  8) ;
	PORTC->PCR[9] = (1 <<  8) ;
	PORTC->PCR[10] = (1 <<  8) ;
	PORTC->PCR[11] = (1 <<  8) ;
	PORTC->PCR[16] = (1 <<  8) ;
	PORTC->PCR[17] = (1 <<  8) ;
	
	PTB->PDDR |= PortBUsed; //All of port B is output
	PTC->PDDR &= ~(PortCUsed); //All of port C is input
  
	PTB->PDOR &= ~PortBUsed;
}

void PIT0_IRQHandler(void) {
	PIT_TFLG0 = 1; //Clears the timeout
	PIT->CHANNEL[0].LDVAL = interPeriod; //Resets timer
	timeRemain--; //Reduces overall timer (should never enter as 0)
	uint8_t temp = DAC_DRV_GetBuffCurIdx(BOARD_DAC_DEMO_DAC_INSTANCE);
	uint8_t bufferSetting = DAC0_C2;
	//Moves buffer pointer one position forward
	DAC_DRV_SoftTriggerBuffCmd(BOARD_DAC_DEMO_DAC_INSTANCE);
}

void indicateGameEnding(void) {
	
}

void respondToAnswer(int p) {
	if (p != mostrecent) {
		strikes += 1;
		blink(mostrecent);
	}
	else
		delay(); //Compensates for lack of break in blink
	if (strikes == 3) {
		indicateGameEnding();
		currentstate = gameover;
	} else {
		currentstate = awaitingnextround;
	}
}

void nextRound(void) {
	int p = randomPitchGenerator();
	mostrecent = p;
	play(p);
}

void runDemo(void) {
	float setDur = duration;
	duration = 0.25; //Slower duration for demo;
	for (int8_t i = 0; i < 7; i++) {
		PTB->PDOR = (1 << pitchToLED[majorKey[i]]);
		play(majorKey[i]);
	}
	for (int8_t i = 5; i >= 0; i--) {
		PTB->PDOR = (1 << pitchToLED[majorKey[i]]);
		play(majorKey[i]);
	}
	PTB->PDOR = 0x00;
	delay();
	duration = setDur;
}

//standard delay
void delay(void) {
	for (int i = 0; i < 20000000; i++);
}
