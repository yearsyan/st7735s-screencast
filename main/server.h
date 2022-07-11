#pragma once
#include "esp_wifi.h"

void set_recevie_one_image_callback(void (*cb)(uint16_t* buff));
esp_netif_t* init_server();