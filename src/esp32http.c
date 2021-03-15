/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
*/

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"

#include "process.h"
#include "names.h"

static const char *TAG = "httpESP32";

int responseDataLen;   /*!< data length of data */
// char *responseData;     /*!< data of the event */
object contentStr;
char httpResponseBuff[512];

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
            ESP_LOGI(TAG, "%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, data=%s", evt->data);
           strcpy(httpResponseBuff, evt->data);
           httpResponseBuff[evt->data_len] = 0;
            if (!esp_http_client_is_chunked_response(evt->client)) {
                responseDataLen = evt->data_len;
                contentStr = newStString(httpResponseBuff);               
                // contentStr = newStString(evt->data);
                // printf("%.*s", responseDataLen, charPtr(contentStr));
            } else {
                responseDataLen = evt->data_len;
                contentStr = newStString(httpResponseBuff);               
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
object httpTaskArgs[2] = { nilobj, nilobj };

void http_init(char *url)
{
    config.url = url;
    client = esp_http_client_init(&config);
}

void http_cleanup(void)
{
    esp_http_client_cleanup(client);
}

object httpRequestFrom(object request);

object httpRequest = nilobj;
object httpBlock = nilobj;

void runHttpTask(object *taskArgs)
{
    // object httpRequest = httpTaskArgs[0];
    // object httpBlock = httpTaskArgs[1];
    ESP_LOGI(TAG, "In runHttpTask()");

    object response = httpRequestFrom(httpRequest);
    // object httpBlock =  basicAt(httpRequest, 5);
    if (httpBlock != nilobj)
    {
        queueBlock(httpBlock, response);
    }
    /* delete a task when finish */
    vTaskDelete(NULL);
}

object httpPrim(int funcNumber, object *arguments)
{
    switch (funcNumber)
    {
        // execute a HttpRequest (argument 1)
        case 0:
            return httpRequestFrom(arguments[1]);
            break;

        // execute a HttpRequest in a seperate task
        case 1:
            httpRequest = arguments[1];
            httpBlock = arguments[2];
            // httpTaskArgs[0] = arguments[1]; // HttpRequest
            // httpTaskArgs[1] = arguments[2]; // done block with response arg

            xTaskCreate(runHttpTask, "runHttpTask", 8096, &httpTaskArgs, 1, NULL);
/*
            xTaskCreatePinnedToCore(
                runHttpTask,
                "runHttpTask", 
                8096, 
                &httpTaskArgs,
                1,
                NULL,
                1);
*/
            break;
        default:
            break;
    }
    
    return trueobj;
}

object httpRequestFrom(object request)
{
    ESP_LOGI(TAG, "In httpRequestFrom()");

    // First inst var of a HttpRequest object is the URL
    char *url = charPtr(basicAt(request, 1));
    ESP_LOGI(TAG, "Request URL: %s", url);
    // Second inst var of a request object is the Method (GET = 0, POST = 1, PUT = 2, PATCH = 3, DELETE = 4)
   int method = intValue(basicAt(request, 2));

    http_init(url);
    // Reusing the client and just setting the url doesn't work despite the docs
    // esp_http_client_set_url( client, urlStr );
    esp_http_client_set_method( client, method );

    if (method == HTTP_METHOD_POST) {
        object contentType = basicAt(request, 3);
        object body = basicAt(request, 4);
        ESP_LOGI(TAG, "POST body %s length %d", charPtr(body), sizeField(body) - 2);
        esp_http_client_set_header(client, "Content-Type", contentType == nilobj ? "application/json" : charPtr(contentType));
        esp_http_client_set_post_field(client, charPtr(body), strlen(charPtr(body)) );
    }
    object responseObj = nilobj;
    contentStr = nilobj;
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
        basicAtPut(responseObj, 3, contentStr);
        // basicAtPut(responseObj, 3, responseData == NULL ? nilobj : newStString(responseData));
    }

    http_cleanup();

    // Instead maybe return the esp error code vs. nil
    return responseObj;
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
