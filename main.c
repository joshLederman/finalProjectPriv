#include <stdio.h>
#include <string.h>

#include "fsl_clock_manager.h"
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_os_abstraction.h"
#include "board.h"

#include "main.h"

//Constant references

//Represents DAC instance, needed for KSDK 
uint8_t dacInstance = 0;

//Buffer values needed to form a sinusoidal wave
uint16_t bufferVal[] = {0x000,0x02D,0x0B1,0x187,0x2A6,0x400,0x587,0x72A,0x8D6,0xA79,0xC00,0xD54,0xE79,0xF4F,0xFD3,0xFFF};

//C, C#, D, D#, E, F, F#, G, G#, A, A#, B

//Frequencies
uint16_t pitch[] = {262,277,294,311,330,349,370,392,415,440,466,494};

//Converts pitch index to output pin number
uint8_t pitchToLED[] = {11,0,10,0,3,2,0,20,0,18,0,19};

//Converts output pin number to pitch index
uint8_t buttonToPitch[] = {0,7,0,0,0,0,0,0,9,11,0,2,0,0,0,0,4,5};

//A 60 MHz PIT clock produces the appropriate output frequency
uint32_t clockFreq = 60000000;

//Represents pins used for input and output on a bit level
//For example, the least significant C in Port B - 0xC -> 1110 -> pin 1, 2, and 3 (but not 0)
uint32_t PortBUsed = 0x9C0E0C;
uint32_t PortCUsed = 0x030F83;
	
//Global Variables

//Buffer confiuration struct
dac_buffer_config_t *g_dacConfig;

//Represents number of PIT cycles before next DAC output value (interrupt)
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
	
	//Sets up DAC configuration
	setupDAC();
	
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
}

//Configures the PIT Timer
void setupTimer(void) {
	SIM->SCGC6 = SIM_SCGC6_PIT_MASK; 	//Turns on clock
	PIT_MCR = 0;											//Clears Module Control Register
	PIT->CHANNEL[0].LDVAL = 0;				//No Timer Value
	PIT->CHANNEL[0].TCTRL = 1; 				//Disable PIT Interrupts - sound is off
	NVIC_EnableIRQ(PIT0_IRQn);
}

//Configures the GPIO Pins
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
  
	//Clears Port B (all LEDs are off)
	PTB->PDOR &= ~PortBUsed;
}

//Configures the DAC
void setupDAC(void) {
	
	//Sets up the DAC at a basic level (KSDK functions and structs)
	dac_converter_config_t dacConfigStruct;
	DAC_DRV_StructInitUserConfigNormal(&dacConfigStruct);
	DAC_DRV_Init(dacInstance, &dacConfigStruct);
	
	//This is a KSDK struct representing the specific buffer configuration
	//Internal values represent the settings of the DAC buffer
	dac_buffer_config_t dacBuffConfigStruct;
	
	//Enables buffer
	dacBuffConfigStruct.bufferEnable = true; 
	//Allows the software PIT ISR to trigger the buffer pointer to move to the next element
	dacBuffConfigStruct.triggerMode = kDacTriggerBySoftware;
	//Buffer works in swing mode - it goes up and down along buffer (buffer contains half of sine wave)
	dacBuffConfigStruct.buffWorkMode = kDacBuffWorkAsSwingMode;
	//Uses full buffer (16 elements, represented as a value 0-15)
	dacBuffConfigStruct.upperIdx = 15;
	//Turns off everything else
	dacBuffConfigStruct.idxStartIntEnable = false;
	dacBuffConfigStruct.idxUpperIntEnable = false;
	dacBuffConfigStruct.idxWatermarkIntEnable = false;
	dacBuffConfigStruct.dmaEnable = false;
	dacBuffConfigStruct.watermarkMode = kDacBuffWatermarkFromUpperAs1Word; //Default setting
	
	//Applies the configuration created above
	DAC_DRV_ConfigBuffer(dacInstance, &dacBuffConfigStruct);
	
	//Loads the 16 values representing a sinusoid into the buffer registers
	DAC_DRV_SetBuffValue(dacInstance, 0, 16, bufferVal);
}

//Runs the introductory demonstration
void runDemo(void) {
	float setDur = duration; //Saves intial duration value
	duration = 0.25; //Slower duration for demo
	//Runs increasing scale (LEDs and sound)
	for (int8_t i = 0; i < 7; i++) {
		PTB->PDOR = (1 << pitchToLED[majorKey[i]]);
		play(majorKey[i]);
	}
	//Runs decreasing scale
	for (int8_t i = 5; i >= 0; i--) {
		PTB->PDOR = (1 << pitchToLED[majorKey[i]]);
		play(majorKey[i]);
	}
	//Clears LED from final loop
	PTB->PDOR = 0x00;
	//Waits time before starting game
	delay();
	//Restores duration
	duration = setDur;
}

//Turns on LED associated with a pitch for a short period of time
void blink(int pitchNum) {
	PTB->PDOR = (1 << pitchToLED[pitchNum]); //Sets appropriate GPIO output
	delay();
	PTB->PDOR = 0x00;	//Clears GPIO output
}

//Plays the indicated tone for a period of time indicated by duration
void play(int pitchNum) {
	double period_s = 1/(double)pitch[pitchNum]; //Calculated the time in seconds bewteen cycles
	//There are 30 points per cycle of the sine wave for the DAC output
	interPeriod = (int)clockFreq*period_s/30;	//Calculates period bewteen DAC outputs in PIT cycles
	
	//Calculates number of interrupts required to have one second output
	timeRemain = duration*pitch[pitchNum]*30;
	
	//Turns on timer and enables interrupts
	PIT_TFLG0 = 1;
	PIT->CHANNEL[0].LDVAL = interPeriod;
	PIT->CHANNEL[0].TCTRL = 3; //Enables interrupts to begin timer
	
	while (timeRemain != 0); //Plays tone until one second has passed
	
	PIT->CHANNEL[0].TCTRL = 1; //Disable interrupts to turn off timer
}
	
//Called when there us a button input
void respondToAnswer(int p) {
	if (p != mostrecent) { //If the pitch is not the one outputted
		strikes += 1;
		blink(mostrecent); //Blink the correct answer
	}
	else
		delay(); //Compensates for lack of break in blink
        //If game ends, run something here ------------------------------------------------------------
        if (strikes == 4) {
            int pos = 0;
            while(1) {
                if (pos == 7) pos = 0;
                play(pos);
                blink(pos);
                pos += 1;
            }
        }
}

//Called to play a new tone
void nextRound(void) {
	int p = randomPitchGenerator(); //Generates new pitch
	mostrecent = p; //Records the pitch
	play(p); //Outputs the sound
}

//Standard delay for the LED output and wait time between correct responses
void delay(void) {
	for (int i = 0; i < 20000000; i++);
}

//PIT Interrupt, triggering move to next buffer
void PIT0_IRQHandler(void) {
	PIT_TFLG0 = 1; //Clears the timeout
	PIT->CHANNEL[0].LDVAL = interPeriod; //Resets timer
	timeRemain--; //Reduces overall timer (should never enter as 0)
	uint8_t temp = DAC_DRV_GetBuffCurIdx(dacInstance);
	uint8_t bufferSetting = DAC0_C2;
	//Moves buffer pointer one position forward
	DAC_DRV_SoftTriggerBuffCmd(dacInstance);
}
