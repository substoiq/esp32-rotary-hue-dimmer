#include "input.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

#define PIN_ENCODER_A  45
#define PIN_ENCODER_B  42
#define PIN_BUTTON     41   /* laag als de knop is ingedrukt */
#define DEBOUNCE_READS 3    /* 3 x 10 ms gelijk = stabiel */

static pcnt_unit_handle_t s_unit;
static int s_counts;

void input_start(void)
{
    /* De PCNT-hardware telt de pulsen van de ring, zodat we er geen missen. */
    pcnt_unit_config_t unit_config = { .high_limit = 1000, .low_limit = -1000 };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &s_unit));

    pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_unit, &filter));

    pcnt_chan_config_t a_config = { .edge_gpio_num = PIN_ENCODER_A, .level_gpio_num = PIN_ENCODER_B };
    pcnt_chan_config_t b_config = { .edge_gpio_num = PIN_ENCODER_B, .level_gpio_num = PIN_ENCODER_A };
    pcnt_channel_handle_t a, b;
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &a_config, &a));
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &b_config, &b));
    pcnt_channel_set_edge_action(a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    gpio_pullup_en(PIN_ENCODER_A);
    gpio_pullup_en(PIN_ENCODER_B);

    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));

    gpio_config_t button = {
        .pin_bit_mask = 1ULL << PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
}

void input_read(int *steps, bool *pressed)
{
    int count = 0;
    pcnt_unit_get_count(s_unit, &count);
    pcnt_unit_clear_count(s_unit);
    s_counts += ENCODER_REVERSE ? -count : count;

    *steps = s_counts / ENCODER_COUNTS_PER_STEP;
    s_counts -= *steps * ENCODER_COUNTS_PER_STEP;

    /* Ontdenderen: de knop telt pas als hij een paar metingen lang hetzelfde is. */
    static bool stable_down = false;
    static int same_reads = 0;
    bool down = gpio_get_level(PIN_BUTTON) == 0;

    *pressed = false;
    if (down == stable_down) {
        same_reads = 0;
    } else if (++same_reads >= DEBOUNCE_READS) {
        stable_down = down;
        same_reads = 0;
        *pressed = down;
    }
}
