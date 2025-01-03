#include "btadvconfig.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

EventGroupHandle_t btEventGroup;
#define BTDONE BIT0

#define BT_TAG "btconfig"
#define DEVICENAME "ahhh"

static void writeNvs(char *ssid, char *passwd, uint8_t channel, uint8_t auth) {
	ESP_LOGI(BT_TAG, "ssid: %s, password:%s, channel: %d, auth: %d", ssid, passwd, channel, auth);
	nvs_handle_t nvsHandle;
	nvs_open("wifi_auth", NVS_READWRITE, &nvsHandle);
	nvs_set_str(nvsHandle, "ssid", ssid);
	nvs_set_str(nvsHandle, "passwd", passwd);
	nvs_set_u8(nvsHandle, "channel", channel);
	nvs_set_u8(nvsHandle, "authmetod", auth);
	nvs_commit(nvsHandle);
	nvs_close(nvsHandle);
};

static void loadNvs(char *ssid, size_t *ssidS, char *passwd, size_t *passwdS, uint8_t *channel, uint8_t *auth) {
	nvs_handle_t nvsHandle;
	nvs_open("wifi_auth", NVS_READONLY, &nvsHandle);
	nvs_get_str(nvsHandle, "ssid", ssid, ssidS);
	nvs_get_str(nvsHandle, "passwd", passwd, passwdS);
	nvs_get_u8(nvsHandle, "channel", channel);
	nvs_get_u8(nvsHandle, "authmetod", auth);
	nvs_commit(nvsHandle);
	nvs_close(nvsHandle);
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
					esp_ble_gatts_cb_param_t *param);
static struct gatts_profile_inst profiles[1] = {
    [0] = {
	.gatts_cb = gatts_profile_event_handler,
	.gatts_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	//.service_id.id.uuid.uuid.uuid128 = (uint8_t *)&(PROFILEUUID[0]),
	.service_id.id.uuid.len = ESP_UUID_LEN_128,
    }};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
					esp_ble_gatts_cb_param_t *param) {
	switch(event) {
	case ESP_GATTS_REG_EVT:
		profiles[0].service_id.is_primary = true;
		profiles[0].service_id.id.inst_id = 0x00;
		profiles[0].service_id.id.uuid.len = ESP_UUID_LEN_128;
		memcpy(&(profiles[0].service_id.id.uuid.uuid.uuid128[0]), &PROFILEUUID,
		       sizeof(profiles[0].service_id.id.uuid.uuid.uuid128));
		ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICENAME));
		ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&advConfig));
		// https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md
		esp_ble_gatts_create_service(gatts_if, &profiles[0].service_id, ch_NUM * 3 + 2); // idq why 6
		break;
	case ESP_GATTS_CREATE_EVT:
		ESP_LOGI(BT_TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status,
			 param->create.service_handle);
		profiles[0].service_handle = param->create.service_handle;

		for(int i = 0; i < ch_NUM; i++) {
			profiles[0].char_uuid[i].len = ESP_UUID_LEN_128;
			memcpy(&(profiles[0].char_uuid[i].uuid.uuid128[0]), &(CHARUUID[i][0]),
			       sizeof(profiles[0].char_uuid[i].uuid.uuid128));
		};

		esp_ble_gatts_start_service(profiles[0].service_handle);

		for(int i = 0; i < ch_NUM - 1; i++) {
			ESP_ERROR_CHECK(esp_ble_gatts_add_char(
			    profiles[0].service_handle, &profiles[0].char_uuid[i],
			    ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			    ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
			    &propvalue[i], &autoResponse));
		};

		ESP_ERROR_CHECK(esp_ble_gatts_add_char(profiles[0].service_handle, &profiles[0].char_uuid[ch_DONE],
						       ESP_GATT_PERM_READ, ESP_GATT_CHAR_PROP_BIT_READ,
						       &propvalue[ch_DONE], NULL));
		break;
	case ESP_GATTS_START_EVT:
		ESP_LOGI(BT_TAG, "Service start, status %d, service_handle %d", param->start.status,
			 param->start.service_handle);
		break;
	case ESP_GATTS_ADD_CHAR_EVT: {
		ESP_LOGI(BT_TAG, "Characteristic add, status %d, attr_handle %d, service_handle %d",
			 param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
		int index = 0;
		for(int i = 0; i < ch_NUM; i++) {
			if(0 ==
			   strncmp((char *)&(param->add_char.char_uuid.uuid.uuid128), (char *)&(CHARUUID[i]), 16)) {
				index = i;
			}
		};
		ESP_LOGI(BT_TAG, "index: %d", index);
		profiles[0].char_handle[index] = param->add_char.attr_handle;
		profiles[0].descr_uuid[index].len = ESP_UUID_LEN_128;
		memcpy(&(profiles[0].descr_uuid[index].uuid.uuid128[0]), &(CHARDECUUID[index][0]),
		       sizeof(profiles[0].char_uuid[index].uuid.uuid128));

		ESP_ERROR_CHECK(esp_ble_gatts_add_char_descr(profiles[0].service_handle, &profiles[0].descr_uuid[index],
							     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL));
		break;
	}
	case ESP_GATTS_ADD_CHAR_DESCR_EVT:
		ESP_LOGI(BT_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d", param->add_char.status,
			 param->add_char.attr_handle, param->add_char.service_handle);
		break;

	case ESP_GATTS_CONNECT_EVT: {
		esp_ble_conn_update_params_t conn_params = {0};
		memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
		/* For the IOS system, please reference the apple official documents about the ble connection parameters
		 * restrictions. */
		conn_params.latency = 0;
		conn_params.max_int = 0x30; // max_int = 0x30*1.25ms = 40ms
		conn_params.min_int = 0x10; // min_int = 0x10*1.25ms = 20ms
		conn_params.timeout = 400;  // timeout = 400*10ms = 4000ms
		ESP_LOGI(BT_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
			 param->connect.conn_id, param->connect.remote_bda[0], param->connect.remote_bda[1],
			 param->connect.remote_bda[2], param->connect.remote_bda[3], param->connect.remote_bda[4],
			 param->connect.remote_bda[5]);
		profiles[0].conn_id = param->connect.conn_id;
		// start sent the update connection parameters to the peer device.
		esp_ble_gap_update_conn_params(&conn_params);
		break;
	}

	case ESP_GATTS_DISCONNECT_EVT:
		ESP_LOGI(BT_TAG, "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
			 ESP_BD_ADDR_HEX(param->disconnect.remote_bda), param->disconnect.reason);
		esp_ble_gap_start_advertising(&advParams);
		break;
	case ESP_GATTS_READ_EVT: {
		ESP_LOGI(BT_TAG, "write event");
		if(param->read.handle == profiles[0].char_handle[ch_DONE]) {

			uint16_t length = 0;
			const uint8_t *pSSID;
			const char *pPASSWD;
			const uint8_t *pCHANNEL;
			const uint8_t *pAUTHMETOD;

			ESP_ERROR_CHECK(
			    esp_ble_gatts_get_attr_value(profiles[0].char_handle[ch_SSID], &length, &pSSID));
			char *SSID = malloc(sizeof(char) * length);
			memset(SSID, 0U, sizeof(char) * length);
			memcpy(SSID, pSSID, length);

			ESP_ERROR_CHECK(esp_ble_gatts_get_attr_value(profiles[0].char_handle[ch_PASSWD], &length,
								     (const uint8_t **)&pPASSWD));
			char *PASSWD = malloc(sizeof(char) * length);
			memset(PASSWD, 0U, sizeof(char) * length);
			memcpy(PASSWD, pPASSWD, length);

			ESP_ERROR_CHECK(esp_ble_gatts_get_attr_value(profiles[0].char_handle[ch_CHANNEL], &length,
								     (const uint8_t **)&pCHANNEL));
			uint8_t *CHANNEL = malloc(sizeof(char) * length);
			memset(CHANNEL, 0U, sizeof(char) * length);
			memcpy(CHANNEL, pCHANNEL, length);

			ESP_ERROR_CHECK(esp_ble_gatts_get_attr_value(profiles[0].char_handle[ch_AUTHMETOD], &length,
								     (const uint8_t **)&pAUTHMETOD));
			char *AUTHMETOD = malloc(sizeof(char) * length);
			memset(AUTHMETOD , 0U, sizeof(char) * length);
			memcpy(AUTHMETOD, pAUTHMETOD, length);

			writeNvs(SSID, PASSWD, *CHANNEL, *AUTHMETOD);
			ESP_LOGD(BT_TAG, "ssid: %s, passwd: %s, auth: %d, channel: %d", SSID, PASSWD, *AUTHMETOD,
				 *CHANNEL);

			free(SSID);
			free(PASSWD);
			free(AUTHMETOD);
			free(CHANNEL);

			ESP_LOGI(BT_TAG, "done bt config");
			esp_gatt_rsp_t rsp = {0};
			rsp.attr_value.handle = param->read.handle;
			rsp.attr_value.len = 1;
			rsp.attr_value.value[0] = 0x00;
			esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK,
						    &rsp);
			xEventGroupSetBits(btEventGroup, BTDONE); // done bt bs
		}
		break;
	}
	default:
		ESP_LOGW(BT_TAG, "not impleneted GATT_profile event: %d", event);
		break;
	};
};

