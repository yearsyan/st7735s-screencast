#pragma once
#include "driver/spi_master.h"


typedef uint16_t (*pixel_cb)(uint16_t x, uint16_t y);
spi_device_handle_t lcd_init();
void lcd_flush_with_callback(spi_device_handle_t spi, uint16_t width, uint16_t height, pixel_cb cb);
void lcd_flush_with_data(spi_device_handle_t spi, uint16_t width, uint16_t height, uint16_t* pixels);
void lcd_draw_area(spi_device_handle_t spi, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);