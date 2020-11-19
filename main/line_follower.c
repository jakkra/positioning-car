
#include "line_follower.h"
#include "esp_log.h"

#include "car_config.h"
#include "driver/gpio.h"

#define STEER_SPEED     5

typedef enum follower_state_t {
    STATE_TURNING_LEFT,
    STATE_TURNING_RIGHT,
    STATE_FORWARD,
    STATE_STOP
} follower_state_t;

static char TAG[] = "line_follower";

static void configure_gpios(void);

static follower_state_t state = STATE_FORWARD;
static uint16_t current_steer_value = 1500;

void line_follower_init(void) {
    configure_gpios();
}

uint16_t line_follower_get_value(uint16_t channel) {
    assert(channel == RC_STEER_CHANNEL); // For now
    uint16_t left = !gpio_get_level(IR_SENSOR_LEFT_PIN);
    uint16_t right = !gpio_get_level(IR_SENSOR_RIGHT_PIN);
    ESP_LOGI(TAG, "Left: %d, Right: %d\n", left, right);

    follower_state_t new_state;

    // Turn left
    if (left && !right) {
        printf("LEFT\n");
        new_state = STATE_TURNING_LEFT;
    }

    // Turn right
    if (!left && right) {
        printf("RIGHT\n");
        new_state = STATE_TURNING_RIGHT;
    }

    if (left && right) {
        printf("FORWARD\n");
        new_state = STATE_FORWARD;
    }

    /*if (!left && !right) {
        printf("IDLE\n");
        new_state = STATE_FORWARD;
    }*/

    if (new_state == state) {
        if (new_state == STATE_TURNING_LEFT) current_steer_value -= STEER_SPEED;
        if (new_state == STATE_TURNING_RIGHT) current_steer_value += STEER_SPEED;
        if (new_state == STATE_FORWARD) current_steer_value = 1500;
        state = new_state;
    } else {
        if (new_state == STATE_TURNING_LEFT) current_steer_value = 1300;
        if (new_state == STATE_TURNING_RIGHT) current_steer_value = 1700;
        if (new_state == STATE_FORWARD) current_steer_value = 1500;
        state = new_state;
    }


    if (current_steer_value > 2000) current_steer_value = 2000;
    if (current_steer_value < 1000) current_steer_value = 1000;
    
    ESP_LOGI(TAG, "Steer: %d", current_steer_value);
    return current_steer_value;
}

static void configure_gpios(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << (IR_SENSOR_LEFT_PIN | IR_SENSOR_RIGHT_PIN);
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}
