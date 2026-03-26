#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"

#define WIFI_SSID "TrainTracker"
#define WIFI_PASS "12345678"

// Stations
const char *stations[] = {
    "Subang Jaya",
    "Bangi",
    "Kajang",
    "Serdang",
    "Petaling Jaya",
    "KL Sentral"
};

// Current station
const char *current_station = "Bangi";

static const char *TAG = "TRAIN";

// -------- Web Handler --------
esp_err_t root_get_handler(httpd_req_t *req)
{
    int num_stations = sizeof(stations) / sizeof(stations[0]);
    int current_idx = 0;

    for (int i = 0; i < num_stations; ++i) {
        if (strcmp(current_station, stations[i]) == 0) {
            current_idx = i;
            break;
        }
    }

    // FIX: use static buffer (avoid crash)
    static char resp[4096];

    int len = snprintf(resp, sizeof(resp),
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>"
    "body { margin:0; font-family: 'Segoe UI', sans-serif; background:#0f172a; color:#fff; }"
    ".container { padding:20px; max-width:500px; margin:auto; }"

    ".header { display:flex; justify-content:space-between; align-items:center; margin-bottom:20px; }"
    ".title { font-size:22px; font-weight:bold; }"
    ".status { background:#16a34a; padding:5px 10px; border-radius:20px; font-size:12px; }"

    ".card { background:#1e293b; padding:20px; border-radius:16px; box-shadow:0 4px 20px rgba(0,0,0,0.3); margin-bottom:20px; }"
    ".card-title { font-size:14px; color:#94a3b8; margin-bottom:5px; }"
    ".card-value { font-size:20px; font-weight:bold; }"

    ".timeline { display:flex; justify-content:space-between; align-items:center; position:relative; margin-top:30px; }"
    ".line { position:absolute; top:12px; left:0; right:0; height:4px; background:#334155; z-index:0; }"

    ".station { display:flex; flex-direction:column; align-items:center; z-index:1; width:60px; }"
    ".dot { width:24px; height:24px; border-radius:50%%; margin-bottom:6px; transition:0.3s; }"

    ".passed { background:#ef4444; }"
    ".current { background:#3b82f6; width:30px; height:30px; box-shadow:0 0 12px #3b82f6; }"
    ".upcoming { background:#64748b; }"

    ".label { font-size:11px; text-align:center; color:#cbd5f5; }"

    "</style></head><body>"

    "<div class='container'>"

    "<div class='header'>"
    "<div class='title'>Train Tracker</div>"
    "<div class='status'>ONLINE</div>"
    "</div>"

    "<div class='card'>"
    "<div class='card-title'>Current Location</div>"
    "<div class='card-value'> %s</div>"
    "</div>"

    "<div class='card'>"
    "<div class='card-title'>Route Progress</div>"

    "<div class='timeline'>"
    "<div class='line'></div>"
    , current_station);

    for (int i = 0; i < num_stations; ++i) {
        const char *cls = "upcoming";
        if (i < current_idx) cls = "passed";
        else if (i == current_idx) cls = "current";

        len += snprintf(resp + len, sizeof(resp) - len,
            "<div class='station'>"
            "<div class='dot %s'></div>"
            "<div class='label'>%s</div>"
            "</div>",
            cls, stations[i]);
    }

    len += snprintf(resp + len, sizeof(resp) - len,
    "</div></div></div></body></html>");

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -------- Start Web Server --------
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler
        };
        httpd_register_uri_handler(server, &uri);
    }
    return server;
}

// -------- WiFi AP Setup --------
void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "AP Started. SSID:%s", WIFI_SSID);
}

// -------- Main --------
void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_softap();
    start_webserver();
}