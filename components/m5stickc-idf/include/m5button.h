/**
 * m5button.h
 *
 * (C) 2019 - Pablo Bacho <pablo@pablobacho.com>
 * This code is licensed under the MIT License.
 */

#ifndef _M5BUTTON_H_
#define _M5BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "m5event.h"
#include "driver/gpio.h"

#include "build.h"

#if TARGET_DEVICE == DEVICE_T_WRISTBAND

#define BUTTON_BUTTON_A_GPIO      GPIO_NUM_33

#else

#define BUTTON_BUTTON_A_GPIO      GPIO_NUM_37

#endif

#define M5BUTTON_BUTTON_B_GPIO      GPIO_NUM_39

ESP_EVENT_DECLARE_BASE(M5BUTTON_A_EVENT_BASE); /*!< BASE event of button A */
ESP_EVENT_DECLARE_BASE(M5BUTTON_B_EVENT_BASE); /*!< BASE event of button B */

#define M5BUTTON_TASK_STACK_DEPTH   2048

/*!< event_group flag bits */
#define M5BUTTON_BUTTON_PUSH_BIT      0b00000001
#define M5BUTTON_BUTTON_POP_BIT       0b00000010

/*!< Time constants */
#define M5BUTTON_DEBOUNCE_TIME   10
#define M5BUTTON_HOLD_TIME       2000

/**
 * List of possible events this module can trigger
 */
typedef enum {
    M5BUTTON_BUTTON_CLICK_EVENT = 0,        /*!< Normal button press */
    M5BUTTON_BUTTON_HOLD_EVENT,             /*!< Button hold */
    M5BUTTON_EVENT_MAX
} m5button_event_id_t;

typedef struct {
    gpio_num_t gpio;                                    /*!< Button GPIO number */
    uint32_t debounce_time;                             /*!< Button debounce time */
    uint32_t hold_time;                                 /*!< Button hold time */
    esp_event_base_t esp_event_base;                    /*!< Button event base */
    EventGroupHandle_t event_group;                     /*!< Event group handle */
    TaskHandle_t task;                                  /*!< Button task handle */
    #if defined(CONFIG_SUPPORT_STATIC_ALLOCATION)
    StaticEventGroup_t event_group_buffer;              /*!< Event group buffer for static allocation */
    StaticTask_t task_buffer;                           /*!< Task buffer for static allocation */
    StackType_t task_stack[M5BUTTON_TASK_STACK_DEPTH];  /*!< Task stack for static allocation */
    #endif // STATIC_ALLOCATION
} m5button_t;

extern m5button_t m5button_a;   /*!< Button A is the button on the front of M5StickC */
extern m5button_t m5button_b;   /*!< Button B is the button on the side of M5StickC, far from the USB connector */

/**
 * @brief   Button interrupt service routine.
 */
void IRAM_ATTR m5button_button_isr_handler(void* arg);

/**
 * @brief   Initialize buttons
 *
 *          Initializes common resources shared by all buttons. It does not enable any buttons.
 *
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5button_init();

/**
 * @brief   Enable button logic, needed to use the button.
 *
 * @param   button button to enable
 * @return  ESP_OK success
 *          ESP_FAIL failed
 */
esp_err_t m5button_enable(m5button_t * button);

/**
 * @brief   Disable button logic.
 *
 * @param   button button to disable
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_INVALID_ARG button null
 */
esp_err_t m5button_disable(m5button_t * button);

/**
 * @brief   Set button's GPIO as input.
 *
 * @param   button button to enable
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_INVALID_ARG button null
 */
esp_err_t m5button_set_as_input(m5button_t * button);

/**
 * @brief   Enable interrupt for a button, needed to use events.
 *
 * @param   button button to enable interrupt of
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_INVALID_ARG button null
 */
esp_err_t m5button_enable_interrupt(m5button_t * button);

/**
 * @brief   Disable interrupt for a button.
 *
 * @param   button button to disable interrupt of
 * @return  ESP_OK success
 *          ESP_FAIL failed
 *          ESP_ERR_INVALID_ARG button null
 */
esp_err_t m5button_disable_interrupt(m5button_t * button);

/**
 * @brief   Check if button is pressed
 *
 * @param   button button to check
 * @return  false not pressed
 *          true otherwise pressed
 */
bool m5button_is_pressed(m5button_t * button);

/**
 * @brief   Generates button events
 *
 *          This task generates button events such as press and release. It includes a debouncing mechanism.
 *          First, the task will wait for a key push or pop. When detected, it will "propose" a candidate event to be sent
 *          and configure a timer of 100ms. If the timer expires the candidate event is posted to the default loop. If
 *          button state changes before the timeout, it will propose the new state of button as candidate for event and
 *          restart timer again.
 *
 * @param   pvParameter pointer to m5button
 */
void m5button_task(void * pvParameter);

#ifdef __cplusplus
}
#endif

#endif // _M5BUTTON_H_