#include "nvs.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_err.h"

#define BT_TAG "btconfig"
#define APPID 0x55
#define DEVICENAME "devicename"

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

static void gattEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t * param){
	switch(event){
		case ESP_GATTS_REG_EVT:
			ESP_LOGI(BT_TAG, "reggistered");
			esp_ble_gap_set_device_name(DEVICENAME);
			esp_ble_adv_data_t advConfig = {
				.set_scan_rsp = true,
				.include_name = true,
				.include_txpower = true,
				.min_interval = 0x06,
				.max_interval = 0x10,
				.appearance =  0,
			};
			esp_ble_gap_config_adv_data(&advConfig);
			//todo: finish this
			break;
		default:
			ESP_LOGW(BT_TAG, "not impleneted");
			break;
			
	}
};

static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param){

};
void btconfig(){
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); 
	ESP_ERROR_CHECK( esp_bt_controller_init(&bt_cfg));
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_BLE));
	ESP_LOGI(BT_TAG, "bt controller enabled");
	esp_bluedroid_init();
	esp_bluedroid_enable();
	esp_ble_gatts_register_callback(gattEventHandler);
	esp_ble_gap_register_callback(gapEventHandler);

	esp_ble_gatts_app_register(APPID);

	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
ESP_LOGE(BT_TAG, "not implemented");
};
