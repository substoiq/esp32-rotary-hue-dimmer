#include "hue_json.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

static const char *get_string(const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

/* Haalt on.on en dimming.brightness uit één grouped_light-object. */
static void read_light(const cJSON *light, hue_light_t *out)
{
    const cJSON *on = cJSON_GetObjectItemCaseSensitive(light, "on");
    const cJSON *on_value = cJSON_GetObjectItemCaseSensitive(on, "on");
    if (cJSON_IsBool(on_value)) {
        out->has_on = true;
        out->on = cJSON_IsTrue(on_value);
    }

    const cJSON *dimming = cJSON_GetObjectItemCaseSensitive(light, "dimming");
    const cJSON *bri = cJSON_GetObjectItemCaseSensitive(dimming, "brightness");
    if (cJSON_IsNumber(bri)) {
        out->has_brightness = true;
        out->brightness = (int)lround(bri->valuedouble);
    }
}

bool hue_json_find_grouped_light(const char *json, const char *name,
                                 char *id_out, size_t id_size)
{
    bool found = false;
    cJSON *root = cJSON_Parse(json);
    const cJSON *group;
    cJSON_ArrayForEach(group, cJSON_GetObjectItemCaseSensitive(root, "data")) {
        const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(group, "metadata");
        const char *group_name = get_string(metadata, "name");
        if (!group_name || strcmp(group_name, name) != 0) {
            continue;
        }
        const cJSON *service;
        cJSON_ArrayForEach(service, cJSON_GetObjectItemCaseSensitive(group, "services")) {
            const char *rtype = get_string(service, "rtype");
            const char *rid = get_string(service, "rid");
            if (rtype && rid && strcmp(rtype, "grouped_light") == 0) {
                snprintf(id_out, id_size, "%s", rid);
                found = true;
                break;
            }
        }
        break;
    }
    cJSON_Delete(root);
    return found;
}

bool hue_json_parse_state(const char *json, hue_light_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    const cJSON *first = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root, "data"), 0);
    if (first) {
        read_light(first, out);
    }
    bool ok = out->has_on;
    cJSON_Delete(root);
    return ok;
}

bool hue_json_parse_event(const char *json, const char *id, hue_light_t *out)
{
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    const cJSON *event;
    cJSON_ArrayForEach(event, root) {
        const char *event_type = get_string(event, "type");
        if (!event_type || strcmp(event_type, "update") != 0) {
            continue;
        }
        const cJSON *item;
        cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(event, "data")) {
            const char *type = get_string(item, "type");
            const char *item_id = get_string(item, "id");
            if (type && item_id && strcmp(type, "grouped_light") == 0 && strcmp(item_id, id) == 0) {
                read_light(item, out);
            }
        }
    }
    cJSON_Delete(root);
    return out->has_on || out->has_brightness;
}
