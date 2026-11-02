#include "display_port.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

#define PIN_POWER_LCD   1
#define PIN_POWER_LEDA  2
#define PIN_SCLK        10
#define PIN_MOSI        11
#define PIN_DC          3
#define PIN_CS          9
#define PIN_RST         14
#define PIN_BACKLIGHT   46

#define SCREEN_SIZE     240
#define BUFFER_LINES    40
#define BUFFER_BYTES    (SCREEN_SIZE * BUFFER_LINES * 2)
#define SPI_CLOCK_HZ    (40 * 1000 * 1000)

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_lock;

void display_lock(void)   { xSemaphoreTakeRecursive(s_lock, portMAX_DELAY); }
void display_unlock(void) { xSemaphoreGiveRecursive(s_lock); }

/* Wordt aangeroepen als de SPI-overdracht klaar is. */
static bool on_transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *data, void *user)
{
    lv_display_flush_ready((lv_display_t *)user);
    return false;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    /* Het scherm verwacht de twee bytes van elke pixel in omgekeerde volgorde. */
    lv_draw_sw_rgb565_swap(pixels, lv_area_get_size(area));
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
}

static uint32_t tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_task(void *arg)
{
    for (;;) {
        display_lock();
        uint32_t wait_ms = lv_timer_handler();
        display_unlock();
        if (wait_ms < 5) wait_ms = 5;
        if (wait_ms > 50) wait_ms = 50;
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

static void power_on(void)
{
    /* Deze twee pinnen schakelen de voeding van het scherm. Zonder dit blijft het zwart. */
    gpio_config_t power = {
        .pin_bit_mask = (1ULL << PIN_POWER_LCD) | (1ULL << PIN_POWER_LEDA),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&power));
    gpio_set_level(PIN_POWER_LCD, 1);
    gpio_set_level(PIN_POWER_LEDA, 1);
}

static void backlight_on(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t channel = {
        .gpio_num = PIN_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1023 * BACKLIGHT_PERCENT / 100,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void display_start(void)
{
    s_lock = xSemaphoreCreateRecursiveMutex();
    power_on();

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUFFER_BYTES,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    lv_init();
    lv_tick_set_cb(tick_ms);
    lv_display_t *display = lv_display_create(SCREEN_SIZE, SCREEN_SIZE);

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_transfer_done,
        .user_ctx = display,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_config, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    void *buffer1 = heap_caps_malloc(BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buffer2 = heap_caps_malloc(BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buffer1 && buffer2);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer1, buffer2, BUFFER_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    backlight_on();
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, NULL, 5, NULL, 1);
}
