// This file allows serving the train station image as /trainstation.jfif
#include <string.h>
#include "esp_http_server.h"
#include "esp_vfs.h"

#define TRAIN_IMAGE_PATH "/spiffs/train station.jfif"

esp_err_t train_image_handler(httpd_req_t *req) {
    FILE *f = fopen(TRAIN_IMAGE_PATH, "rb");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size);
    if (!buf) {
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
