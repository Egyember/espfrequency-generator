#include "nvs.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_err.h"

#define BT_TAG "btconfig"

void writeNvs(char* ssid, char* passwd, int channel, int auth){
	nvs_handle_t nvsHandle;
	nvs_open("wifi_auth", NVS_READWRITE, &nvsHandle);
	nvs_set_str(nvsHandle, "ssid", ssid);
	nvs_set_str(nvsHandle, "passwd", passwd);
	nvs_set_u8(nvsHandle, "channel", channel);
	nvs_set_u8(nvsHandle, "authmetod", auth);
	nvs_commit(nvsHandle);
	nvs_close(nvsHandle);
};

void btconfig(){
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); 
	ESP_ERROR_CHECK( esp_bt_controller_init(&bt_cfg));
};
