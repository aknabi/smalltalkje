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

static const char *TAG = "uart_select_example";

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

#define TICKS_TO_WAIT_FOR_CHAR 5

static int fd;
TaskHandle_t waitingTaskHandle = NULL;

#endif

static char c = 0;

#ifdef TARGET_ESP32

static void uart_close()
{
    close(fd);
}

int timeoutCounter = 0;

/*
    struct timeval {
        long    tv_sec;         // seconds
        long    tv_usec;        // and microseconds
    };
*/
struct timeval tv = {
    .tv_sec = 0,
    .tv_usec = 10000,
};

void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0);

    if ((fd = open("/dev/uart/0", O_RDWR)) == -1)
    {
        ESP_LOGE(TAG, "Cannot open UART");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return;
    }

    // We have a driver now installed so set up the read/write functions to use driver also.
    esp_vfs_dev_uart_use_driver(0);
}

static void uart_select_task()
{

    while (true)
    {
        int s;

        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        s = select(fd + 1, &rfds, NULL, NULL, &tv);

        if (s < 0)
        {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            break;
        }
        else if (s == 0)
        {
            if (++timeoutCounter > 500)
            {
                // ESP_LOGI(TAG, "Timeout 500 times");
                timeoutCounter = 0;
            }
        }
        else
        {
            if (FD_ISSET(fd, &rfds))
            {
                if (read(fd, &c, 1) > 0)
                {
                    // ESP_LOGI(TAG, "Received: %c", c);
                    // Note: Only one character was read even the buffer contains more. The other characters will
                    // be read one-by-one by subsequent calls to select() which will then return immediately
                    // without timeout.

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

void uart_input_init()
{
    uart_init();
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    waitingTaskHandle = xTaskGetCurrentTaskHandle();

    xTaskCreate(uart_select_task, "uart_select_task", 4 * 1024, NULL, 5, NULL);
}

char getInputCharacter()
{
    setvbuf(stdin, NULL, _IOFBF, 80);

    waitingTaskHandle = xTaskGetCurrentTaskHandle();
    c = 0;
    uint32_t charsReceived = ulTaskNotifyTake(pdTRUE, TICKS_TO_WAIT_FOR_CHAR);
    // if (c != 0) ESP_LOGI(TAG, "getInputCharacter Received: %c", c);
    return charsReceived == 0 ? 0 : c;
}

#else

char getInputCharacter()
{
    c = fgetc(stdin);
    return c;
}

#endif
