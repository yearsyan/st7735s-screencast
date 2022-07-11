#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"

#define BACK_PIN (0)
#define DC_CONTROL_PIN (1)
#define RESET_PIN (2)
#define SCLK_PIN (6)
#define SDA_PIN (5)
#define CS_PIN (10)
#define X_OFFSET (2)
#define Y_OFFSET (3)

void lcd_spi_pre_transfer_callback(spi_transaction_t *t) { 
    gpio_set_level(DC_CONTROL_PIN, (uint32_t)t->user);
}


void lcd_data(spi_device_handle_t spi, uint8_t* data, uint32_t len) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * len;
    t.tx_buffer = data;
    t.user = (void*)1;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void lcd_data_8(spi_device_handle_t spi, uint8_t data) {
    lcd_data(spi, &data, 1);
}

void lcd_data_16(spi_device_handle_t spi, uint16_t data) {
    lcd_data(spi, (uint8_t*)&data, 2);
}

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void*)0;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void lcd_flush_with_data(spi_device_handle_t spi, uint16_t width, uint16_t height, uint16_t* pixels) {
    uint8_t data_arr[4] = {0};

    lcd_cmd(spi, 0x2a);
    data_arr[0] = 0;
    data_arr[1] = X_OFFSET;
    data_arr[2] = ((width-1) >> 8) & 0xff;
    data_arr[3] = ((width-1) & 0xff) + X_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2b);
    data_arr[0] = 0;
    data_arr[1] = Y_OFFSET;
    data_arr[2] = ((height-1) >> 8) & 0xff;
    data_arr[3] = ((height-1) & 0xff) + Y_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2c);
    for(int y = 0;y < height; y++) {
        for (int x = 0;x < width; x++) {
            uint16_t v = pixels[128*y + x];
            lcd_data_8(spi, ((v&0xff00) >> 8));
            lcd_data_8(spi, v & 0xff);
            // lcd_data_16(spi, (v & 0xff << 8) | ((v&0xff00) >> 8));
        }
    }
}

void lcd_flush_with_callback(spi_device_handle_t spi, uint16_t width, uint16_t height, pixel_cb cb) {
    uint8_t data_arr[4] = {0};

    lcd_cmd(spi, 0x2a);
    data_arr[0] = 0;
    data_arr[1] = X_OFFSET;
    data_arr[2] = ((width-1) >> 8) & 0xff;
    data_arr[3] = ((width-1) & 0xff) + X_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2b);
    data_arr[0] = 0;
    data_arr[1] = Y_OFFSET;
    data_arr[2] = ((height-1) >> 8) & 0xff;
    data_arr[3] = ((height-1) & 0xff) + Y_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2c);
    for(int y = 0;y < height; y++) {
        for (int x = 0;x < width; x++) {
            lcd_data_16(spi, cb(x, y));
        }
    }
}

void lcd_draw_area(spi_device_handle_t spi, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    uint8_t data_arr[4] = {0};
    uint32_t pixel_count = (x2 - x1) * (y2 - y1);

    lcd_cmd(spi, 0x2a);
    data_arr[0] = (x1 >> 8) & 0xff;
    data_arr[1] = (x1 & 0xff) + X_OFFSET;
    data_arr[2] = (x2 >> 8) & 0xff;
    data_arr[3] = (x2 & 0xff) + X_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2b);
    data_arr[0] = (y1 >> 8) & 0xff;
    data_arr[1] = (y1 & 0xff) + Y_OFFSET;
    data_arr[2] = (y2 >> 8) & 0xff;
    data_arr[3] = (y2 & 0xff) + Y_OFFSET;
    lcd_data(spi, data_arr, 4);

    lcd_cmd(spi, 0x2c);
    for(int i = 0;i < pixel_count;i++) {
        lcd_data_16(spi, color);
    }
}

