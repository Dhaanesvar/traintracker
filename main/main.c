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

// 🔥 Train movement (10 sec)
void train_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
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
    static char resp[7000];

    int len = 0;
    len += snprintf(resp + len, sizeof(resp) - len,
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta http-equiv='refresh' content='5'>"
        "<style>");
    len += snprintf(resp + len, sizeof(resp) - len,
        "body{margin:0;font-family:'Segoe UI';background:url('/static/trainstation.jfif') center/cover no-repeat fixed;color:white;}" );
    len += snprintf(resp + len, sizeof(resp) - len,
        ".overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(10,15,30,0.75);}"
        ".container{position:relative;z-index:1;padding:20px;max-width:520px;margin:auto;}"
        ".header{display:flex;justify-content:space-between;margin-bottom:20px;}"
        ".title{font-size:24px;font-weight:bold;}"
        ".status{background:#22c55e;padding:6px 12px;border-radius:20px;}"
        ".card{background:rgba(30,41,59,0.85);padding:20px;border-radius:18px;margin-bottom:20px;}"
        ".track{width:100%%%%;height:60px;}"
        ".track path{stroke:#64748b;stroke-width:4;fill:none;}"
        ".track-active{stroke:#3b82f6;stroke-width:4;fill:none;}"
        ".stations{position:relative;display:flex;justify-content:space-between;margin-top:-35px;}"
        ".station{display:flex;flex-direction:column;align-items:center;}"
        ".dot{width:18px;height:18px;border-radius:50%%%%;}"
        ".passed{background:#ef4444;}"
        ".current{background:#3b82f6;transform:scale(1.4);box-shadow:0 0 12px #3b82f6;}"
        ".upcoming{background:#64748b;}"
        ".label{font-size:10px;margin-top:4px;text-align:center;}"
        "table{width:100%%%%;border-collapse:collapse;font-size:12px;}"
        "th,td{padding:8px;text-align:center;}"
        "tr{border-bottom:1px solid #334155;}"
        ".message{text-align:center;font-size:22px;font-style:italic;margin-top:20px;color:#facc15;}"
        "</style></head>");
    len += snprintf(resp + len, sizeof(resp) - len,
        "<body><div class='overlay'></div><div class='container'>"
        "<div class='header'><div class='title'>Train Tracker</div>"
        "<div class='status'>ONLINE</div></div>"
        "<div class='card' style='padding-bottom:36px;'><b>Route Progress</b>"
        "<div style='position:relative;height:80px;'>"
        "<svg class='track' viewBox='0 0 520 60' style='position:absolute;left:0;top:0;width:100%%%%;height:60px;'>"
        "<path d='M30 40 Q60 10 100 30 Q140 50 180 30 Q220 10 260 30 Q300 50 340 30 Q380 10 420 30 Q460 50 490 40'/>"
        "</svg>"
        "<div style='position:absolute;left:0;top:0;width:100%%%%;height:60px;'>");
    // New dot positions, sampled along the curve for 10 stations
    const int dot_x[10] = {30, 80, 130, 180, 230, 280, 330, 380, 430, 490};
    const int dot_y[10] = {40, 22, 38, 22, 38, 22, 38, 22, 38, 40};
    for (int i = 0; i < num_stations; i++) {
        const char *cls = "upcoming";
        char extra_style[64] = "";
        if (i < current_idx) cls = "passed";
        else if (i == current_idx) cls = "current";
        // Make first station bigger
        if (i == 0) {
            strcpy(extra_style, "width:60px;");
        } else if (i == num_stations-1) {
            strcpy(extra_style, "width:60px;text-align:right;");
        } else {
            strcpy(extra_style, "width:40px;");
        }
        // Make first dot and label larger
        if (i == 0) {
            len += snprintf(resp + len, sizeof(resp) - len,
                "<div class='station' style='position:absolute;left:%dpx;top:%dpx;%stransform:translate(-30%%,-50%%);'>"
                "<div class='dot %s' style='width:28px;height:28px;'></div>"
                "<div class='label' style='font-size:15px;font-weight:bold;'>%s</div></div>",
                dot_x[i], dot_y[i], extra_style, cls, stations[i]);
        } else if (i == num_stations-1) {
            // Last station: right-align label
            len += snprintf(resp + len, sizeof(resp) - len,
                "<div class='station' style='position:absolute;left:%dpx;top:%dpx;%stransform:translate(30%%,-50%%);'>"
                "<div class='dot %s'></div>"
                "<div class='label' style='text-align:right;'>%s</div></div>",
                dot_x[i], dot_y[i], extra_style, cls, stations[i]);
        } else {
            len += snprintf(resp + len, sizeof(resp) - len,
                "<div class='station' style='position:absolute;left:%dpx;top:%dpx;%stransform:translate(-50%%,-50%%);'>"
                "<div class='dot %s'></div>"
                "<div class='label'>%s</div></div>",
                dot_x[i], dot_y[i], extra_style, cls, stations[i]);
        }
    }
    len += snprintf(resp + len, sizeof(resp) - len, "</div></div></div>");

    /* BOX 1.5: Train location info */
    int next_idx = (current_idx + 1) % num_stations;
    len += snprintf(resp + len, sizeof(resp) - len,
        "<div class='card' style='background:#334155;margin-top:-10px;margin-bottom:18px;padding:12px 18px;font-size:15px;text-align:center;'>"
        "Train #100 is at <b>%s</b>, heading to <b>%s</b>."
        "</div>",
        stations[current_idx], stations[next_idx]);

    /* BOX 2 */
    len += snprintf(resp + len, sizeof(resp) - len,
        "<div class='card'><b>Upcoming Train Schedule</b>"
        "<table>"
        "<tr><th>Train</th><th>ETA</th><th>Delay</th><th>Trip</th></tr>"
        "<tr><td>T1</td><td>2 min</td><td>On Time</td><td>#101</td></tr>"
        "<tr><td>T2</td><td>5 min</td><td>+1 min</td><td>#102</td></tr>"
        "<tr><td>T3</td><td>8 min</td><td>On Time</td><td>#103</td></tr>"
        "<tr><td>T4</td><td>12 min</td><td>+2 min</td><td>#104</td></tr>"
        "<tr><td>T5</td><td>15 min</td><td>On Time</td><td>#105</td></tr>"
        "<tr><td>T6</td><td>20 min</td><td>+3 min</td><td>#106</td></tr>"
        "</table></div>"
    );

    /* MESSAGE */
    len += snprintf(resp + len, sizeof(resp) - len,
        "<div class='message'>Welcome aboard and Enjoy your journey with comfort and safety.</div>"
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