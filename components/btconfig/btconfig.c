#include "nvs.h"
#include "esp_bt.h"

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

};
