#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>

#define BT_TAG "btconfig"
#define APPID 0x55
#define DEVICENAME "devicename"
static const esp_bt_uuid_t localserviceUUID = {.len = ESP_UUID_LEN_128,
					       .uuid = {
						   .uuid128 = {0xeb, 0x6c, 0xb4, 0xc7, 0x81, 0xc6, 0x4f, 0xaf, 0x92,
							       0x8d, 0x77, 0xd5, 0x2e, 0xcf, 0x23, 0xf6}, // random uuid
					       }};

char ssid[32] = {};
#define ATTRIBURENUM 1
static const uint8_t attruuids[ATTRIBURENUM][16] = {
    [0] = {0x07, 0x90, 0xf9, 0x27, 0xa9, 0xd2, 0x46, 0x60, 0xaf, 0x44, 0x95, 0x9d, 0x1e, 0x49, 0x66, 0xce},
};
static const esp_gatts_attr_db_t attributes[ATTRIBURENUM] = {
    [0] = {.attr_control = {ESP_GATT_AUTO_RSP},
	   .att_desc =
	       {
		   .uuid_length = ESP_UUID_LEN_128,
		   .uuid_p = (uint8_t *)&(attruuids[0][0]),
		   .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
		   .max_length = 32,
		   .length = 32,
		   .value = (uint8_t *)&(ssid[0]),
	       }},
};
uint16_t attrHandlers[ATTRIBURENUM] = {};

static esp_ble_adv_data_t advConfig = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x06,
    .max_interval = 0x10,
    .appearance = 0,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = localserviceUUID.len,
    .p_service_uuid = (uint8_t *)&(localserviceUUID.uuid),
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t advParams = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
static void writeNvs(char *ssid, char *passwd, int channel, int auth) {
	nvs_handle_t nvsHandle;
	nvs_open("wifi_auth", NVS_READWRITE, &nvsHandle);
	nvs_set_str(nvsHandle, "ssid", ssid);
	nvs_set_str(nvsHandle, "passwd", passwd);
	nvs_set_u8(nvsHandle, "channel", channel);
	nvs_set_u8(nvsHandle, "authmetod", auth);
	nvs_commit(nvsHandle);
	nvs_close(nvsHandle);
};

static void gattEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param) {
	switch(event) {
	case ESP_GATTS_REG_EVT:
		if(param->reg.status == ESP_GATT_OK) {
			ESP_LOGI(BT_TAG, "reggistered");
			// todo: store the gatt interface somewhere
			esp_ble_gap_set_device_name(DEVICENAME);
			esp_ble_gap_config_adv_data(&advConfig);
			ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(attributes, gatt_if, 1, 0));
		};
		break;
	case ESP_GATTS_CREAT_ATTR_TAB_EVT:
		ESP_LOGI(BT_TAG, "The number handle =%x",param->add_attr_tab.num_handle);
		if (param->add_attr_tab.status != ESP_GATT_OK){
			ESP_LOGE(BT_TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
		}
		else if (param->add_attr_tab.num_handle != ATTRIBURENUM){
			ESP_LOGE(BT_TAG, "Create attribute table abnormally, num_handle (%d) \
					doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, ATTRIBURENUM);
		}
		else {
			memcpy(attrHandlers, param->add_attr_tab.handles, sizeof(attrHandlers));
			esp_ble_gatts_start_service(attrHandlers[]);
		}
		break;
	default:
		ESP_LOGW(BT_TAG, "not impleneted GATT event: %d", event);
		break;
	}
};

static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	switch(event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&advParams);
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		ESP_LOGD(BT_TAG, "SCAN respnse data set compleat");
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		if(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
			ESP_LOGI(BT_TAG, "Advertising start successfull!");
		} else {
			ESP_LOGE(BT_TAG, "Advertising start failed");
		};
		break;
	case ESP_GAP_BLE_LOCAL_ER_EVT:
	case ESP_GAP_BLE_LOCAL_IR_EVT:
		ESP_LOGD(BT_TAG, "DEBUG EVENT RECIVED");
		break;

	default:
		ESP_LOGW(BT_TAG, "not impleneted GAP event: %d", event);
		break;
	};
};

// https://github.com/espressif/esp-idf/blob/v5.3/examples/bluetooth/bluedroid/ble/gatt_server_service_table/tutorial/Gatt_Server_Service_Table_Example_Walkthrough.md
void btconfig() {
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
	ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
	ESP_LOGI(BT_TAG, "bt controller enabled");
	esp_bluedroid_init();
	esp_bluedroid_enable();
	esp_ble_gatts_register_callback(gattEventHandler);
	esp_ble_gap_register_callback(gapEventHandler);

	ESP_ERROR_CHECK(esp_ble_gatts_app_register(APPID));
	vTaskDelay(1000000 / portTICK_PERIOD_MS); // testing

	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
	ESP_LOGE(BT_TAG, "not implemented ended hear");
};
