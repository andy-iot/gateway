#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"

#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "lora.h"

int retry_num = 0;
uint8_t buf[255];
static const char *TAG = "andy-iot-gateway";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) 
{
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

void wifi_connection() 
{
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

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) 
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void task_rx(void *p)
{
    int x;
    for(;;) {
        lora_receive();    // put into receive mode
        while(lora_received()) {
            x = lora_receive_packet(buf, sizeof(buf));
            buf[x] = 0;
            int rssi = lora_packet_rssi();
            printf("Received: %s, RSSI: %d\n", buf, rssi);
            lora_receive();
        }
        vTaskDelay(1);
    }
}


/**
 *
 *
 */
void app_main(void) 
{

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
	
	nvs_flash_init();
	wifi_connection();

	ESP_LOGI(TAG, "[APP] Attempting to connect to MQTT Broker");
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = CONFIG_BROKER_URL,
	};

	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);

    lora_init();
    lora_set_frequency(433e6);
    lora_enable_crc();

    printf("Lora Initialized: %d\n", lora_initialized());

    xTaskCreate(task_rx, "task_rx", 2048, NULL, 5, NULL);

}

