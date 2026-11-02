/* Leest de HTTP-stroom van de eventstream en geeft elke "data:"-regel door.
 * De bytes komen in willekeurige stukjes binnen, dus de parser onthoudt waar hij was. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef void (*hue_sse_data_cb)(const char *data, void *user);

typedef struct {
    char  *line;          /* regelbuffer, door de aanroeper aangeleverd */
    size_t line_size;
    size_t line_len;
    bool   overflow;
    int    state;
    bool   chunked;
    size_t chunk_left;
    bool   chunk_ext;
    int    status;        /* HTTP-status, 0 zolang die nog niet bekend is */
    bool   finished;      /* de server sloot de stroom netjes af */
    hue_sse_data_cb cb;
    void  *user;
} hue_sse_t;

void hue_sse_init(hue_sse_t *p, char *line_buffer, size_t size, hue_sse_data_cb cb, void *user);
void hue_sse_feed(hue_sse_t *p, const char *bytes, size_t len);
