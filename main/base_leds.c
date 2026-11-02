#include "base_leds.h"
#include "app_config.h"
#include "esp_log.h"
#include "led_strip.h"

#define PIN_LEDS   48
#define LED_COUNT  5

static const char *TAG = "base_leds";
static led_strip_handle_t s_strip;
static int s_shown = -1;   /* -1 = nog niets getoond, anders 0 of 1 */

void base_leds_start(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_LEDS,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    if (led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip) != ESP_OK) {
        ESP_LOGE(TAG, "ledring niet gestart");
        s_strip = NULL;
        return;
    }
    base_leds_show(false);
}

void base_leds_show(bool lights_on)
{
    /* Alleen naar de ring schrijven als er echt iets verandert. */
    if (s_strip == NULL || s_shown == (int)lights_on) {
        return;
    }
    s_shown = lights_on;

    for (int i = 0; i < LED_COUNT; i++) {
        led_strip_set_pixel(s_strip, i, 0, lights_on ? BASE_LED_GREEN : 0, 0);
    }
    led_strip_refresh(s_strip);
}
