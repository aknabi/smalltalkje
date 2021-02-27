
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "httpESP32";

esp_err_t http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

esp_http_client_config_t config = {
   .url = "http://httpbin.org/get?fname=Abdul&lname=Nabi",
   .event_handler = http_event_handle,
};

// esp_http_client_config_t config = {
//    .url = "https://www.howsmyssl.com",
//    .event_handler = http_event_handle,
// };

esp_http_client_handle_t client;
esp_err_t httpError;

void http_init(void)
{
    client = esp_http_client_init(&config);
}

// Maybe do this internally to get the time (or from ST time class startup)
// Then everyone else can just use doRequest()
void http_doFirstRequest(void)
{
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
    }
}

void http_doRequest(void)
{
    esp_http_client_set_url(client, "http://httpbin.org/anything");
    esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    esp_http_client_set_header(client, "HeaderKey", "HeaderValue");
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
    }    
}

void http_cleanup(void)
{
    esp_http_client_cleanup(client);
}

void http_test(void)
{
    http_init();
    http_doFirstRequest();
    http_doRequest();
    http_cleanup();
}
