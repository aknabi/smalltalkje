/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	UART (Serial) Communication Implementation
    
    This module provides UART (Universal Asynchronous Receiver-Transmitter) functionality
    for the Smalltalkje system. It handles serial communication for both ESP32 targets
    and other platforms, with platform-specific implementations.
    
    The ESP32 implementation uses the ESP-IDF UART driver and FreeRTOS task management
    to provide non-blocking input with timeouts. It includes support for:
    - UART initialization and configuration
    - Character input with optional timeout
    - Input event notification using task notifications
    - Select-based input monitoring
    
    For non-ESP32 platforms, a simpler implementation using standard C library
    functions is provided.
*/

#include "build.h"
#include "target.h"
#include <stdio.h>

#ifdef TARGET_ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"

/** Tag for ESP logging */
static const char *TAG = "uart_select_example";

/** Number of ticks to wait for character input before timeout */
#define TICKS_TO_WAIT_FOR_CHAR 5

/** File descriptor for UART device */
static int fd;

/** Handle for task waiting for input */
TaskHandle_t waitingTaskHandle = NULL;

#endif

/** Last character read from UART */
static char c = 0;

#ifdef TARGET_ESP32

/**
 * Close the UART connection
 * 
 * This function closes the UART file descriptor.
 */
static void uart_close()
{
    close(fd);
}

/** Counter for tracking timeout occurrences */
int timeoutCounter = 0;

/** Timeout structure for select() */
struct timeval tv = {
    .tv_sec = 0,        // seconds
    .tv_usec = 10000,   // microseconds (10ms)
};

/**
 * Initialize the UART
 * 
 * This function configures and initializes UART0 with standard settings:
 * - 115200 baud rate
 * - 8 data bits
 * - No parity
 * - 1 stop bit
 * - No flow control
 * 
 * It also opens the UART device file and configures the VFS to use the UART driver.
 */
void uart_init()
{
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    // Initialize UART with configuration
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0);

    // Open UART device file
    if ((fd = open("/dev/uart/0", O_RDWR)) == -1)
    {
        ESP_LOGE(TAG, "Cannot open UART");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return;
    }

    // Configure VFS to use UART driver
    esp_vfs_dev_uart_use_driver(0);
}

/**
 * Task that monitors UART for input
 * 
 * This task runs continuously in the background, using select() to monitor
 * the UART file descriptor for input. When input is available, it reads a
 * character and notifies any waiting task.
 * 
 * The task uses a timeout to avoid blocking indefinitely, periodically
 * checking if data is available.
 */
static void uart_select_task()
{
    while (true)
    {
        int s;
        fd_set rfds;

        // Set up the file descriptor set for select()
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        // Wait for input with timeout
        s = select(fd + 1, &rfds, NULL, NULL, &tv);

        if (s < 0)
        {
            // Select error
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            break;
        }
        else if (s == 0)
        {
            // Timeout - no input available
            if (++timeoutCounter > 500)
            {
                // Reset timeout counter every 500 timeouts
                timeoutCounter = 0;
            }
        }
        else
        {
            // Input is available
            if (FD_ISSET(fd, &rfds))
            {
                // Read one character
                if (read(fd, &c, 1) > 0)
                {
                    // Character read successfully
                    // Note: Only one character is read at a time, even if more are available.
                    // Subsequent calls to select() will return immediately for the remaining characters.

                    // Notify waiting task if any
                    if (waitingTaskHandle != NULL)
                        xTaskNotifyGive(waitingTaskHandle);
                }
                else
                {
                    ESP_LOGE(TAG, "UART read error");
                    break;
                }
            }
            else
            {
                ESP_LOGE(TAG, "No FD has been set in select()");
                break;
            }
        }
    }

    vTaskDelete(NULL);
}

/**
 * Initialize UART input monitoring
 * 
 * This function initializes the UART and creates a task to monitor it for input.
 * It configures the select() timeout and sets up the current task as the one to
 * be notified when input is available.
 */
void uart_input_init()
{
    // Initialize UART
    uart_init();
    
    // Configure select() timeout
    tv.tv_sec = 0;
    tv.tv_usec = 10000;  // 10ms

    // Set current task as the one to notify
    waitingTaskHandle = xTaskGetCurrentTaskHandle();

    // Create task to monitor UART
    xTaskCreate(uart_select_task, "uart_select_task", 4 * 1024, NULL, 5, NULL);
}

/**
 * Get a character from UART input with timeout
 * 
 * This function waits for a character from the UART with a timeout.
 * It sets the current task as the one to be notified when a character
 * is available, then waits for the notification. If the timeout expires
 * before a character is received, it returns 0.
 * 
 * @return The character received, or 0 if timeout expired
 */
char getInputCharacter()
{
    // Set fully buffered I/O
    setvbuf(stdin, NULL, _IOFBF, 80);

    // Set current task as the one to notify
    waitingTaskHandle = xTaskGetCurrentTaskHandle();
    
    // Clear the character buffer
    c = 0;
    
    // Wait for notification with timeout
    uint32_t charsReceived = ulTaskNotifyTake(pdTRUE, TICKS_TO_WAIT_FOR_CHAR);
    
    // Return character or 0 if timeout
    return charsReceived == 0 ? 0 : c;
}

#else

/**
 * Get a character from standard input (non-ESP32 version)
 * 
 * For non-ESP32 platforms, this function simply reads a character
 * from stdin using the standard C library function fgetc().
 * 
 * @return The character received
 */
char getInputCharacter()
{
    c = fgetc(stdin);
    return c;
}

#endif
