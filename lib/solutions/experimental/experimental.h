#include "main.h"

#ifndef __EXPERIMENTAL__
#define __EXPERIMENTAL__

struct EXPERIMENTAL_type {};
typedef struct EXPERIMENTAL_type * HEXPERIMENTAL;

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi, SPI_HandleTypeDef* hslow_spi);
void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental);
void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental);

#endif //__EXPERIMENTAL__
