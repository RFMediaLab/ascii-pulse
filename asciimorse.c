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
#define SILENCE_TIMEOUT 1000
#define DIT 60
// Maximum time for a . 
#define KEYMAX 600
 // maximum time a key should ever be
#define MAX_PULSES 6
 // Maximum number of pulses per character

#define DEBOUNCE 5
 // Num millis to assume we're bounce


int coreBuffer[MAX_PULSES];
int listen();
int buffIndex = 0;
unsigned int start_millis = 0;
unsigned int stop_millis = 0;
int lastPdiff = 0;
int incData = 0;
int inPulse = 0;
/*
 * Thanks to jacquerie/morse.c
 * abstract this away to a seperate place eventually
 * Should support arbitrary pulse/char mappings
 */
char PULSE_TO_CHAR[128] = {
        0, 0, 'E', 'T', 'I', 'N', 'A', 'M',
        'S', 'D', 'R', 'G', 'U', 'K', 'W', 'O',
        'H', 'B', 'L', 'Z', 'F', 'C', 'P', 0,
        'V', 'X', 0, 'Q', 0, 'Y', 'J', 0,
        '5', '6', 0, '7', 0, 0, 0, '8',
        0, '/', 0, 0, 0, '(', 0, '9',
        '4', '=', 0, 0, 0, 0, 0, 0,
        '3', 0, 0, 0, '2', 0, '1', '0',
        0, 0, 0, 0, 0, 0, 0, ':',
        0, 0, 0, 0, '?', 0, 0, 0,
        0, 0, '/', 0, 0, 0, '@', 0,
        0, 0, 0, 0, 0, 0, '|', 0,
        0, '-', 0, 0, 0, 0, 0, 0,
        0, 0, '.', 0, '_', ')', 0, 0,
        0, 0, 0, ',', 0, '!', 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
};

void reset() {
	start_millis = 0;
	stop_millis = 0;
	lastPdiff = 0;
	incData = 0;
	memset(&coreBuffer[0], 0, sizeof(coreBuffer));
	// index = 0;
	inPulse = 0;
	buffIndex = 0;
}

void startPulse() {
	int workingMillis = millis();
	if ( (workingMillis - start_millis) < DEBOUNCE ) {
		// Don't reset start_milis
	} else { 
		start_millis = workingMillis; 
		//printf("Starting at %i\n", start_millis);
		while (digitalRead(KEY_LISTEN) == 1) {
			delay(DEBOUNCE); 
		} 
		stop_millis = millis();
		lastPdiff = stop_millis - start_millis;
		//printf("StopKey: %i ms\n", lastPdiff);
		incData = 1;
		
	}
	delay(DEBOUNCE);
	start_millis = millis();

}


void stopPulse() {
	//printf("gotPulseIRQ_FALLING\n");
	stop_millis = millis();
	if ( start_millis < stop_millis ){
		lastPdiff = stop_millis - start_millis;
		incData = 1;
	} else {
		// Something went wrong, maybe the buffer overflowed
		//reset();
		printf("would have reset!/n");
	}
}

int  preProcessSequence() {
	// break out if we're starting with a 0;
	if ( coreBuffer[0] == 0 ) { return 0; }  
	//printf("Attempting to resolve sequence\n");	
	unsigned char sum =0, bit;
	int i = 0;
	for (bit = 1; bit; bit <<= 1) {
		switch(coreBuffer[i++]) {
		case 0:
			//printf("index: %i\n", sum|bit);
			return sum | bit;
		default: 
			return 0;
		case 2:
			sum |= bit;
		case 1:
			break;
		}

	}
	return 0;
	
}


char lookupSequence(int index) 
{
	char p = PULSE_TO_CHAR[index];
	if (p == 0 || p == NULL)  {
		return 0;
	} else {
		return p;
	}
}
int convertPulse(int pDiff)
{
	//printf("Converting pulse\n");
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

	printf("initializing...\n");
	wiringPiSetupPhys();

	/* Listen, and write  */  
//	pinMode(KEY_LISTEN, INPUT);
//	pinMode(KEY_SPEAK, OUTPUT);

	/* set pullup for input pin */
//	pullUpDnControl(INPUT,PUD_UP);

	printf("Registering interrupts...\n");
	/* Register inturrupt handler */
	int resR = wiringPiISR(KEY_LISTEN, INT_EDGE_RISING, &startPulse);
	///int resF = wiringPiISR(KEY_LISTEN, INT_EDGE_FALLING, &stopPulse);
	/* Enter kernel loop */
	printf("Entering wait mode...\n");
	int timeout = 0;
	while(1) { 
		char c = '?';
		if ( incData ) {
			timeout = millis();	
			int resB = convertPulse(lastPdiff);
			lastPdiff = 0;
			incData = 0;
			// Add the symbol to the queue or process the char
			if ( resB == 3 ) { // error or we're done 
				c = lookupSequence(preProcessSequence());
				if ( c != 0 ) {
				printf("%c\n",c);}
				reset();
			} else {
				coreBuffer[buffIndex] = resB;
				buffIndex++;
			}  		
		} else {
			// check for timeout
			if (millis() > timeout + SILENCE_TIMEOUT) {
				//printf("timing out - printing\n");
				if ( coreBuffer[0] == 0 ) {
					// Buffer is 0, don't do anything
				} else {
					c = lookupSequence(preProcessSequence());
					if ( c != 0) {
						printf("%c\n",c);
					}
					timeout = millis();
					reset();
				}
			} 
			// do nothing
			//sleep(.1);
		}
		
	}
	printf("broke out of loop?!\n");
	return 1;
}

