#include "TX20.h"

/*
  fonctions permettant la recuperation des informations provenant d'une station meteo La Crosse TX20

*/

const char TX20_Directions[16][4] = {{"N"}, {"NNE"}, {"NE"}, {"ENE"}, {"E"}, {"ESE"}, {"SE"}, {"SSE"}, {"S"}, {"SSW"}, {"SW"}, {"WSW"}, {"W"}, {"WNW"}, {"NW"}, {"NNW"}};

lastFrame lf;			// on alloue la structure lastFrame, qui va contenir les infos de la derniere trame recue de la TX20
char TX20frame[41];  	// la trame de la TX20, 1 bit par indice

lastFrame *getLastFrame()
{
	return &lf;
}

// retourne la derniere trame recue de la TX20, de maniere formatee
char *getLastDecodeFrame()
{
	static char newframe[47];
	int ind = 0;
	int ind_raw;
	
    for (ind_raw = 0; ind_raw < 41; ind_raw++)
    {
		newframe[ind] = 48 + (TX20frame[ind_raw] ^1);  // transfo binaire en chiffre ascii
		if ((ind_raw == 4) || (ind_raw == 8) ||(ind_raw == 20) || (ind_raw == 24) || (ind_raw == 28))
        {
			newframe[++ind] = ' ';
		}			
		ind++;
    }
	return newframe;
	
}

/*
  ------------- decode la trame recue du TX20 -----------------------
  . frame contient la trame de 41 bits recue de la TX20. Un index a une valeur de 0 ou 1/16
  . lf2 est le resultat du decodage. 
*/
void decodeFrame(char *frame, lastFrame *lf2)
{
	unsigned int pin;
	lf2->startframe = 0;
	lf2->winddir = 0;
	lf2->winddir2 = 0;
	lf2->windspeed = 0;
	lf2->windspeed2 = 0;
	lf2->checksum = 0;
	lf2->chk = 0;
	
	for (int i =0; i < 41; i++) 
	{
		pin = frame[i];
		if (i < 5){			// 5 premiers bits
			// start, inverted. 5 bits, doit avoir la valeur 4 (00100)
			lf2->startframe = (lf2->startframe<<1) | (pin^1);
		} else
		if (i < 5+4){			// 4 bits suivants
			// wind dir, inverted. 4 bits. Depuis le Nord, par pas de 1/16 de 360° (donc, 22.5°)
			lf2->winddir = lf2->winddir>>1 | ((pin^1)<<3);
		} else
		if (i < 5+4+12){
			// windspeed, inverted. 12 bits. valeur en 0.1 m/s
			lf2->windspeed = lf2->windspeed>>1 | ((pin^1)<<11);
		} else
		if (i < 5+4+12+4){
			// checksum, inverted
			lf2->checksum = lf2->checksum>>1 | ((pin^1)<<3);
		} else
		if (i < 5+4+12+4+4){
			// wind dir
			lf2->winddir2 = lf2->winddir2>>1 | (pin<<3);
		} else {
			// windspeed
			lf2->windspeed2 = lf2->windspeed2>>1 | (pin<<11);
		}
	}
	lf2->chk = ( lf2->winddir + (lf2->windspeed&0xf) + ((lf2->windspeed>>4)&0xf) + ((lf2->windspeed>>8)&0xf) );lf2->chk&=0xf;

}

