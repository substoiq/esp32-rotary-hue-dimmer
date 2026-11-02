/* JSON van de Hue Bridge uitpakken. In Python zou dit json.loads() plus een paar
 * dict-lookups zijn; in C doen we dat met de bibliotheek cJSON. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool  has_on;
    bool  on;
    bool  has_brightness;
    int   brightness;
} hue_light_t;

/* Zoekt in het antwoord van /resource/zone of /resource/room de groep met deze
 * naam en geeft de id van de bijbehorende grouped_light terug. */
bool hue_json_find_grouped_light(const char *json, const char *name,
                                 char *id_out, size_t id_size);

/* Leest de stand uit het antwoord van /resource/grouped_light/<id>. */
bool hue_json_parse_state(const char *json, hue_light_t *out);

/* Leest een eventstream-bericht. Geeft true als er een wijziging voor deze
 * grouped_light in stond. */
bool hue_json_parse_event(const char *json, const char *id, hue_light_t *out);
