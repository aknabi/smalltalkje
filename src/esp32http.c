/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

    ESP32 HTTP Client Implementation
    
    This module provides HTTP client functionality for the ESP32 platform in the Smalltalkje
    system. It implements a wrapper around the ESP-IDF HTTP client library to allow Smalltalk
    code to make HTTP requests (GET, POST, etc.) and process responses.
    
    The module supports:
    - Creating and configuring HTTP requests
    - Setting headers and request bodies for various HTTP methods
    - Processing HTTP responses and events
    - Executing requests synchronously or asynchronously using FreeRTOS tasks
    
    HTTP requests and responses are represented as Smalltalk objects with appropriate
    instance variables to store URLs, methods, headers, status codes, and content.
*/

#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"

#include "process.h"
#include "names.h"

/** Tag for ESP logging */
static const char *TAG = "httpESP32";

/** Response data length from HTTP request */
int responseDataLen;

/** Smalltalk string object containing response content */
object contentStr;

/** Buffer for storing HTTP response data */
char httpResponseBuff[512];

/**
 * Handle HTTP client events
 * 
 * This callback function processes events from the ESP HTTP client during
 * request execution. It handles various event types including connection events,
 * header events, data reception, and disconnection.
 * 
 * For data events, it copies the received data into a buffer and creates a
 * Smalltalk string object to store the response content.
 * 
 * @param evt Pointer to the HTTP client event structure
 * @return ESP_OK to continue processing events
 */
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
            
            responseDataLen = evt->data_len;
            contentStr = newStString(httpResponseBuff);               
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

/** Default HTTP client configuration */
esp_http_client_config_t config = {
   .url = "http://httpbin.org/get",
   .event_handler = http_event_handle,
};

/** HTTP client handle */
esp_http_client_handle_t client;

/** Error status from HTTP operations */
esp_err_t httpError;

/** Arguments for HTTP task execution */
object httpTaskArgs[2] = { nilobj, nilobj };

/**
 * Initialize the HTTP client with a specific URL
 * 
 * @param url The URL to initialize the HTTP client with
 */
void http_init(char *url)
{
    config.url = url;
    client = esp_http_client_init(&config);
}

/**
 * Clean up the HTTP client and release resources
 */
void http_cleanup(void)
{
    esp_http_client_cleanup(client);
}

/** Forward declaration for httpRequestFrom function */
object httpRequestFrom(object request);

/** The HTTP request object for async execution */
object httpRequest = nilobj;

/** The callback block to execute after HTTP request completion */
object httpBlock = nilobj;

/**
 * FreeRTOS task function for executing HTTP requests asynchronously
 * 
 * This function executes an HTTP request in a separate FreeRTOS task,
 * which allows the Smalltalk VM to continue execution while the HTTP
 * request is in progress. After completion, it queues the callback block
 * with the response as an argument.
 * 
 * @param taskArgs Pointer to task arguments (unused)
 */
void runHttpTask(object *taskArgs)
{
    ESP_LOGI(TAG, "In runHttpTask()");

    object response = httpRequestFrom(httpRequest);
    
    if (httpBlock != nilobj)
    {
        queueBlock(httpBlock, response);
    }
    
    /* delete a task when finish */
    vTaskDelete(NULL);
}

/**
 * Primitives for HTTP operations
 * 
 * This function implements the Smalltalk primitives for HTTP operations:
 * - Function 0: Execute an HTTP request synchronously
 * - Function 1: Execute an HTTP request asynchronously in a separate task
 * 
 * @param funcNumber The primitive function number to execute
 * @param arguments Array of Smalltalk objects as arguments
 * @return Result object from the primitive operation
 */
object httpPrim(int funcNumber, object *arguments)
{
    switch (funcNumber)
    {
        // Execute a HttpRequest (argument 1) synchronously
        case 0:
            return httpRequestFrom(arguments[1]);
            break;

        // Execute a HttpRequest in a separate task
        case 1:
            httpRequest = arguments[1];
            httpBlock = arguments[2];

            xTaskCreate(runHttpTask, "runHttpTask", 8096, &httpTaskArgs, 1, NULL);
            break;
            
        default:
            break;
    }
    
    return trueobj;
}

/**
 * Execute an HTTP request and return the response
 * 
 * This function takes a Smalltalk HttpRequest object, extracts the necessary
 * information (URL, method, headers, body), performs the HTTP request, and
 * creates a Smalltalk HttpResponse object with the results.
 * 
 * Structure of HttpRequest object:
 * - Instance variable 1: URL (String)
 * - Instance variable 2: Method (Integer, 0=GET, 1=POST, 2=PUT, 3=PATCH, 4=DELETE)
 * - Instance variable 3: Content-Type (String, for POST requests)
 * - Instance variable 4: Body (String, for POST requests)
 * 
 * Structure of HttpResponse object:
 * - Instance variable 1: Status code (Integer)
 * - Instance variable 2: Content length (Integer)
 * - Instance variable 3: Content (String)
 * 
 * @param request The Smalltalk HttpRequest object
 * @return A Smalltalk HttpResponse object or nil on failure
 */
object httpRequestFrom(object request)
{
    ESP_LOGI(TAG, "In httpRequestFrom()");

    // Extract request information from Smalltalk object
    char *url = charPtr(basicAt(request, 1));
    ESP_LOGI(TAG, "Request URL: %s", url);
    int method = intValue(basicAt(request, 2));

    // Initialize HTTP client
    http_init(url);
    esp_http_client_set_method(client, method);

    // Handle POST request with content type and body
    if (method == HTTP_METHOD_POST) {
        object contentType = basicAt(request, 3);
        object body = basicAt(request, 4);
        ESP_LOGI(TAG, "POST body %s length %d", charPtr(body), sizeField(body) - 2);
        
        esp_http_client_set_header(client, "Content-Type", 
            contentType == nilobj ? "application/json" : charPtr(contentType));
        esp_http_client_set_post_field(client, charPtr(body), strlen(charPtr(body)));
    }
    
    // Perform the request
    object responseObj = nilobj;
    contentStr = nilobj;
    httpError = esp_http_client_perform(client);
    
    if (httpError == ESP_OK) {
        int statusCode = esp_http_client_get_status_code(client);
        int contentLength = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "Status = %d, content_length = %d", statusCode, contentLength);
        
        // Create a response object
        responseObj = allocObject(3);
        setClass(responseObj, globalSymbol("HttpResponse"));
        basicAtPut(responseObj, 1, newInteger(statusCode));
        basicAtPut(responseObj, 2, newInteger(contentLength));
        basicAtPut(responseObj, 3, contentStr);
    }

    http_cleanup();
    return responseObj;
}

/**
 * Helper function for making simple HTTP requests
 * 
 * This is a utility function for making basic HTTP requests without
 * creating Smalltalk objects. It sets a default URL, method, and header,
 * then performs the request and logs the result.
 * 
 * HTTP request object structure: url, method, header
 * HTTP response object structure: status code, content length, content
 * 
 * @param url The URL to request (ignored, as it's overridden)
 * @param method The HTTP method to use
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
