/* Scherm en LVGL. LVGL is niet thread-safe: roep je het aan buiten de LVGL-taak,
 * zet de aanroep dan tussen display_lock() en display_unlock(). */
#pragma once

void display_start(void);
void display_lock(void);
void display_unlock(void);