static void gattEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param) {
	switch(event) {
	case ESP_GATTS_REG_EVT:
		if(param->reg.status == ESP_GATT_OK) {
			profiles[0].gatts_if = gatt_if;
		} else {
			ESP_LOGE(BT_TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id,
				 param->reg.status);
			break;
		}
		__attribute__((fallthrough));
	default:
		if(gatt_if == ESP_GATT_IF_NONE || gatt_if == profiles[0].gatts_if) {
			if(profiles[0].gatts_cb) {
				profiles[0].gatts_cb(event, gatt_if, param);
			}
		}
		break;
	}
};

static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	switch(event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&advParams);
		ESP_LOGD(BT_TAG, "ADV_DATA_SET_COMPLETE_EVT set compleat");
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		esp_ble_gap_start_advertising(&advParams);
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
	case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
		ESP_LOGD(BT_TAG, "DEBUG EVENT RECIVED");
		break;

	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		ESP_LOGI(BT_TAG, "update connection params status = %d, conn_int = %d,latency = %d, timeout = %d",
			 param->update_conn_params.status, param->update_conn_params.conn_int,
			 param->update_conn_params.latency, param->update_conn_params.timeout);
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
	btEventGroup = xEventGroupCreate();
	loadNvs(ssid, (size_t *) &(propvalue[ch_SSID].attr_len), pass, (size_t *) &(propvalue[ch_SSID].attr_len), &channel, &authmetod);
	ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));
	EventBits_t bits = xEventGroupWaitBits(btEventGroup, BTDONE, pdTRUE, pdFALSE, portMAX_DELAY);

	esp_bluedroid_disable();
	esp_bluedroid_deinit();
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
};
