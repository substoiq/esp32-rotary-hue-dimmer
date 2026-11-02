/* De regels van de dimmer. Geen hardware of netwerk, dus los te testen. */
#pragma once
#include <stdbool.h>

typedef struct {
    bool on;
    int  brightness;   /* 1 t/m 100 */
} dimmer_state_t;

void dimmer_init(dimmer_state_t *s);

/* Knop: uit -> aan op DIM_ON_PERCENT, aan -> uit. */
void dimmer_press(dimmer_state_t *s);

/* Ring: steps > 0 is rechtsom. Geeft true als er iets veranderde.
 * Staan de lampen uit, dan doet de ring niets. */
bool dimmer_rotate(dimmer_state_t *s, int steps);

/* Stand die van buitenaf komt, bijvoorbeeld uit de Hue-app. */
void dimmer_set_external(dimmer_state_t *s, bool has_on, bool on,
                         bool has_brightness, int brightness);
