
/*
   Smalltalkje ESP32 WiFi Implementation
   
   This module provides WiFi connectivity functionality for the Smalltalk
   environment running on ESP32 devices. It implements:
   
   1. WiFi station (client) mode functionality
   2. Connection management and event handling
   3. Integration with Smalltalk event system
   4. Network scanning capabilities
   5. Support for RTC time synchronization via SNTP
   
   Based on ESP-IDF WiFi station example code, which is in the Public Domain
   (or CC0 licensed, at your option). Unless required by applicable law or
   agreed to in writing, this software is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "memory.h"
#include "names.h"

#include "esp32wifi.h"

/* The examples use WiFi configuration that you can set via project configuration menu
   If you'd rather not, just change the below entries to strings with
   the config you want
*/
#define EXAMPLE_ESP_MAXIMUM_RETRY  10000 
// #define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* Default WiFi credentials - can be changed via Smalltalk API */
#define WIFI_DEFAULT_SSID   "WIFI_DEFAULT_SSID"
#define WIFI_DEFAULT_PASS   "WIFI_DEFAULT_PASS"

/* Global WiFi configuration variables - accessible via setter functions */
char wifi_ssid[32] = WIFI_DEFAULT_SSID;      /* Current WiFi network SSID */
char wifi_password[64] = WIFI_DEFAULT_PASS;  /* Current WiFi password */

static const char *TAG = "wifi station";

static int s_retry_num = 0;

/**
 * WiFi event handler for the ESP32
 * 
 * This callback function handles various WiFi and IP events, including:
 * - WiFi station started: Initiates connection to the configured AP
 * - WiFi disconnection: Attempts reconnection with backoff
 * - IP acquisition: Signals successful connection
 * - Station connected: Triggers Smalltalk event handler blocks and initializes time via SNTP
 * 
 * The handler uses an event group to signal connection status to waiting threads
 * and integrates with the Smalltalk event system by queuing blocks to run
 * when connection events occur.
 * 
 * @param arg Opaque user data
 * @param event_base The base ID of the event to register the handler for
 * @param event_id The ID of the event to register the handler for
 * @param event_data Event data
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Failed to connect to SSID: [%s], password: [%s]", wifi_ssid, wifi_password);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        object eventDict = globalSymbol("EventHandlerBlocks");
        if (eventDict != nilobj) {
            object wifiBlock = nameTableLookup(eventDict, "WifiConnected");
            if (wifiBlock != nilobj) {
                queueVMBlockToRun(wifiBlock);
            }
        }

        // Use SNTP to get and set the time from the Internet
        m5rtc_init();
    }
}

/**
 * Initializes ESP32 WiFi in station mode
 * 
 * This function sets up the WiFi subsystem in station (client) mode by:
 * 1. Creating an event group for synchronization
 * 2. Initializing the TCP/IP stack (netif)
 * 3. Creating the default event loop
 * 4. Setting up a default station interface
 * 5. Initializing the WiFi driver with default configuration
 * 6. Setting the operation mode to station
 * 
 * After calling this function, the WiFi subsystem is ready to connect,
 * but no actual connection attempt is made yet.
 */
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
}

/**
 * Connects to WiFi using the stored SSID and password
 * 
 * This is a convenience function that calls wifi_connect_to() with
 * the currently stored wifi_ssid and wifi_password values.
 */
void wifi_connect(void)
{
    wifi_connect_to(wifi_ssid, wifi_password);
}

/**
 * Connects to a specified WiFi network
 * 
 * This function attempts to connect to a WiFi access point using the provided
 * credentials. It:
 * 1. Registers event handlers for WiFi and IP events
 * 2. Configures the WiFi station with the provided SSID and password
 * 3. Starts the WiFi subsystem and initiates connection
 * 4. Waits for connection success or failure
 * 5. Triggers Smalltalk event handlers on successful connection
 * 
 * @param ssid The SSID (network name) to connect to
 * @param password The password for the WiFi network
 */
void wifi_connect_to(char *ssid, char *password)
{
    esp_event_handler_t instance_any_id = &wifi_event_handler;
    esp_event_handler_t instance_got_ip = &wifi_event_handler;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                                ESP_EVENT_ANY_ID,
                                                &wifi_event_handler,
                                                NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                                IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler,
                                                NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_DEFAULT_SSID,
            .password = WIFI_DEFAULT_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Non-event handler - connected to ap SSID: %s password: %s", wifi_ssid, wifi_password);
        object eventDict = globalSymbol("EventHandlerBlocks");
        if (eventDict != nilobj) {
            object wifiBlock = nameTableLookup(eventDict, "WifiConnected");
            if (wifiBlock != nilobj) {
                queueVMBlockToRun(wifiBlock);
            }
        }

        m5rtc_init();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s, password: %s", wifi_ssid, wifi_password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

/**
 * Initializes the WiFi subsystem
 * 
 * This function prepares the WiFi subsystem for use, initializing it in
 * station mode, but doesn't actually attempt to connect to any network.
 * The function is typically called early in the system initialization
 * process.
 */
void wifi_start(void)
{
    // This has been done at startp, but it's required for ESP32 wifi
    // nvs_init();

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    // wifi_connect();
}

/**
 * Sets the WiFi SSID for future connections
 * 
 * Updates the stored SSID to be used in subsequent connection attempts.
 * This function does not initiate a connection; it only stores the 
 * provided SSID for later use with wifi_connect().
 * 
 * @param ssid The SSID (network name) to store
 */
void wifi_set_ssid(char *ssid)
{
    strcpy(wifi_ssid, ssid);
}

/**
 * Sets the WiFi password for future connections
 * 
 * Updates the stored password to be used in subsequent connection attempts.
 * This function does not initiate a connection; it only stores the
 * provided password for later use with wifi_connect().
 * 
 * @param password The password to store
 */
void wifi_set_password(char *password)
{
    strcpy(wifi_password, password);
}

#define DEFAULT_SCAN_LIST_SIZE 10

static void print_auth_mode(int authmode);

/**
 * Scans for available WiFi networks and returns them as a Smalltalk array
 * 
 * This function initiates a WiFi scan to discover nearby access points.
 * The scan is performed synchronously (blocking until complete) and returns
 * the results as a Smalltalk Array containing String objects representing
 * the SSIDs of found networks.
 * 
 * The scan is limited to DEFAULT_SCAN_LIST_SIZE (10) networks.
 * 
 * @return A Smalltalk Array of String objects representing detected SSIDs
 */
object wifi_scan(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    object resultArray = newArray(ap_count > DEFAULT_SCAN_LIST_SIZE ? DEFAULT_SCAN_LIST_SIZE : ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        basicAtPut(resultArray, i + 1, newStString((char *) ap_info[i].ssid));
        // ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        // ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        // print_auth_mode(ap_info[i].authmode);
        // ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
    esp_wifi_stop();
    return resultArray;
}

/**
 * Helper function to print human-readable WiFi authentication mode
 * 
 * Translates the numeric authentication mode constants from the ESP-IDF
 * WiFi library into readable strings for logging purposes.
 * 
 * @param authmode The authentication mode constant from ESP-IDF WiFi library
 */
static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}
