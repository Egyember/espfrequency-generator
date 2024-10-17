#include <stdint.h>
#include "driver/dac_cosine.h"
#include "esp_err.h"
#include "soc/sens_reg.h"
#include <math.h>
#include "gpioout.h"

void setFreq(uint16_t freq){
	uint16_t ajustedFreq = (unsigned int)(freq * 65536 / (8*pow(10, 6))) & freqMask;
	uint32_t regcontent = *((uint32_t *)SENS_SAR_DAC_CTRL1_REG);
	uint32_t maskedregister = regcontent & ~freqMask;
	*((uint32_t *)SENS_SAR_DAC_CTRL1_REG) = maskedregister | ajustedFreq;
};

dac_cosine_handle_t startFreq(uint16_t freq){
	dac_cosine_config_t config = {0};
	config.freq_hz = freq;	
	config.chan_id = DAC_CHAN_0;
	config.clk_src = DAC_COSINE_CLK_SRC_DEFAULT;
	config.phase = DAC_COSINE_PHASE_0;
	config.offset = 0;
	config.atten = DAC_COSINE_ATTEN_DB_0;
	dac_cosine_handle_t handler;
	ESP_ERROR_CHECK(dac_cosine_new_channel(&config, &handler));
	ESP_ERROR_CHECK(dac_cosine_start(handler));
	return handler;
};

void stopFreq(dac_cosine_handle_t handler){
	ESP_ERROR_CHECK(dac_cosine_stop(handler));
	ESP_ERROR_CHECK(dac_cosine_del_channel(handler));
};
