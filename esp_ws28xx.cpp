#include "esp_ws28xx.h"
#include <sys/_stdint.h>

uint16_t *dma_buffer;
CRGB *ws28xx_pixels;
static int n_of_leds, reset_delay;
static size_t dma_buf_size;
led_strip_model_t led_model;
uint8_t leds_brightness = 255;
bool apply_color_cor = false;

static spi_settings_t spi_settings = {
    .host = SPI2_HOST,
    .dma_chan = SPI_DMA_CH_AUTO,
    .devcfg =
        {
            .command_bits = 0,
            .address_bits = 0,
            .mode = 0, // SPI mode 0
            .clock_speed_hz =
                static_cast<int>(3.2 * 1000 * 1000), // Clock out at 3.2 MHz
            .spics_io_num = -1,                      // CS pin
            .flags = SPI_DEVICE_TXBIT_LSBFIRST,
            .queue_size = 1,
        },
    .buscfg =
        {
            .miso_io_num = -1,
            .sclk_io_num = -1,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        },
};

static const uint16_t timing_bits[16] = {
    0x1111, 0x7111, 0x1711, 0x7711, 0x1171, 0x7171, 0x1771, 0x7771,
    0x1117, 0x7117, 0x1717, 0x7717, 0x1177, 0x7177, 0x1777, 0x7777};

esp_err_t ws28xx_init(int pin, led_strip_model_t model, int num_of_leds,
                      CRGB *led_buffer_ptr) {
    esp_err_t err = ESP_OK;
    n_of_leds = num_of_leds;
    led_model = model;
    // 12 bytes for each led + bytes for initial zero and reset state
    dma_buf_size = n_of_leds * 12 + (reset_delay + 1) * 2;
    ws28xx_pixels = led_buffer_ptr;
    spi_settings.buscfg.mosi_io_num = pin;
    spi_settings.buscfg.max_transfer_sz = dma_buf_size;
    err = spi_bus_initialize(spi_settings.host, &spi_settings.buscfg,
                             spi_settings.dma_chan);
    if (err != ESP_OK) {
        free(ws28xx_pixels);
        return err;
    }
    err = spi_bus_add_device(spi_settings.host, &spi_settings.devcfg,
                             &spi_settings.spi);
    if (err != ESP_OK) {
        free(ws28xx_pixels);
        return err;
    }
    // Insrease if something breaks. values are less than recommended in
    // datasheets but seem stable
    reset_delay = (model == WS2812B) ? 3 : 30;
    // Critical to be DMA memory.
    dma_buffer = (uint16_t *)heap_caps_malloc(dma_buf_size, MALLOC_CAP_DMA);
    if (dma_buffer == NULL) {
        free(ws28xx_pixels);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void ws28xx_fill(CRGB color, int from, int to) {
    for (int i = from; i < to; i++) {
        ws28xx_pixels[i] = color;
    }
}

void ws28xx_fill_all(CRGB color) { ws28xx_fill(color, 0, n_of_leds); }

void ws28xx_set_brightness(uint8_t brightness) { leds_brightness = brightness; }

void ws28xx_apply_color_cor(bool apply_flag) { apply_color_cor = apply_flag; }

esp_err_t ws28xx_update() {
    spi_transaction_t transmission = {};
    esp_err_t err;
    int n = 0;
    memset(dma_buffer, 0, dma_buf_size);
    dma_buffer[n++] = 0;
    for (int i = 0; i < n_of_leds; i++) {
        if (leds_brightness != 255) {
            ws28xx_pixels[i].r *= (((float)leds_brightness) / 255);
            ws28xx_pixels[i].g *= (((float)leds_brightness) / 255);
            ws28xx_pixels[i].b *= (((float)leds_brightness) / 255);
        }
        // Values for TypicalLEDStrip from FastLED lib
        if (apply_color_cor) {
            ws28xx_pixels[i].g *= (((float)176) / 255);
            ws28xx_pixels[i].b *= (((float)240) / 255);
        }
        // Data you want to write to each LEDs
        if (led_model == WS2815) {
            // Red
            dma_buffer[n++] = timing_bits[0x0f & (ws28xx_pixels[i].r >> 4)];
            dma_buffer[n++] = timing_bits[0x0f & ws28xx_pixels[i].r];

            // Green
            dma_buffer[n++] = timing_bits[0x0f & (ws28xx_pixels[i].g >> 4)];
            dma_buffer[n++] = timing_bits[0x0f & ws28xx_pixels[i].g];
        } else {
            // Green
            dma_buffer[n++] = timing_bits[0x0f & (ws28xx_pixels[i].g >> 4)];
            dma_buffer[n++] = timing_bits[0x0f & ws28xx_pixels[i].g];

            // Red
            dma_buffer[n++] = timing_bits[0x0f & (ws28xx_pixels[i].r >> 4)];
            dma_buffer[n++] = timing_bits[0x0f & ws28xx_pixels[i].r];
        }
        // Blue
        dma_buffer[n++] = timing_bits[0x0f & (ws28xx_pixels[i].b >> 4)];
        dma_buffer[n++] = timing_bits[0x0f & ws28xx_pixels[i].b];
    }
    for (int i = 0; i < reset_delay; i++) {
        dma_buffer[n++] = 0;
    }
    transmission.length = dma_buf_size * 8;
    transmission.tx_buffer = dma_buffer;
    err = spi_device_transmit(spi_settings.spi, &transmission);
    return err;
}
