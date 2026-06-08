#ifndef GPS_H
#define GPS_H

#include <stdint.h>

typedef struct  
{										    

	uint32_t latitude;
	uint8_t nshemi;			  
	uint32_t longitude;
	uint8_t ewhemi;
}__attribute__((packed)) nmea_msg;
 

typedef struct Data
{
	float latitude;
	char N_S;
	float longitude;
	char E_W;
}Data;

    
void NMEA_GNRMC_Analysis(nmea_msg *gpsx, uint8_t *buf);

#endif