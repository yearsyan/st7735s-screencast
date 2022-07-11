#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "server.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"

#define AP_MAX (12)


httpd_handle_t start_http_server();
static esp_err_t img_handler(httpd_req_t *req);
static esp_err_t index_handler(httpd_req_t *req);
static void (*recevie_one_image_callback)(uint16_t* buff) = NULL;

static httpd_uri_t post_img = {
    .uri       = "/img",
    .method    = HTTP_POST,
    .handler   = img_handler,
};

static httpd_uri_t index_page = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
};

static void on_got_ip(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data) {
    gpio_set_level(19, 1);
    ESP_LOGI("info", "got ipv4 address");
    start_http_server();
}

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data) {
    ESP_LOGW("info", "wifi connect!"); 
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    ESP_LOGW("info", "wifi disconnect try reconnect");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_wifi_connect();
}

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_send(req, "Hello", 5);
    return ESP_OK;
}

static esp_err_t img_handler(httpd_req_t *req) {
    int ret,recv_count = 0, remaining = req->content_len;
    uint16_t* buf = (uint16_t*)malloc(128*128*sizeof(uint16_t));
    while (remaining > 0) {
        ret = httpd_req_recv(req, ((char*)buf) + recv_count, 128*128*2);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        recv_count += ret;
        remaining -=ret;
    }

    if (recevie_one_image_callback != NULL) {
        recevie_one_image_callback(buf);
    }
    
    httpd_resp_send(req, "DONE", 4);
    free(buf);
    return ESP_OK;
}

esp_netif_t* init_server() {
     char *desc = "INIT WIFI";
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "WRT",
            .password = "qwer1234",
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .threshold.rssi = -100
        }
    };
    
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    
    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    
    esp_wifi_set_default_wifi_sta_handlers();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_connect();
    return netif;
}

httpd_handle_t start_http_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port = 80;

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_register_uri_handler(server, &post_img);
    httpd_register_uri_handler(server, &index_page);
    ESP_LOGW("info", "http server start done"); 
    return server;
}

void set_recevie_one_image_callback(void (*cb)(uint16_t* buff)) {
    recevie_one_image_callback = cb;
}