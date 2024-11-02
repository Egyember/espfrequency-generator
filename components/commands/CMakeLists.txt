idf_component_register(SRCS "commands.c"
	INCLUDE_DIRS "include"
	RREQUIRES esp_driver_gptimer
	EQUIRES gpioout
)
