#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

int retry_num = 0;

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if(event_id == WIFI_EVENT_STA_START) {
		printf("WiFi Connecting ... \n");
	} else if(event_id == WIFI_EVENT_STA_CONNECTED) {
		printf("WiFi Connected!\n");
	} else if(event_id == WIFI_EVENT_STA_DISCONNECTED) {
		printf("WiFi lost connection\n");
		if(retry_num<5) {
			esp_wifi_connect();
			retry_num++;
			printf("Retrying to connect... \n");
		}	
	} else if(event_id == IP_EVENT_STA_GOT_IP) {
		printf("WiFi got an IP...\n\n");
	}
}

void wifi_connection() {
	esp_netif_init();
	esp_event_loop_create_default();

	esp_netif_create_default_wifi_sta();
	wifi_init_config_t wifi_initialization = WIFI_INIT_CONFIG_DEFAULT();

	esp_wifi_init(&wifi_initialization);
	
	esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
	esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

	wifi_config_t wifi_configuration = {
		.sta = {
			.ssid = "",
			.password = "",
		}
	};

	strcpy((char*)wifi_configuration.sta.ssid, CONFIG_WIFI_SSID);
	strcpy((char*)wifi_configuration.sta.password, CONFIG_WIFI_PASS);

	esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
	esp_wifi_start();
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_connect();

	printf("wifi_init_softap finished. SSID: %s password: %s\n\n", CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);
}

void app_main(void) {
	nvs_flash_init();
	wifi_connection();	
}

