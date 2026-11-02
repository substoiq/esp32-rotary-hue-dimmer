/* Praat met de Hue Bridge via CLIP v2. Draait in eigen taken en meldt alles via een queue. */
#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "hue_json.h"

typedef enum { HUE_EVT_STATUS, HUE_EVT_LIGHT } hue_event_type_t;

typedef struct {
    hue_event_type_t type;
    bool        connected;   /* HUE_EVT_STATUS */
    const char *error;       /* HUE_EVT_STATUS: korte tekst als connected false is */
    hue_light_t light;       /* HUE_EVT_LIGHT */
} hue_event_t;

void hue_start(QueueHandle_t events);

/* Vraagt een nieuwe stand aan. Komen er snel meerdere achter elkaar, dan wordt
 * alleen de laatste verstuurd. */
void hue_set(bool on, int brightness);
