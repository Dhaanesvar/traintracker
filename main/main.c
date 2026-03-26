#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_SSID "TrainTracker"
#define WIFI_PASS "12345678"

const char *stations[] = {
    "Subang Jaya","Bangi","Kajang","Serdang","Petaling Jaya",
    "KL Sentral","Putrajaya","Ipoh","Butterworth","Penang"
};

int num_stations = sizeof(stations) / sizeof(stations[0]);
int current_idx = 0;

static const char *TAG = "TRAIN";

// 🚆 Move train every 8 sec (smooth demo)
void train_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(8000));
        current_idx++;
        if (current_idx >= num_stations) current_idx = 0;
        ESP_LOGI(TAG, "Train moved to: %s", stations[current_idx]);
    }
}

// -------- Static File Handler --------
esp_err_t static_file_handler(httpd_req_t *req) {
    char filepath[128] = "/spiffs";
    strncat(filepath, req->uri + 7, sizeof(filepath) - strlen(filepath) - 1);

    struct stat st;
    if (stat(filepath, &st) != 0) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buf = malloc(st.st_size);
    fread(buf, 1, st.st_size, f);
    fclose(f);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, buf, st.st_size);
    free(buf);
    return ESP_OK;
}

// -------- WEB UI --------
esp_err_t root_get_handler(httpd_req_t *req)
{
    static char resp[9000];
    int len = 0;

    len += snprintf(resp + len, sizeof(resp) - len,
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='3'>"  // faster refresh

    "<style>"
    "body{margin:0;font-family:'Segoe UI';"
    "background:url('/static/trainstation.jfif') center/cover no-repeat fixed;color:white;}"

    ".overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(10,15,30,0.75);}"

    ".container{position:relative;z-index:1;padding:20px;max-width:750px;margin:auto;}"

    ".header{display:flex;justify-content:space-between;margin-bottom:20px;}"
    ".title{font-size:28px;font-weight:bold;}"
    ".status{background:#22c55e;padding:6px 14px;border-radius:20px;}"

    ".card{background:rgba(30,41,59,0.9);padding:25px;border-radius:20px;margin-bottom:20px;"
    "box-shadow:0 10px 40px rgba(0,0,0,0.5);}"

    ".big-card{height:260px;}"

    ".message{text-align:center;font-size:26px;font-style:italic;margin-top:25px;color:#facc15;}"

    "table{width:100%%;border-collapse:collapse;margin-top:10px;}"
    "th,td{padding:10px;text-align:center;}"
    "tr{border-bottom:1px solid #334155;}"
    "</style></head>"

    "<body><div class='overlay'></div><div class='container'>"

    "<div class='header'><div class='title'>Smart Train Tracker</div>"
    "<div class='status'>LIVE</div></div>"

    "<div class='card big-card'><b>Live Route Progress</b>"
    "<svg viewBox='0 0 600 150' width='100%%'>"

    // Track
    "<path d='M50 100 Q200 20 350 100 Q500 180 550 100'"
    " stroke='#64748b' stroke-width='6' fill='none'/>"
    );

    // Train position calculation
    int train_x = 50 + (current_idx * 50);
    int train_y = (current_idx % 2 == 0) ? 100 : 60;

    // Stations
    for (int i = 0; i < num_stations; i++) {
        int x = 50 + (i * 50);
        int y = (i % 2 == 0) ? 100 : 60;

        const char *color = "#64748b";
        if (i < current_idx) color = "#ef4444";
        else if (i == current_idx) color = "#3b82f6";

        len += snprintf(resp + len, sizeof(resp) - len,
        "<circle cx='%d' cy='%d' r='%d' fill='%s'/>"
        "<text x='%d' y='%d' font-size='11' text-anchor='middle' fill='#e2e8f0'>%s</text>",
        x, y,
        (i == current_idx) ? 9 : 6,
        color,
        x, y + 20,
        stations[i]);
    }

    // 🚆 TRAIN ICON (moving)
    len += snprintf(resp + len, sizeof(resp) - len,
    "<text x='%d' y='%d' font-size='20'></text>",
    train_x - 10, train_y - 10
    );

    len += snprintf(resp + len, sizeof(resp) - len,
    "</svg></div>"

    "<div class='card'>Train is currently at <b>%s</b> heading to <b>%s</b></div>",
    stations[current_idx],
    stations[(current_idx + 1) % num_stations]
    );

    // Table
    len += snprintf(resp + len, sizeof(resp) - len,
    "<div class='card'><b>Upcoming Train Schedule</b>"
    "<table>"
    "<tr><th>Train</th><th>ETA</th><th>Status</th><th>Trip</th></tr>"
    "<tr><td>T1</td><td>2 min</td><td style='color:#22c55e'>On Time</td><td>#101</td></tr>"
    "<tr><td>T2</td><td>5 min</td><td style='color:#ef4444'>Delayed</td><td>#102</td></tr>"
    "<tr><td>T3</td><td>8 min</td><td style='color:#22c55e'>On Time</td><td>#103</td></tr>"
    "<tr><td>T4</td><td>12 min</td><td style='color:#ef4444'>Delayed</td><td>#104</td></tr>"
    "<tr><td>T5</td><td>15 min</td><td style='color:#22c55e'>On Time</td><td>#105</td></tr>"
    "<tr><td>T6</td><td>20 min</td><td style='color:#ef4444'>Delayed</td><td>#106</td></tr>"
    "</table></div>"

    "<div class='message'>Welcome aboard and Your journey begins with innovation.</div>"

    "</div></body></html>"
    );

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -------- Server --------
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri = { .uri="/", .method=HTTP_GET, .handler=root_get_handler };
        httpd_register_uri_handler(server, &uri);

        httpd_uri_t static_uri = { .uri="/static/*", .method=HTTP_GET, .handler=static_file_handler };
        httpd_register_uri_handler(server, &static_uri);
    }
    return server;
}

// -------- WiFi --------
void wifi_init_softap(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

// -------- MAIN --------
void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    wifi_init_softap();
    start_webserver();

    xTaskCreate(train_task, "train_task", 2048, NULL, 5, NULL);
}