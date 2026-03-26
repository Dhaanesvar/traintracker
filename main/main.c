#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"


#define WIFI_SSID "TrainTracker"
#define WIFI_PASS "12345678"

// Simulated train stations
const char *stations[] = {
    "Station A",
    "Station B",
    "Station C",
    "Station D",
    "Station E",
    "Station F"
};
// Simulate current location (now at Station B)
const char *current_station = "Station B";

static const char *TAG = "TRAIN";

// -------- Web Handler --------
esp_err_t root_get_handler(httpd_req_t *req)
{
    // Find current station index
    int num_stations = sizeof(stations) / sizeof(stations[0]);
    int current_idx = 0;
    for (int i = 0; i < num_stations; ++i) {
        if (strcmp(current_station, stations[i]) == 0) {
            current_idx = i;
            break;
        }
    }

    char resp[1024];
    int len = snprintf(resp, sizeof(resp),
        "<h1>Train Tracking System</h1>"
        "<p>Status: ONLINE</p>"
        "<div style='display:flex;align-items:center;gap:16px;margin:24px 0;'>");

    // Add station dots and labels
    for (int i = 0; i < num_stations; ++i) {
        const char *color = "#222"; // Not yet arrived (black)
        if (i < current_idx) color = "red"; // Passed
        else if (i == current_idx) color = "blue"; // Current
        len += snprintf(resp + len, sizeof(resp) - len,
            "<div style='text-align:center;'>"
            "<div style='width:24px;height:24px;border-radius:50%%;background:%s;margin:auto;'></div>"
            "<div style='font-size:12px;margin-top:4px;'>%s</div>"
            "</div>",
            color, stations[i]);
        if (i < num_stations - 1) {
            len += snprintf(resp + len, sizeof(resp) - len,
                "<div style='height:4px;width:32px;background:#aaa;'></div>");
        }
    }
    len += snprintf(resp + len, sizeof(resp) - len, "</div>");

    len += snprintf(resp + len, sizeof(resp) - len,
        "<p>Current Location: %s</p>", current_station);

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