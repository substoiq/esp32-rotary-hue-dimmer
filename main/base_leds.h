/* De ledring in de voet: laat zien of de lampen in de zone aan zijn. */
#pragma once
#include <stdbool.h>

void base_leds_start(void);

/* Zacht groen als lights_on true is, anders uit. Alleen aanroepen vanuit de hoofdlus. */
void base_leds_show(bool lights_on);
