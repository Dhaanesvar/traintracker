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
    "Perlis","Kedah","Penang","Perak","Selangor",
    "N9","Melaka","Johor","Kelantan","Terengganu"
};

int num_stations = sizeof(stations) / sizeof(stations[0]);
int current_idx = 0;

static const char *TAG = "TRAIN";

// 🚆 Train movement
void train_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        current_idx++;
        if (current_idx >= num_stations) current_idx = 0;
        ESP_LOGI(TAG, "Train moved to: %s", stations[current_idx]);
    }
}

// -------- STATIC FILE --------
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
    static char resp[12000];
    int len = 0;

    len += snprintf(resp + len, sizeof(resp) - len,
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='3'>"

    "<style>"
    "body{margin:0;font-family:'Segoe UI';"
    "background:url('/static/trainstation.jfif') center/cover no-repeat fixed;color:white;}"

    ".overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(10,15,30,0.75);}"

    ".main{display:flex;position:relative;z-index:1;}"

    ".left{flex:3;padding:20px;}"
    ".right{flex:1;padding:20px;}"

    ".header{display:flex;justify-content:space-between;margin-bottom:20px;}"
    ".title{font-size:30px;font-weight:bold;}"
    ".status{background:#22c55e;padding:8px 16px;border-radius:20px;}"

    ".card{background:rgba(30,41,59,0.9);padding:25px;border-radius:20px;"
    "margin-bottom:20px;box-shadow:0 10px 40px rgba(0,0,0,0.6);}"

    ".big-card{height:320px;}"

    "table{width:100%%;border-collapse:collapse;margin-top:10px;}"
    "th,td{padding:12px;text-align:center;}"
    "tr{border-bottom:1px solid #334155;}"

    ".message{text-align:center;font-size:24px;font-style:italic;"
    "margin-top:20px;color:#facc15;}"

    ".info-title{font-size:20px;font-weight:bold;margin-bottom:10px;color:#38bdf8;}"
    ".info-text{font-size:14px;line-height:1.6;color:#cbd5f5;}"
    ".highlight{color:#facc15;font-weight:bold;}"

    "</style></head>"

    "<body><div class='overlay'></div>"

    "<div class='main'>"

    "<div class='left'>"

    "<div class='header'>"
    "<div class='title'>Smart Train Tracker</div>"
    "<div class='status'>LIVE</div>"
    "</div>"

    "<div class='card big-card'>"
    "<b style='font-size:22px;'>Live Route Progress</b>"

    "<svg viewBox='0 0 900 200' width='100%%' height='220'>"

    "<line x1='60' y1='120' x2='850' y2='120' "
    "stroke='#64748b' stroke-width='10' stroke-linecap='round'/>"
    );

    int start_x = 60;
    int end_x = 850;
    int y = 120;

    for (int i = 0; i < num_stations; i++) {

        int x = start_x + i * (end_x - start_x) / (num_stations - 1);

        const char *color = "#64748b";
        if (i < current_idx) color = "#ef4444";
        else if (i == current_idx) color = "#3b82f6";

        len += snprintf(resp + len, sizeof(resp) - len,
            "<circle cx='%d' cy='%d' r='%d' fill='%s'/>"
            "<text x='%d' y='%d' font-size='14' text-anchor='middle' fill='#e2e8f0'>%s</text>",
            x, y,
            (i == current_idx) ? 16 : 12,
            color,
            x, y + 35,
            stations[i]);
    }



    len += snprintf(resp + len, sizeof(resp) - len,
    "</svg></div>"

    "<div class='card'>"
    "Train is currently at <b>%s</b> heading to <b>%s</b>"
    "</div>",
    stations[current_idx],
    stations[(current_idx + 1) % num_stations]
    );

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

    "<div class='message'>Welcome aboard. Your journey begins with innovation</div>"

    "</div>" // LEFT

    "<div class='right'>"

    "<div class='card'>"
    "<div class='info-title'>Railway History</div>"
    "<div class='info-text'>"
    "Founded in <span class='highlight'>1998</span>, the Smart Rail Network was designed "
"to revolutionize intercity transportation across Malaysia. What began as a modest "
"two-line system has evolved into a nationwide infrastructure connecting key economic "
"and cultural regions.<br><br>"

"Over the past two decades, the railway has expanded to serve more than "
"<span class='highlight'>1.5 million passengers annually</span>, providing reliable, "
"safe, and efficient mobility for both daily commuters and long-distance travelers.<br><br>"

"Today, the Smart Train System stands as a symbol of modern engineering and digital "
"transformation, integrating embedded systems, IoT communication, and intelligent "
"control mechanisms to ensure seamless operations.<br><br>"

"With a vision toward the future, ongoing developments aim to implement "
"<span class='highlight'>AI-powered predictive maintenance</span> and fully autonomous "
"train operations, positioning the railway as a leader in next-generation transport systems."

"</div></div>"

    "<div class='card'>"
    "<div class='info-title'>System Stats</div>"
    "<div class='info-text'>"
    "1. Stations: <span class='highlight'>10</span><br>"
    "2. Active Trains: <span class='highlight'>6</span><br>"
    "3. Avg Speed: <span class='highlight'>120 km/h</span><br>"
    "4. Status: <span class='highlight'>Operational</span>"
    "</div></div>"

    "</div></div></body></html>"
    );

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -------- SERVER --------
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

// -------- WIFI --------
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