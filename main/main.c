/*
 * Hue-dimmer voor de Elecrow CrowPanel 1.28" Rotary Display.
 *
 * De hoofdlus is de enige plek die de stand van de dimmer wijzigt:
 *   knop en ring  -> stand aanpassen -> scherm bijwerken -> opdracht naar de bridge
 *   bridge        -> stand aanpassen -> scherm bijwerken
 */
#include "app_config.h"
#include "dimmer.h"
#include "display_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hue.h"
#include "input.h"
#include "ui.h"
#include "wifi.h"

static dimmer_state_t s_state;
static bool s_connected;
static const char *s_error;

static void refresh_screen(void)
{
    display_lock();
    ui_update(&s_state, s_connected, s_error);
    display_unlock();
}

void app_main(void)
{
    dimmer_init(&s_state);

    display_start();
    display_lock();
    ui_create();
    display_unlock();

    input_start();
    wifi_start();

    QueueHandle_t events = xQueueCreate(16, sizeof(hue_event_t));
    hue_start(events);

    int64_t last_local_ms = 0;

    for (;;) {
        int steps;
        bool pressed;
        input_read(&steps, &pressed);
        int64_t now_ms = esp_timer_get_time() / 1000;

        /* Zonder verbinding kennen we de echte stand niet, dus dan doet de bediening niets. */
        if (s_connected) {
            bool changed = false;
            if (pressed) {
                dimmer_press(&s_state);
                changed = true;
            }
            if (dimmer_rotate(&s_state, steps)) {
                changed = true;
            }
            if (changed) {
                last_local_ms = now_ms;
                refresh_screen();              /* het scherm reageert direct */
                hue_set(s_state.on, s_state.brightness);
            }
        }

        hue_event_t event;
        while (xQueueReceive(events, &event, 0) == pdTRUE) {
            if (event.type == HUE_EVT_STATUS) {
                s_connected = event.connected;
                s_error = event.error;
            } else if (now_ms - last_local_ms > HUE_ECHO_IGNORE_MS) {
                /* Vlak na eigen bediening komen onze eigen opdrachten terug.
                 * Die negeren we, anders springt de stand terug tijdens het draaien. */
                dimmer_set_external(&s_state, event.light.has_on, event.light.on,
                                    event.light.has_brightness, event.light.brightness);
            }
            refresh_screen();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
