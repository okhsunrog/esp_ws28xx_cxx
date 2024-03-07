#ifndef MAIN_ESP_WS28XX_H_
#define MAIN_ESP_WS28XX_H_

#include "FastLED.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <stdio.h>
#include <string.h>
#include <sys/_stdint.h>

typedef struct {
    spi_host_device_t host;
    spi_device_handle_t spi;
    int dma_chan;
    spi_device_interface_config_t devcfg;
    spi_bus_config_t buscfg;
} spi_settings_t;

typedef enum {
    WS2812B = 0,
    WS2815,
} led_strip_model_t;

esp_err_t ws28xx_init(int pin, led_strip_model_t model, int num_of_leds,
                      CRGB *led_buffer_ptr);
void ws28xx_fill(CRGB color, int from, int to);
void ws28xx_fill_all(CRGB color);
void ws28xx_set_brightness(uint8_t brightness);
void ws28xx_apply_color_cor(bool apply_flag);
esp_err_t ws28xx_update();

#endif /* MAIN_ESP_WS28XX_H_ */
