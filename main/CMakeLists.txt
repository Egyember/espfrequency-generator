idf_component_register(SRCS "wifi_teszt.c"
	INCLUDE_DIRS "."
	REQUIRES esp_netif
	REQUIRES nvs_flash
	REQUIRES esp_wifi
	REQUIRES broadcaster
	REQUIRES commands
)
