#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "string.h"

#include "rc_receiver_rmt.h"
#include "web_server.h"
#include "car_config.h"

static char TAG[] = "rc_car";

#define MIN_STEER_VAL 	544
#define MAX_STEER_VAL	2500

const uint16_t config_default_ch_values[RC_NUM_CHANNELS] = { RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER };

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);


static bool websocket_connected = false;

static uint16_t get_channel_value(uint16_t channel) {
    uint16_t value;

    if (websocket_connected) {
        value = web_server_controller_get_value(channel);
    } else {
        value = rc_receiver_rmt_get_val(RC_STEER_CHANNEL);
    }

    return value;
}

static void driving_task(void *ignore) {
	mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, STEER_SERVO_PIN);
	mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, MOTOR_PIN);

	mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
	mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &pwm_config);

	while (true) {
		uint16_t rc_value = get_channel_value(RC_STEER_CHANNEL);
		uint32_t duty_us = map(rc_value, 1000, 2000, MIN_STEER_VAL, MAX_STEER_VAL);
		mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty_us);

		rc_value = get_channel_value(RC_MOTOR_CHANNEL);
		mcpwm_set_duty_in_us(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_OPR_A, rc_value);
		rc_value = get_channel_value(RC_MODE_SW2_CHANNEL);
		if (rc_value < 1400) {
			gpio_set_level(SWITCH_CONTROL_BUTTON_PIN, 0);
		} else {
			gpio_set_level(SWITCH_CONTROL_BUTTON_PIN, 1);
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

static void configure_switch_gpio(void) {
	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << SWITCH_CONTROL_BUTTON_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    
    switch (event_id) {
        case WIFI_EVENT_AP_START:
            webserver_start();
            break;
        case WIFI_EVENT_AP_STOP:
            break;
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGW(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGW(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
            break;
        }
    }
}

static void start_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 3,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    if (strlen(AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", AP_SSID, AP_PASS);
}

static void handle_websocket_status(websocket_status_t status) {
    if (status == WEBSOCKET_STATUS_CONNECTED) {
        websocket_connected = true;
    } else if (status == WEBSOCKET_STATUS_DISCONNECTED) {
        websocket_connected = false;
    } else {
        assert(false); // Unhandled
    }
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	rc_receiver_rmt_init();
	configure_switch_gpio();
	xTaskCreate(&driving_task,"receiver_task", 2048, NULL, 5, NULL);
    
	webserver_init(&handle_websocket_status);
	start_ap();
	ESP_LOGW(TAG, "Started and running\n");
}

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

