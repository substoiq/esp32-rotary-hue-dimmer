#include "dimmer.h"
#include "app_config.h"

static int clamp(int value)
{
    if (value < DIM_MIN_PERCENT) return DIM_MIN_PERCENT;
    if (value > DIM_MAX_PERCENT) return DIM_MAX_PERCENT;
    return value;
}

void dimmer_init(dimmer_state_t *s)
{
    s->on = false;
    s->brightness = DIM_ON_PERCENT;
}

void dimmer_press(dimmer_state_t *s)
{
    s->on = !s->on;
    if (s->on) {
        s->brightness = DIM_ON_PERCENT;
    }
}

bool dimmer_rotate(dimmer_state_t *s, int steps)
{
    if (!s->on || steps == 0) {
        return false;
    }
    int next = clamp(s->brightness + steps * DIM_STEP_PERCENT);
    if (next == s->brightness) {
        return false;
    }
    s->brightness = next;
    return true;
}

void dimmer_set_external(dimmer_state_t *s, bool has_on, bool on,
                         bool has_brightness, int brightness)
{
    if (has_on) s->on = on;
    if (has_brightness) s->brightness = clamp(brightness);
}
