/*
 * hue.c: de koppeling met de bridge, in twee taken.
 *
 *  command_task  zoekt de zone op, leest de stand en verstuurt opdrachten.
 *  events_task   houdt de eventstream open, zodat wijzigingen uit de Hue-app binnenkomen.
 *
 * Vergelijkbaar met twee threads in Python die via een queue met het hoofdprogramma praten.
 */
#include "hue.h"
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "credentials.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hue_sse.h"
#include "wifi.h"

#define BODY_SIZE      (48 * 1024)
#define SSE_LINE_SIZE  (16 * 1024)
#define RETRY_MS       3000
#define BIT_HAVE_ID    BIT0

static const char *TAG = "hue";

static QueueHandle_t      s_events;
static TaskHandle_t       s_command_task;
static EventGroupHandle_t s_flags;
static SemaphoreHandle_t  s_pending_lock;

/* De nieuwste aangevraagde stand die nog niet verstuurd is. */
static bool s_pending, s_pending_on;
static int  s_pending_brightness;

static volatile bool s_resync;
static char s_light_id[64];

static esp_http_client_handle_t s_client;
static char *s_body;
static int   s_body_len;

/* ---------- Meldingen naar het hoofdprogramma ---------- */

static void post_status(bool connected, const char *error)
{
    hue_event_t event = { .type = HUE_EVT_STATUS, .connected = connected, .error = error };
    xQueueSend(s_events, &event, pdMS_TO_TICKS(100));
}

static void post_light(const hue_light_t *light)
{
    hue_event_t event = { .type = HUE_EVT_LIGHT, .light = *light };
    xQueueSend(s_events, &event, pdMS_TO_TICKS(100));
}

/* ---------- HTTPS-verzoeken ---------- */

static esp_err_t on_http_event(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && s_body_len + event->data_len < BODY_SIZE) {
        memcpy(s_body + s_body_len, event->data, event->data_len);
        s_body_len += event->data_len;
        s_body[s_body_len] = '\0';
    }
    return ESP_OK;
}

static void client_open(void)
{
    esp_http_client_config_t config = {
        .url = "https://" HUE_BRIDGE_IP "/clip/v2/resource",
        .event_handler = on_http_event,
        .timeout_ms = 5000,
        .keep_alive_enable = true,   /* houd de verbinding open: scheelt een TLS-handshake */
        .skip_cert_common_name_check = true,
    };
    s_client = esp_http_client_init(&config);
    esp_http_client_set_header(s_client, "hue-application-key", HUE_APP_KEY);
    esp_http_client_set_header(s_client, "Content-Type", "application/json");
}

/* Doet één verzoek en geeft de HTTP-status terug, of -1 bij een netwerkfout. */
static int request(esp_http_client_method_t method, const char *path, const char *body)
{
    char url[160];
    snprintf(url, sizeof(url), "https://" HUE_BRIDGE_IP "%s", path);

    /* De bridge kan een stilliggende verbinding hebben gesloten. Daarom een tweede
     * poging op een nieuwe verbinding. */
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!s_client) {
            client_open();
        }
        s_body_len = 0;
        s_body[0] = '\0';
        esp_http_client_set_url(s_client, url);
        esp_http_client_set_method(s_client, method);
        esp_http_client_set_post_field(s_client, body, body ? strlen(body) : 0);

        if (esp_http_client_perform(s_client) == ESP_OK) {
            return esp_http_client_get_status_code(s_client);
        }
        esp_http_client_cleanup(s_client);
        s_client = NULL;
    }
    return -1;
}

/* ---------- Zone opzoeken en stand lezen ---------- */

