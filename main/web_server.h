#pragma once

#include "stdint.h"
#include "esp_err.h"

typedef enum websocket_status_t
{
  WEBSOCKET_STATUS_CONNECTED,
  WEBSOCKET_STATUS_DISCONNECTED,
} websocket_status_t;

typedef void(websocket_connection_status(websocket_status_t status));

void webserver_init(websocket_connection_status* cb);
void webserver_start(void);
uint16_t web_server_controller_get_value(uint8_t channel);
esp_err_t webserver_ws_send(uint8_t* payload, uint32_t len);

