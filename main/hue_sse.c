#include "hue_sse.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum { ST_HEADER, ST_CHUNK_SIZE, ST_CHUNK_DATA, ST_CHUNK_END, ST_BODY };

void hue_sse_init(hue_sse_t *p, char *line_buffer, size_t size, hue_sse_data_cb cb, void *user)
{
    memset(p, 0, sizeof(*p));
    p->line = line_buffer;
    p->line_size = size;
    p->state = ST_HEADER;
    p->cb = cb;
    p->user = user;
}

/* Voegt één teken toe. Geeft true als er een hele regel klaarstaat. */
static bool add_to_line(hue_sse_t *p, char c)
{
    if (c == '\n') {
        if (p->line_len > 0 && p->line[p->line_len - 1] == '\r') {
            p->line_len--;
        }
        p->line[p->line_len] = '\0';
        return true;
    }
    if (p->line_len + 1 < p->line_size) {
        p->line[p->line_len++] = c;
    } else {
        p->overflow = true;   /* te lang: deze regel slaan we over */
    }
    return false;
}

static void end_line(hue_sse_t *p)
{
    p->line_len = 0;
    p->overflow = false;
}

static void header_line(hue_sse_t *p)
{
    if (p->status == 0) {
        int status;
        if (sscanf(p->line, "HTTP/%*s %d", &status) == 1) {
            p->status = status;
        }
    } else if (p->line[0] == '\0') {
        p->state = p->chunked ? ST_CHUNK_SIZE : ST_BODY;
    } else if (strncasecmp(p->line, "transfer-encoding:", 18) == 0 && strstr(p->line, "chunked")) {
        p->chunked = true;
    }
}

static void body_char(hue_sse_t *p, char c)
{
    if (!add_to_line(p, c)) {
        return;
    }
    if (!p->overflow && strncmp(p->line, "data:", 5) == 0) {
        const char *data = p->line + 5;
        while (*data == ' ') data++;
        p->cb(data, p->user);
    }
    end_line(p);
}

void hue_sse_feed(hue_sse_t *p, const char *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char c = bytes[i];
        switch (p->state) {
        case ST_HEADER:
            if (add_to_line(p, c)) {
                header_line(p);
                end_line(p);
            }
            break;
        case ST_CHUNK_SIZE:
            /* De grootte staat in hex op een eigen regel. Die houden we apart bij,
             * want een chunk kan midden in een data-regel eindigen. */
            if (c == '\n') {
                p->chunk_ext = false;
                if (p->chunk_left == 0) {
                    p->finished = true;
                } else {
                    p->state = ST_CHUNK_DATA;
                }
            } else if (isxdigit((unsigned char)c) && !p->chunk_ext) {
                int digit = isdigit((unsigned char)c) ? c - '0' : tolower((unsigned char)c) - 'a' + 10;
                p->chunk_left = p->chunk_left * 16 + digit;
            } else if (c == ';') {
                p->chunk_ext = true;
            }
            break;
        case ST_CHUNK_DATA:
            body_char(p, c);
            if (--p->chunk_left == 0) {
                p->state = ST_CHUNK_END;
            }
            break;
        case ST_CHUNK_END:
            if (c == '\n') {
                p->chunk_ext = false;
                p->state = ST_CHUNK_SIZE;
            }
            break;
        case ST_BODY:
            body_char(p, c);
            break;
        }
    }
}
