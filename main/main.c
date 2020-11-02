#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "esp_log.h"

#include "rc_receiver_rmt.h"
#include "car_config.h"

static char TAG[] = "rc_car";

#define MIN_STEER_VAL 	544
#define MAX_STEER_VAL	2500

const uint16_t config_default_ch_values[RC_NUM_CHANNELS] = { RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER, RC_CENTER };

uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max);


void driving_task(void *ignore) {
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
		uint16_t rc_value = rc_receiver_rmt_get_val(RC_STEER_CHANNEL);
		uint32_t duty_us = map(rc_value, 1000, 2000, MIN_STEER_VAL, MAX_STEER_VAL);
		mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, duty_us);

		rc_value = rc_receiver_rmt_get_val(RC_MOTOR_CHANNEL);
		mcpwm_set_duty_in_us(MCPWM_UNIT_1, MCPWM_TIMER_0, MCPWM_OPR_A, rc_value);
		rc_value = rc_receiver_rmt_get_val(RC_MODE_SW2_CHANNEL);
		if (rc_value < 1400) {
			gpio_set_level(SWITCH_CONTROL_BUTTON_PIN, 0);
		} else {
			gpio_set_level(SWITCH_CONTROL_BUTTON_PIN, 1);
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void configure_switch_gpio(void) {
	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << SWITCH_CONTROL_BUTTON_PIN;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

void app_main() {
	rc_receiver_rmt_init();
	configure_switch_gpio();
	xTaskCreate(&driving_task,"receiver_task", 2048, NULL, 5, NULL);
	ESP_LOGW(TAG, "Started and running\n");
	printf("hejsansass\n");
}


uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}