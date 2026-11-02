/* De draaiing van de ring en de drukknop. */
#pragma once
#include <stdbool.h>

void input_start(void);

/* Elke 10 ms aanroepen. steps: aantal klikken sinds de vorige keer (rechtsom positief).
 * pressed: true op het moment dat de knop wordt ingedrukt. */
void input_read(int *steps, bool *pressed);
