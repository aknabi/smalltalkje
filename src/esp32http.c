#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"

#include "memory.h"
#include "names.h"

static const char *TAG = "httpESP32";

int responseDataLen;   /*!< data length of data */
char *responseData;     /*!< data of the event */


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
                responseDataLen = evt->data_len;
                responseData = (char*) evt->data;
                printf("%.*s", responseDataLen, responseData);
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
   .url = "http://httpbin.org/get",
   .event_handler = http_event_handle,
};

esp_http_client_handle_t client;
esp_err_t httpError;

object httpRequestFrom(object request);

object httpPrim(int funcNumber, object *arguments)
{
    switch (funcNumber)
    {
        case 0:
            return httpRequestFrom(arguments[1]);
            break;

        default:
            break;
    }
    
    return nilobj;
}

object httpRequestFrom(object request)
{
    // First inst var of a request object is the URL
    char *urlStr = charPtr(basicAt(request, 1));
    int method = intValue(basicAt(request, 2));
    ESP_LOGI(TAG, "Prim 183 URL = %s method: %d", urlStr, method);

    esp_http_client_set_url( client, urlStr );
    // Second inst var of a request object is the Method (GET = 0, POST = 1, PUT = 2, PATCH = 3, DELETE = 4)
    
    esp_http_client_set_method( client, method );
    responseData = NULL;
    object responseObj = nilobj;
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        int statusCode = esp_http_client_get_status_code(client);
        int contentLength = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Status = %d, content_length = %d", statusCode, contentLength);
        // create a response object
        responseObj = allocObject(3);
        setClass(responseObj, globalSymbol("HttpResponse"));
        // instVar 1 is status code
        basicAtPut(responseObj, 1, newInteger(statusCode));
        // instVar 2 is content length
        basicAtPut(responseObj, 2, newInteger(contentLength));
        // instVar 3 is content string
        basicAtPut(responseObj, 3, responseData == NULL ? nilobj : newStString(responseData));
    }
    // Instead maybe return the esp error code vs. nil
    return responseObj;
}

void http_init(void)
{
    client = esp_http_client_init(&config);
}

void http_doGetRequest(void)
{
    esp_http_client_set_url(client, "http://httpbin.org/get?fname=Abdul&lname=Nabi");
    esp_http_client_set_method(client, HTTP_METHOD_GET);
 
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
    }
}

/*
 * HTTP request object is: url, method, header
 * HTTP response object is: status code, content len, content
 */
void http_doRequest(char *url, esp_http_client_method_t method)
{
    esp_http_client_set_url(client, "http://httpbin.org/anything");
    esp_http_client_set_method(client, method);
    esp_http_client_set_header(client, "HeaderKey", "HeaderValue");
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
    }    
}

object requestFrom(object request)
{
    // First inst var of a request object is the URL
    esp_http_client_set_url( client, charPtr(basicAt(request, 1)) );
    // Second inst var of a request object is the Method (GET = 0, POST = 1, PUT = 2, PATCH = 3, DELETE = 4)
    esp_http_client_set_method( client, intValue(basicAt(request, 1)) );
    responseData = NULL;
    object responseObj = nilobj;
    httpError = esp_http_client_perform(client);
    if (httpError == ESP_OK) {
        int statusCode = esp_http_client_get_status_code(client);
        int contentLength = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Status = %d, content_length = %d", statusCode, contentLength);
        // create a response object
        responseObj = allocObject(3);
        setClass(responseObj, globalSymbol("HttpResponse"));
        // instVar 1 is status code
        basicAtPut(responseObj, 1, newInteger(statusCode));
        // instVar 2 is content length
        basicAtPut(responseObj, 2, newInteger(contentLength));
        object contentStr = newStString(responseData);
        basicAtPut(responseObj, 3, contentStr);
        // basicAtPut(responseObj, 3, responseData == NULL ? nilobj : newStString(responseData));
    }
    // Instead maybe return the esp error code vs. nil
    return responseObj;
}

void http_cleanup(void)
{
    esp_http_client_cleanup(client);
}

void http_test(void)
{
    http_init();
    http_doGetRequest();
    http_doRequest("http://httpbin.org/anything", HTTP_METHOD_DELETE);
    // http_cleanup();
}