spi_device_handle_t lcd_init(void) {
    
    esp_err_t ret;
    spi_device_handle_t spi = NULL;
    spi_bus_config_t bus_config = {
        .miso_io_num = -1,
        .mosi_io_num = SDA_PIN,
        .sclk_io_num = SCLK_PIN,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1
    };

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 40*1000*1000,
        .spics_io_num = CS_PIN,
        .queue_size = 7,
        .pre_cb = lcd_spi_pre_transfer_callback,
        .flags = SPI_DEVICE_NO_DUMMY
    };
    
    gpio_set_direction(CS_PIN, GPIO_MODE_OUTPUT);
    ret = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(SPI2_HOST, &device_config, &spi);
    ESP_ERROR_CHECK(ret);
    

    // 初始化 Data Control 
    gpio_reset_pin(DC_CONTROL_PIN);
    gpio_set_direction(DC_CONTROL_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DC_CONTROL_PIN, 0);

    // reset 
    gpio_reset_pin(RESET_PIN);
    gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 启动背光
    gpio_reset_pin(BACK_PIN);
    gpio_set_direction(BACK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BACK_PIN, 1);

    lcd_cmd(spi, 0x01); // software reset
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(spi, 0x11); // sleep out
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Frame rate
    lcd_cmd(spi, 0xb1);
    lcd_data_8(spi, 0x01);
    lcd_data_8(spi, 0x2c);
    lcd_data_8(spi, 0x2d);
    lcd_cmd(spi, 0xb2);
    lcd_data_8(spi, 0x01);
    lcd_data_8(spi, 0x2c);
    lcd_data_8(spi, 0x2d);
    lcd_cmd(spi, 0xb3);
    lcd_data_8(spi, 0x01);
    lcd_data_8(spi, 0x2c);
    lcd_data_8(spi, 0x2d);
    lcd_data_8(spi, 0x01);
    lcd_data_8(spi, 0x2c);
    lcd_data_8(spi, 0x2d);

    lcd_cmd(spi, 0xb4);
    lcd_data_8(spi, 0x07);


    // Power
    lcd_cmd(spi, 0xc0);
    lcd_data_8(spi, 0xa2);
    lcd_data_8(spi, 0x02);
    lcd_data_8(spi, 0x84);
    lcd_cmd(spi, 0xc1);
    lcd_data_8(spi, 0xc5);
    lcd_cmd(spi, 0xc2);
    lcd_data_8(spi, 0x0a);
    lcd_data_8(spi, 0x00);
    lcd_cmd(spi, 0xc3);
    lcd_data_8(spi, 0x8a);
    lcd_data_8(spi, 0x2a);
    lcd_cmd(spi, 0xc4);
    lcd_data_8(spi, 0x8a);
    lcd_data_8(spi, 0xee);
    lcd_cmd(spi, 0xc5);
    lcd_data_8(spi, 0x0e);

    lcd_cmd(spi, 0x20); //Display Inversion Off 

    lcd_cmd(spi, 0x3a); // Color Mode
    lcd_data_8(spi, 0x05); // 16bit
    
    // lcd_cmd(spi, 0x21);

    // Gamma
    lcd_cmd(spi, 0xe0);
    {
        uint8_t data_arr[16] = {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2b, 0x39, 0x00, 0x01, 0x03, 0x10};
        for(int i = 0;i < 16;i++) {
            lcd_data_8(spi, data_arr[i]);
        }
    }
    lcd_cmd(spi, 0xe1);
    {
         uint8_t data_arr[16] = {0x03, 0x1d, 0x07, 0x06, 0x2e, 0x2c, 0x29, 0x2d, 0x2e, 0x2e, 0x37, 0x3f, 0x00, 0x00, 0x02, 0x10};
        for(int i = 0;i < 16;i++) {
            lcd_data_8(spi, data_arr[i]);
        }
    }

    lcd_cmd(spi, 0x13);
    vTaskDelay(pdMS_TO_TICKS(20));
    lcd_cmd(spi, 0x29);
    vTaskDelay(pdMS_TO_TICKS(100));

    lcd_cmd(spi, 0x36);
    lcd_data_8(spi, 0xc8);
   
    return spi;

}