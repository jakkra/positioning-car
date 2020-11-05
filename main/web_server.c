#include "web_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <esp_system.h>
#include <esp_event.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "esp_timer.h"

#include "car_config.h"

// Browsers don't like unsecure websockets
//#define ROVER_WS_SSL

#include "config.h"
#ifdef ROVER_WS_SSL
#include "esp_https_server.h"
#endif

#define WS_SERVER_PORT          80
#define MAX_WS_INCOMING_SIZE    100
#define WS_CONNECT_MESSAGE      "CONNECT"
#define MAX_WS_CONNECTIONS      5
#define INVALID_FD              -1
#define TX_BUF_SIZE             100

typedef struct ws_client {
    int     fd;
    bool    tx_in_progress;
} ws_client;

typedef struct web_server {
    httpd_handle_t                  handle;
    bool                            running;
    uint8_t                         tx_buf[TX_BUF_SIZE];
    uint16_t                        tx_buf_len;
    uint16_t                        channel_values[RC_NUM_CHANNELS];
    websocket_connection_status*    callback;
    bool                            client_connected;
} web_server;


static void on_client_disconnect(httpd_handle_t hd, int sockfd);
static esp_err_t http_resp_send_index(httpd_req_t *req);
static esp_err_t http_resp_send_joystick(httpd_req_t *req);
static void reset_ch_values(void);

static esp_err_t ws_handler(httpd_req_t *req);

static const httpd_uri_t ws = {
    .uri        = "/ws",
    .method     = HTTP_GET,
    .handler    = ws_handler,
    .user_ctx   = NULL,
    .is_websocket = true
};

static httpd_uri_t index_download = {
    .uri       = "/",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = http_resp_send_index,
    .user_ctx  = NULL
};

static httpd_uri_t joystick_download = {
    .uri       = "/virtualjoystick.js",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = http_resp_send_joystick,
    .user_ctx  = NULL
};

static const char *TAG = "web_server";

static web_server server;


void webserver_init(websocket_connection_status* cb)
{
    memset(&server, 0, sizeof(web_server));
    server.running = false;
    ESP_LOGI(TAG, "webserver_init");
    server.handle = NULL;
    server.client_connected = false;
    server.callback = cb;
    reset_ch_values();
}

void webserver_start(void)
{
    assert(!server.running);
    ESP_LOGI(TAG, "webserver_start");
    esp_err_t err;

#ifdef ROVER_WS_SSL
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");
    config.cacert_pem = cacert_pem_start;
    config.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    config.prvtkey_pem = prvtkey_pem_start;
    config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    config.httpd.server_port = WS_SERVER_PORT;
    config.httpd.close_fn = on_client_disconnect;
    config.httpd.max_open_sockets = MAX_WS_CONNECTIONS;

    err = httpd_ssl_start(&server.handle, &config);
    assert(err == ESP_OK);

    err = httpd_register_uri_handler(server.handle, &ws);
    assert(err == ESP_OK);
    server.running = true;
    ESP_LOGI(TAG, "Web Server started on port %d, server handle %p", config.httpd.server_port, server.handle);
#else
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = WS_SERVER_PORT;
    config.close_fn = on_client_disconnect;
    config.max_open_sockets = MAX_WS_CONNECTIONS;
    err = httpd_start(&server.handle, &config);
    assert(err == ESP_OK);

    err = httpd_register_uri_handler(server.handle, &ws);
    assert(err == ESP_OK);
    err = httpd_register_uri_handler(server.handle, &index_download);
    assert(err == ESP_OK);
    err = httpd_register_uri_handler(server.handle, &joystick_download);
    assert(err == ESP_OK);

    server.running = true;
    ESP_LOGI(TAG, "Web Server started on port %d, server handle %p", config.server_port, server.handle);
#endif
    
}

static void on_client_disconnect(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "Client disconnected");
    server.client_connected = false;
    server.callback(WEBSOCKET_STATUS_DISCONNECTED);
}

static void reset_ch_values() {
    for (uint8_t i = 0; i < RC_NUM_CHANNELS; i++) {
        server.channel_values[i] = config_default_ch_values[i];
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    assert(server.handle == req->handle);
    uint8_t buf[MAX_WS_INCOMING_SIZE] = { 0 };
    httpd_ws_frame_t packet;
    
    memset(&packet, 0, sizeof(httpd_ws_frame_t));
    packet.payload = buf;

    esp_err_t ret = httpd_ws_recv_frame(req, &packet, MAX_WS_INCOMING_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }

    if (packet.type == HTTPD_WS_TYPE_BINARY) {
        if (packet.len >= RC_NUM_CHANNELS * sizeof(uint16_t)) {
            uint16_t* values = (uint16_t*)packet.payload;
            for (uint8_t i = 0; i < RC_NUM_CHANNELS; i++) {
                if (values[i] >= 1000 && values[i] <= 2000) {
                    server.channel_values[i] = values[i];
                } else {
                    ESP_LOGE(TAG, "Expected channel values to be in range 1000 - 2000 but was: %d\n", values[i]);
                    server.channel_values[i] = config_default_ch_values[i];
                }
            }
            if (!server.client_connected) {
                ESP_LOGI(TAG, "Client connected");
                server.client_connected = true;
                server.callback(WEBSOCKET_STATUS_CONNECTED);
            }
        } else {
            ESP_LOGI(TAG, "Invalid binary length");
            reset_ch_values();
        }
    }
    printf("%d, %d \t %d, %d \t %d, %d\n", server.channel_values[0], server.channel_values[1], server.channel_values[2],  server.channel_values[3], server.channel_values[4], server.channel_values[5]);    
   
    return ESP_OK;
}

static esp_err_t http_resp_send_index(httpd_req_t *req)
{
    extern const unsigned char index_start[] asm("_binary_index_html_start");
    extern const unsigned char index_end[]   asm("_binary_index_html_end");
    const size_t index_size = (index_end - index_start);

    httpd_resp_send_chunk(req, (const char *)index_start, index_size);

    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t http_resp_send_joystick(httpd_req_t *req)
{
    extern const unsigned char joystick_start[] asm("_binary_virtualjoystick_js_start");
    extern const unsigned char joystick_end[]   asm("_binary_virtualjoystick_js_end");
    const size_t joystick_size = (joystick_end - joystick_start);

    httpd_resp_send_chunk(req, (const char *)joystick_start, joystick_size);

    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

uint16_t web_server_controller_get_value(uint8_t channel)
{
    assert(server.running);
    assert(channel < RC_NUM_CHANNELS);
    return server.channel_values[channel];
}
