#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lcd.h"
#include "server.h"


#define BUTTON_GPIO (9)
#define BLINK_GPIO (19)
#define BLINK_GPIO2 (18)

spi_device_handle_t lcd_spi = NULL;

void image_callback(uint16_t* buff) {
    lcd_flush_with_data(lcd_spi, 128, 128, buff);
}

void app_main(void)
{
    int led_level = 0;
    
    gpio_reset_pin(BLINK_GPIO);
    gpio_reset_pin(BLINK_GPIO2);
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_GPIO2, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_level(BLINK_GPIO, 0);
    gpio_set_level(BLINK_GPIO2, led_level);

    lcd_spi = lcd_init();
    lcd_draw_area(lcd_spi, 0, 0, 127, 127, 0x0000);
    lcd_draw_area(lcd_spi, 20, 20, 40, 40, 0xF800);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    set_recevie_one_image_callback(image_callback);
    init_server();

    while (1)
    {
        int level = gpio_get_level(BUTTON_GPIO);
        if (level != led_level) {
            led_level = level;
            gpio_set_level(BLINK_GPIO2, !level);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
}