static bool sync_with_bridge(const char **error)
{
    *error = "bridge onbereikbaar";

    /* De id blijft geldig, dus die hoeven we maar één keer op te zoeken. */
    if (s_light_id[0] == '\0') {
        static const char *const paths[] = { "/clip/v2/resource/zone", "/clip/v2/resource/room" };
        for (int i = 0; i < 2 && s_light_id[0] == '\0'; i++) {
            int status = request(HTTP_METHOD_GET, paths[i], NULL);
            if (status == 401 || status == 403) {
                *error = "sleutel geweigerd";
                return false;
            }
            if (status != 200) {
                ESP_LOGW(TAG, "%s gaf status %d", paths[i], status);
                return false;
            }
            hue_json_find_grouped_light(s_body, HUE_ZONE_NAME, s_light_id, sizeof(s_light_id));
        }
        if (s_light_id[0] == '\0') {
            *error = "zone niet gevonden";
            return false;
        }
        ESP_LOGI(TAG, "%s heeft grouped_light %s", HUE_ZONE_NAME, s_light_id);
        xEventGroupSetBits(s_flags, BIT_HAVE_ID);
    }

    char path[128];
    snprintf(path, sizeof(path), "/clip/v2/resource/grouped_light/%s", s_light_id);
    int status = request(HTTP_METHOD_GET, path, NULL);
    hue_light_t light;
    if (status != 200 || !hue_json_parse_state(s_body, &light)) {
        ESP_LOGW(TAG, "stand lezen gaf status %d", status);
        return false;
    }
    post_light(&light);
    return true;
}

/* ---------- Opdrachten versturen ---------- */

static bool take_pending(bool *on, int *brightness)
{
    xSemaphoreTake(s_pending_lock, portMAX_DELAY);
    bool had_pending = s_pending;
    *on = s_pending_on;
    *brightness = s_pending_brightness;
    s_pending = false;
    xSemaphoreGive(s_pending_lock);
    return had_pending;
}

void hue_set(bool on, int brightness)
{
    xSemaphoreTake(s_pending_lock, portMAX_DELAY);
    s_pending = true;
    s_pending_on = on;
    s_pending_brightness = brightness;
    xSemaphoreGive(s_pending_lock);
    xTaskNotifyGive(s_command_task);
}

static int send_state(bool on, int brightness)
{
    char path[128], body[128];
    snprintf(path, sizeof(path), "/clip/v2/resource/grouped_light/%s", s_light_id);
    if (on) {
        /* Aan en helderheid in één opdracht, zodat de lampen niet eerst op de oude stand flitsen. */
        snprintf(body, sizeof(body),
                 "{\"on\":{\"on\":true},\"dimming\":{\"brightness\":%d},\"dynamics\":{\"duration\":%d}}",
                 brightness, HUE_TRANSITION_MS);
    } else {
        snprintf(body, sizeof(body), "{\"on\":{\"on\":false}}");
    }
    return request(HTTP_METHOD_PUT, path, body);
}

static void command_task(void *arg)
{
    int64_t last_send_ms = 0;

    for (;;) {
        wifi_wait_connected();

        const char *error;
        if (!sync_with_bridge(&error)) {
            post_status(false, error);
            vTaskDelay(pdMS_TO_TICKS(RETRY_MS));
            continue;
        }
        post_status(true, NULL);
        s_resync = false;

        while (!s_resync) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (s_resync) {
                break;
            }

            /* Wacht eerst de minimale tijd af. Wat er intussen binnenkomt, overschrijft
             * de vorige waarde, dus de bridge krijgt alleen de nieuwste stand. */
            int64_t wait_ms = last_send_ms + HUE_SEND_INTERVAL_MS - esp_timer_get_time() / 1000;
            if (wait_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(wait_ms));
            }

            bool on;
            int brightness;
            if (!take_pending(&on, &brightness)) {
                continue;
            }
            int status = send_state(on, brightness);
            last_send_ms = esp_timer_get_time() / 1000;

            if (status == 429) {
                /* De bridge vraagt om rustiger aan te doen: straks opnieuw, tenzij er iets nieuws is. */
                vTaskDelay(pdMS_TO_TICKS(1000));
                xSemaphoreTake(s_pending_lock, portMAX_DELAY);
                if (!s_pending) {
                    s_pending = true;
                    s_pending_on = on;
                    s_pending_brightness = brightness;
                }
                xSemaphoreGive(s_pending_lock);
                xTaskNotifyGive(s_command_task);
            } else if (status != 200) {
                ESP_LOGW(TAG, "opdracht gaf status %d", status);
                post_status(false, status == 401 || status == 403 ? "sleutel geweigerd"
                                                                  : "bridge onbereikbaar");
                break;   /* verbinding en stand opnieuw ophalen */
            }
        }
    }
}

