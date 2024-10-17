#ifndef GPIOOUT
#define GPIOOUT
#include <stdint.h>
#include "driver/dac_cosine.h"
#define freqMask ((1U << 16) -1)
void setFreq(uint16_t freq);
dac_cosine_handle_t startFreq(uint16_t freq);
void stopFreq(dac_cosine_handle_t handler);
#endif
