#ifndef RPI_TX20_H_
#define	RPI_TX20_H_

#include <bcm2835.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>

// Defines
#define	TRUE	1
#define	FALSE	0

# define INVALID_PIN_STATE -3		// pb gpio. la pin DTR n'a pas change d'etat apres envoi TX20_DTR_SET_LOW
# define NO_FRAME -2				// pas de detection de debut de trame, apres envoi TX20_DTR_SET_LOW
# define FRAME_NOK -1				// la trame decodee n'est pas valide
# define FRAME_OK 0					// la trame decodee est valide

// Define the Raspberry Pi GPIO Pins for the TX20
#define TX20DATA RPI_GPIO_P1_18 // Input on RPi pin GPIO 11 physical
#define TX20DTR RPI_GPIO_P1_12 // Input on RPi pin GPIO 12 physical

# define TX20_MODE_ASYNC 0			// mode de lecture asynchrone. On passe le DTR a HIGH apres chaque lecture de trame. Il faut au moins 5s pour recuperer la trame
# define TX20_MODE_SYNC 1			// mode de lecture synchrone. On Laisse le DTR a LOW ; la TX20 emet des trames toutes les 1.5s environ

#define	TX20_DATA_SET_INPUT	        bcm2835_gpio_fsel(TX20DATA, BCM2835_GPIO_FSEL_INPT); bcm2835_gpio_set_pud(TX20DATA, BCM2835_GPIO_PUD_DOWN) // pin en input, en pull-down
#define	TX20_DTR_SET_OUTPUT	        bcm2835_gpio_fsel(TX20DTR, BCM2835_GPIO_FSEL_OUTP); bcm2835_gpio_set_pud(TX20DTR, BCM2835_GPIO_PUD_OFF)   // pin en output, sans pull-p/down
#define TX20_DTR_SET_LOW            bcm2835_gpio_write(TX20DTR, LOW);
#define TX20_DTR_SET_HIGH           bcm2835_gpio_write(TX20DTR, HIGH);

#define TX20_DATA_GET_BIT		    bcm2835_gpio_lev(TX20DATA)
#define TX20_DoDelay delayMicroseconds(1220)  // 41 bits = 50.02 ms

extern const char TX20_Directions[16][4];

typedef struct
{
	int errorcode;
	int startframe;
	int winddir;
	int winddir2;
	int windspeed;
	int windspeed2;
	int checksum;
	int chk;
} lastFrame;

/* Public Functions ----------------------------------------------------------- */
int RPi_TX20_Initialize( int );
void RPi_TX20_Terminate( int );
int RPi_TX20_GetReading(int mode, int *iDir, int *iSpeed );
lastFrame *getLastFrame();
char *getLastDecodeFrame();

#endif

