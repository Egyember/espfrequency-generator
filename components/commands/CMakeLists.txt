idf_component_register(SRCS "commands.c"
	INCLUDE_DIRS "include"
	REQUIRES esp_driver_gptimer
	REQUIRES gpioout
)
