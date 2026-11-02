/* Het ronde scherm. Alleen aanroepen tussen display_lock() en display_unlock(). */
#pragma once
#include <stdbool.h>
#include "dimmer.h"

void ui_create(void);

/* connected false: geel bolletje met "verbinden…" als error NULL is, anders rood met de fouttekst. */
void ui_update(const dimmer_state_t *state, bool connected, const char *error);
