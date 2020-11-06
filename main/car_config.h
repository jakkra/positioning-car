#pragma once

#define RC_NUM_CHANNELS   6

#define RC_LOW    1000
#define RC_CENTER 1500
#define RC_HIGH   2000

#define RC_CH1  0
#define RC_CH2  1
#define RC_CH3  2
#define RC_CH4  3
#define RC_CH5  4
#define RC_CH6  5

// Input pins connected to each channel on RC Receiver
#define RC_CH1_INPUT            39
#define RC_CH2_INPUT            34
#define RC_CH3_INPUT            25
#define RC_CH4_INPUT            26
#define RC_CH5_INPUT            27
#define RC_CH6_INPUT            23

#define RC_STEER_CHANNEL        RC_CH1
#define RC_MOTOR_CHANNEL        RC_CH2

#define RC_MODE_SW1_CHANNEL     RC_CH5
#define RC_MODE_SW2_CHANNEL     RC_CH6

#define MOTOR_PIN               32
#define STEER_SERVO_PIN         33

#define SWITCH_CONTROL_BUTTON_PIN 22

#define AP_SSID                             "DF_CAR"
#define AP_PASS                             ""

#define WIFI_STA_SSID                       "lolololol"
#define WIFI_STA_PASSWORD                   "password"

extern const uint16_t config_default_ch_values[RC_NUM_CHANNELS];