/*
	Acquisition d'une trame de la TX20
	C'est une boucle d'attente d'une valeur haute (1) du signal DATA qui indique le debut de la trame,
	suivie de la detection de la pin data toutes les 1.22ms, sur une boucle de 41
*/
int RPi_TX20_GetReading(int mode, int *iDir, int *iSpeed )
{
    static int firstexec = 1;
	int returncode = 0;
    int bitcount = 0;
    unsigned int pin;
	
	lf.startframe = 0; lf.winddir = 0; lf.winddir2 = 0; lf.windspeed = 0; lf.windspeed2 = 0; lf.checksum = 0; lf.chk = 0; lf.errorcode = 0;
	
	
	if (firstexec == 1)			// 1ere execution de cette fonction
	{
		firstexec = 0;
		if (mode == TX20_MODE_SYNC)
			TX20_DTR_SET_LOW;		// la TX20 va emettre des trames de 41 bits toutes les 1.5 secondes environ
	}
	
	if (mode == TX20_MODE_ASYNC)
	{
		TX20_DTR_SET_LOW;		// la TX20 va emettre des trames de 41 bits toutes les 5.2 secondes. Sauf si re execution en moins de 10 ms ; alors, 1.5s
	}

    // Input should be low right now
    if ( TX20_DATA_GET_BIT != 0 )
	{
        TX20_DTR_SET_HIGH;
        returncode = INVALID_PIN_STATE;
    }
	else 
	{
		// on attend le debut de la trame
		// on se donne 10 secondes pour avoir un debut de trame. L'attente est couteuse en CPU : quasiment 100%
		// en fait, ca prend environ 1.5s si on relance la procedure tres rapidement (qqs milli secondes)
		// sinon, c'est de l'ordre de 5.2s
		for (int i = 0; i < 100000; i++) 
		{
			pin = TX20_DATA_GET_BIT;
			if (pin == 1 ){
				break;
			}
			delayMicroseconds(100);
		}
		
		if (pin == 0)
		{
			//fprintf(stderr, "TX20. Pas de debut de trame\n");
			if (mode == TX20_MODE_ASYNC)
				TX20_DTR_SET_HIGH;
			returncode = NO_FRAME;
		}
		
		else
		{
			for (bitcount = 0; bitcount < 41; bitcount++) 
			{
				pin = (TX20_DATA_GET_BIT);
				TX20frame[bitcount] = pin;
				TX20_DoDelay;
			}
		}

		//check if we got a valid datagram
		decodeFrame(TX20frame, &lf);
		//Calculate Checksum
		lf.chk = ( lf.winddir + (lf.windspeed&0xf) + ((lf.windspeed>>4)&0xf) + ((lf.windspeed>>8)&0xf) );lf.chk&=0xf;
		
		if (lf.startframe==4 && lf.winddir==lf.winddir2 && lf.windspeed==lf.windspeed2 && lf.checksum==lf.chk)
		{
			returncode = FRAME_OK;			
		} else 
		{
			returncode = FRAME_NOK;
		}
	}

	//return values
	*iDir = lf.winddir;
	*iSpeed = lf.windspeed;

	if (mode == TX20_MODE_ASYNC)
		TX20_DTR_SET_HIGH;
	
	return returncode;
}

// intitialisation des pins du GPIO
void RPi_TX20_InitPins( void )
{
	// Set the DATA pin to input
	TX20_DATA_SET_INPUT;
	// and the DTR pin to output
	TX20_DTR_SET_OUTPUT;
}

// initilaisation du GPIO
int RPi_TX20_Initialize(int mode)
{
	//Initialise the Raspberry Pi GPIO
	if (!bcm2835_init()){
		fprintf(stderr, "initialisation du GPIO impossible\n");
		return(FALSE);
	}

	// Set up the TX23 Pins
	RPi_TX20_InitPins();
	
	if (mode == TX20_MODE_ASYNC)
	{
		TX20_DTR_SET_HIGH;		// stop datas
	}
	
	return TRUE;
}

// fin du programme. On remet le GPIO dans un etat 'normal'
void RPi_TX20_Terminate (int mode)
{
	//if (mode == TX20_MODE_ASYNC)
	//{
		TX20_DTR_SET_HIGH;		// stop datas
	//}
	//bcm2835_gpio_set_pud(TX20DATA, BCM2835_GPIO_PUD_OFF);  // on retire le pulldown
	bcm2835_close();
}

