idf_component_register(
	SRCS "gpioout.c"
	INCLUDE_DIRS "include"
	REQUIRES esp_driver_gpio
	REQUIRES esp_driver_dac
	REQUIRES soc
)
