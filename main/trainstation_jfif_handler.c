#include <stdio.h>
#include "esp_http_server.h"
#include "esp_log.h"

esp_err_t trainstation_jfif_handler(httpd_req_t *req) {
    ESP_LOGI("TRAIN_IMG", "Handling request for /trainstation.jfif");
    FILE *f = fopen("/spiffs/trainstation.jfif", "rb");
    if (!f) {
        ESP_LOGE("TRAIN_IMG", "Failed to open /spiffs/trainstation.jfif");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size);
    if (!buf) {
        ESP_LOGE("TRAIN_IMG", "Failed to allocate buffer of size %zu", size);
        fclose(f);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    fread(buf, 1, size, f);
    fclose(f);
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, buf, size);
    free(buf);
    return ESP_OK;
}
