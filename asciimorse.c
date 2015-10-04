#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <wiringPi.h>

/* PIN ASSIGNMENT */
#define KEY_LISTEN 18 //GPIO24 - pin 18 in hw - pin 5 in wPi
#define KEY_SPEAK 16 //GPIO23 - pin 16 in hw

/* PROFILE VALUES */
/* This should be sliding-window determined */
#define SILENCE_TIMEOUT 10000;
#define DIT 1000 // Maximum time for a . 
#define KEYMAX = 5000 // maximum time a key should ever be
#define MAX_PULSES = 6 // Maximum number of pulses per character
int coreBuffer[MAX_PULSES];
int listen();
int index = 0;
unsigned int start_mills = 0;
unsigned int stop_mills = 0;
int lastPdiff = 0;
bool incData = false;

/*
 * Thanks to jacquerie/morse.c
 * abstract this away to a seperate place eventually
 * Should support arbitrary pulse/char mappings
 */
static const char* PULSE_TO_CHAR[128] = {
        NULL, NULL, "E", "T", "I", "N", "A", "M",
        "S", "D", "R", "G", "U", "K", "W", "O",
        "H", "B", "L", "Z", "F", "C", "P", NULL,
        "V", "X", NULL, "Q", NULL, "Y", "J", NULL,
        "5", "6", NULL, "7", NULL, NULL, NULL, "8",
        NULL, "/", NULL, NULL, NULL, "(", NULL, "9",
        "4", "=", NULL, NULL, NULL, NULL, NULL, NULL,
        "3", NULL, NULL, NULL, "2", NULL, "1", "0",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, ":",
        NULL, NULL, NULL, NULL, "?", NULL, NULL, NULL,
        NULL, NULL, "\"", NULL, NULL, NULL, "@", NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, "'", NULL,
        NULL, "-", NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, ".", NULL, "_", ")", NULL, NULL,
        NULL, NULL, NULL, ",", NULL, "!", NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

void reset() {
	start_mills = 0;
	stop_mills = 0;
	lastPdiff = 0;
	incData = false;
	memset(&coreBuffer[0], 0, sizeof(coreBuffer));
	// index = 0;
}

void startPulse() {
/* This function registers the start of a pulse */
	start_mills = mills();
}
void stopPulse() {
	stop_mills = mills();
	if ( start_mills < stop_mills ){
		lastPdiff = stop_mills - start_mills;
		incData = true
	} else {
		// Something went wrong, maybe the buffer overflowed
		//reset();
		printf("would have reset!/n");
	}
}

int  preProcessSequence() {
	
	unsigned char sum =0, bit;
	int i = 0;
	for (bit = 1; bit; bit <<= 1) {
		switch(coreBuffer[i]) {
		case 0:
			return sum | bit;
		default: 
			return 0;
		case 2:
			sum |= bit;
		case 1:
			break;
		}

		i++;
	}
	return 0;
	
}


char lookupSequence(int index) {
	return PULSE_TO_CHAR[index];
}
int convertPulse(int pDiff)
{
	/* Converts a pulse-time into 0, 1 or 2
	   1: Dit .
	   2: DA -
	   3: Something went wrong
	*/

	if ( pDiff < KEYMAX ) {
		if (pDiff <= DIT) {
			return 1;
		} else {
			return 2;
		}
	} else {
		return 3;
	}
}
int main(void)
{

	/* reset memory */
	reset();
	/* Call the setup function for wiringPi */
	/* We're going to work with the pins directly */
	wiringPiSetupPhys();

	/* Listen, and write  */  
	pinMode(KEY_LISTEN, INPUT);
	pinMode(KEY_SPEAK, OUTPUT);

	/* set pullup for input pin */
	pullUpDnControl(INPUT,PUD_UP);

	/* Register inturrupt handler */
	int resR = wiringPiISR(KEY_LISTEN, INT_EDGE_RISING, &startPulse);
	int resF = wiringPiISR(KEY_LISTEN, INT_EDGE_FALLING, &stopPulse);
	/* Enter kernel loop */
	while(true) {
		char c = '?';
		if ( incData ) {
			int resB = convertPulse(lastPdiff);
			// Add the symbol to the queue or process the char
			if ( resB == 3 ) { // error or we're done 
				c = lookupSequence(preProcessChar());
				reset();
			} else {
				coreBuffer[index] = resB;
			}  		
		} else {
			// do nothing
			//sleep(.1);
		} 
		printf("%c",c);
		
	}
}