/* ---------- Eventstream ---------- */

static void on_sse_data(const char *data, void *user)
{
    hue_light_t light;
    if (hue_json_parse_event(data, s_light_id, &light)) {
        post_light(&light);
    }
}

static void read_event_stream(char *line_buffer)
{
    esp_tls_cfg_t config = {
        .timeout_ms = 10000,
        .skip_common_name = true,
        .keep_alive_cfg = &(tls_keep_alive_cfg_t){
            .keep_alive_enable = true,
            .keep_alive_idle = 30,
            .keep_alive_interval = 5,
            .keep_alive_count = 3,
        },
    };
    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        return;
    }
    if (esp_tls_conn_new_sync(HUE_BRIDGE_IP, strlen(HUE_BRIDGE_IP), 443, &config, tls) != 1) {
        esp_tls_conn_destroy(tls);
        return;
    }

    static const char http_request[] =
        "GET /eventstream/clip/v2 HTTP/1.1\r\n"
        "Host: " HUE_BRIDGE_IP "\r\n"
        "hue-application-key: " HUE_APP_KEY "\r\n"
        "Accept: text/event-stream\r\n"
        "\r\n";
    if (esp_tls_conn_write(tls, http_request, strlen(http_request)) < 0) {
        esp_tls_conn_destroy(tls);
        return;
    }

    hue_sse_t parser;
    hue_sse_init(&parser, line_buffer, SSE_LINE_SIZE, on_sse_data, NULL);
    ESP_LOGI(TAG, "eventstream geopend");

    char chunk[512];
    while (!parser.finished) {
        int count = esp_tls_conn_read(tls, chunk, sizeof(chunk));
        if (count == ESP_TLS_ERR_SSL_WANT_READ || count == ESP_TLS_ERR_SSL_WANT_WRITE ||
            count == ESP_TLS_ERR_SSL_TIMEOUT) {
            continue;   /* er kwam even niets binnen, dat is normaal */
        }
        if (count <= 0) {
            break;
        }
        hue_sse_feed(&parser, chunk, count);
        if (parser.status != 0 && parser.status != 200) {
            ESP_LOGW(TAG, "eventstream gaf status %d", parser.status);
            break;
        }
    }
    esp_tls_conn_destroy(tls);
    ESP_LOGW(TAG, "eventstream gesloten");
}

static void events_task(void *arg)
{
    char *line_buffer = heap_caps_malloc(SSE_LINE_SIZE, MALLOC_CAP_SPIRAM);
    xEventGroupWaitBits(s_flags, BIT_HAVE_ID, pdFALSE, pdTRUE, portMAX_DELAY);

    for (;;) {
        wifi_wait_connected();
        read_event_stream(line_buffer);
        vTaskDelay(pdMS_TO_TICKS(RETRY_MS));

        /* We kunnen iets gemist hebben: haal de stand opnieuw op. */
        s_resync = true;
        xTaskNotifyGive(s_command_task);
    }
}

void hue_start(QueueHandle_t events)
{
    s_events = events;
    s_flags = xEventGroupCreate();
    s_pending_lock = xSemaphoreCreateMutex();
    s_body = heap_caps_malloc(BODY_SIZE, MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCore(command_task, "hue_cmd", 8192, NULL, 4, &s_command_task, 0);
    xTaskCreatePinnedToCore(events_task, "hue_events", 8192, NULL, 4, NULL, 0);
}
