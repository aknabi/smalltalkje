/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	System Primitives Implementation
    
    This module implements system-level primitives for Smalltalkje, with a focus
    on ESP32-specific functionality. It provides Smalltalk code access to hardware
    features and system capabilities through primitive functions.
    
    The implementation supports:
    - Task and process management for FreeRTOS
    - GPIO manipulation
    - Display control for various ESP32 devices (M5StickC, SSD1306)
    - I2C communication and interrupt handling
    - Date and time operations
    - HTTP and network functionality
    - Non-volatile storage (NVS) operations
    - System information and platform details
    
    TODO: Structure this so that the target (e.g. ESP32, nRF53, Dialog) code
    is in a target file and the prim definitions are here calling those.
*/

#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "memory.h"
#include "target.h"
#include "names.h"

#include "build.h"
#include "tty.h"
#include "datetime.h"

#include "primitive.h"

#ifdef TARGET_ESP32
#include "esp32wifi.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_sntp.h"

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
#include "ssd1306_oled.h"
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
#include "tft.h"

/** I2C port 1 clock speed (100 KHz) - needs to be slow for CardKB Hat */
#define I2C_PORT_1_CLK_SPEED 100000

/** SDA GPIO pin for I2C port 1 (on the M5StickC 8-pin connector) */
#define I2C_PORT_1_SDA_GPIO_PIN 0

/** SCL GPIO pin for I2C port 1 (on the M5StickC 8-pin connector) */
#define I2C_PORT_1_SCL_GPIO_PIN 26

#endif

#endif

#ifdef TARGET_ESP32

/*
 * I2C Interrupt Support Code
 */

/** I2C port used for the CardKB keyboard */
#define CARD_KB_I2C_PORT I2C_NUM_1

/** Maximum data length for I2C read/write test operations */
#define RW_TEST_LENGTH 32

/** Queue for I2C events */
static xQueueHandle i2c_event_queue = NULL;

/** Interrupt handle for I2C slave */
intr_handle_t i2c_slave_intr_handle;

/** Mutex for print operations during interrupt handling */
SemaphoreHandle_t print_mux = NULL;

/** Structure for I2C queue messages */
struct i2CQueueMessage{
	int portNumber;
} xMessage;

/**
 * I2C interrupt handler
 * 
 * This function is called when an I2C interrupt occurs. It creates a message
 * with the I2C port number and sends it to the event queue for processing.
 * It also frees the interrupt handler to allow for re-registration.
 * 
 * @param args Interrupt handler arguments (unused)
 */
void IRAM_ATTR i2c_interrupt(void *args){
	ets_printf("i2c_interrupt has been triggered\n");
	
	// Create a message with the I2C port number
	struct i2CQueueMessage *message;
	message = (struct i2CQueueMessage*) malloc(sizeof(struct i2CQueueMessage));
	message->portNumber = CARD_KB_I2C_PORT;
	
	// Free the interrupt handler	
	if(i2c_isr_free(i2c_slave_intr_handle) == ESP_OK) {
		i2c_slave_intr_handle = NULL;
		ets_printf("Free-ed interrupt handler\n");
	} else {
		ets_printf("Failed to free interrupt handler\n");
	}

	// Send the message to the event queue
	BaseType_t ret = xQueueSendFromISR(i2c_event_queue, &message, NULL);
	if(ret != pdTRUE){
		ets_printf("Could not send event to queue (%d)\n", ret);
	}
}

/** Tag for ESP logging related to I2C operations */
const char* TAG = "I2C";

/**
 * Install and configure the I2C port 1 driver
 * 
 * This function configures and installs the I2C driver for port 1, which
 * is used for the CardKB keyboard. It sets up the GPIO pins, clock speed,
 * and other parameters.
 * 
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t installI2CPort1Driver()
{
    esp_err_t e;
    
    // Configure I2C parameters
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_PORT_1_SDA_GPIO_PIN;
    conf.scl_io_num = I2C_PORT_1_SCL_GPIO_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_PORT_1_CLK_SPEED;
    
    // Set I2C parameters
    e = i2c_param_config(I2C_NUM_1, &conf);
    if(e == ESP_OK) {
        // Install I2C driver
        e = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
        if(e != ESP_OK) {
            ESP_LOGE(TAG, "Error during I2C 1 driver install: %s", esp_err_to_name(e));
        }
    } else {
        ESP_LOGE(TAG, "Error during I2C 1 param config installation: %s", esp_err_to_name(e));
    }

    return e;
}

/**
 * Set up I2C interrupt handling
 * 
 * This function configures I2C interrupt handling for the CardKB keyboard.
 * It creates an event queue and mutex, deletes any existing I2C driver,
 * registers an interrupt handler, and reinstalls the I2C driver.
 * 
 * @param i2c_addr The I2C address to monitor (unused)
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t setupI2CInterrupt(i2c_port_t i2c_addr)
{
    esp_err_t e;

    // Create event queue and mutex if they don't exist
    if (i2c_event_queue == NULL) {
        i2c_event_queue = xQueueCreate(5, sizeof(uint32_t *));
    }
    if (print_mux == NULL) {
        print_mux = xSemaphoreCreateMutex();
    }

    // Delete any existing I2C driver
    e = i2c_driver_delete(CARD_KB_I2C_PORT);
    ets_printf("i2c_driver_delete returned: %s\n", esp_err_to_name(e));

    // Register the I2C interrupt handler
    e = i2c_isr_register(CARD_KB_I2C_PORT, &i2c_interrupt, NULL, 0, &i2c_slave_intr_handle);
    ets_printf("i2c_isr_register returned: %s\n", esp_err_to_name(e));

    // Install the I2C driver
    e = installI2CPort1Driver();
    ets_printf("installI2CPort1Driver returned: %s\n", esp_err_to_name(e));

    return e;
}

/**
 * Forward declaration for i2cReadByte function
 */
esp_err_t i2cReadByte(uint8_t i2c_addr, uint8_t *data_byte);

/**
 * Task to read data from the CardKB keyboard
 * 
 * This function runs as a FreeRTOS task that continuously polls the
 * CardKB keyboard for input. When a key is pressed, it queues a block
 * to be executed by the Smalltalk VM with the key code as an argument.
 * 
 * @param arg Task argument (unused)
 */
static void read_card_kb_task(void *arg) {
    // Create event queue if it doesn't exist
    if (i2c_event_queue == NULL) {
        i2c_event_queue = xQueueCreate(5, sizeof(uint8_t *));
    }
    
    uint8_t dataByte = 0;
    uint8_t kbPort = 95;  // I2C address of the CardKB keyboard

    while(1) {
        // Read a byte from the keyboard
        esp_err_t e = i2cReadByte(kbPort, &dataByte);
        
        // If successful and a key was pressed, queue a block to handle it
        if (e == ESP_OK && dataByte > 0) {
            object kbBlock = nameTableLookup(globalSymbol("EventHandlerBlocks"), "KeyboardChar");
            if (kbBlock != nilobj)
            {
                queueBlock(kbBlock, newInteger((int)dataByte));
            }
        }
        
        // Short delay before checking again
        vTaskDelay(portTICK_RATE_MS * 2 / 1000);
    }
    
    vTaskDelete(NULL);
}

/**
 * Task to handle I2C interrupt events
 * 
 * This function runs as a FreeRTOS task that processes I2C interrupt events
 * from the event queue. When an interrupt occurs, it reads data from the I2C
 * slave device and re-registers the interrupt handler.
 * 
 * @param arg Task argument (unused)
 */
static void i2c_handle_interrupt_task(void *arg){
    // Log task startup
    xSemaphoreTake(print_mux, portMAX_DELAY);
    ESP_LOGI(TAG, "Starting i2c_handle_interrupt task");
    ESP_LOGI(TAG, "Waiting for i2c events in the event queue");
    xSemaphoreGive(print_mux);
    
    while(1){
        xSemaphoreTake(print_mux, portMAX_DELAY);
        
        // Wait for a message from the event queue
        struct QueueMessage *message;
        BaseType_t ret = xQueueReceive(i2c_event_queue, &(message), 1000 / portTICK_RATE_MS);
        
        if(ret){
            ESP_LOGI(TAG, "Found new I2C event to handle");
            ESP_LOGI(TAG, "Resetting queue");
            
            free(message);
            
            // Allocate buffer for I2C data
            int size;
            uint8_t *data = (uint8_t *)malloc(RW_TEST_LENGTH);
            
            // Read the data length first
            int data_length = 0;
            size = i2c_slave_read_buffer(CARD_KB_I2C_PORT, &data, 16, 1000 / portTICK_RATE_MS);
            
            if(size){
                data_length = atoi((char*)data);
                ESP_LOGI(TAG, "Master told me that there are a few bytes comming up");
                printf("%d bytes to be precise", size);
                disp_buf(data, size);
            } else {
                ESP_LOGW(TAG, "i2c_slave_read_buffer returned -1");
            }
            
            // Read the actual data
            size = i2c_slave_read_buffer(CARD_KB_I2C_PORT, &data, data_length, 1000 / portTICK_RATE_MS);
            
            if(size != data_length){
                ESP_LOGW(TAG, "I2C expected data length vs read data length does not match");
            } else {
                disp_buf(data, size);
            }
            
            // Re-register the interrupt handler
            ESP_LOGI(TAG, "Registering interrupt again");
            esp_err_t isr_register_ret = i2c_isr_register(CARD_KB_I2C_PORT, i2c_interrupt, NULL, 0, &i2c_slave_intr_handle);
            
            if(isr_register_ret == ESP_OK){
                ESP_LOGI(TAG, "Registered interrupt handler");
            } else {
                ESP_LOGW(TAG, "Failed to register interrupt handler");
            }            
        } else {
            ESP_LOGW(TAG, "Failed to get queued event");
            printf("xQueueReceive() returned %d\n", ret);
        }
        
        xSemaphoreGive(print_mux);
        vTaskDelay(portTICK_RATE_MS / 1000);
    }
    
    vSemaphoreDelete(print_mux);    
    vTaskDelete(NULL);
}

/**
 * Read a single byte from an I2C device
 * 
 * This function reads a single byte from an I2C device at the specified address.
 * It creates an I2C command that starts communication, reads a byte, and stops.
 * 
 * @param i2c_addr The I2C address of the device to read from
 * @param data_byte Pointer to store the read byte
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t i2cReadByte(uint8_t i2c_addr, uint8_t *data_byte)
{
    esp_err_t e;
    i2c_cmd_handle_t cmd;

    // Create an I2C command link
    cmd = i2c_cmd_link_create();

    // Build the I2C command to read a byte
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data_byte, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    // Execute the I2C command
    e = i2c_master_cmd_begin(I2C_NUM_1, cmd, 50/portTICK_PERIOD_MS);

    // Check for errors
    if (e != ESP_OK) {
        ESP_LOGE("ESP32", "error reading I2C byte addr %d (%s)", i2c_addr, esp_err_to_name(e));
    }
    
    // Clean up
    i2c_cmd_link_delete(cmd);

    return e;
}

/** Counter for task control */
int counter = 0;

/** Array of button handler processes */
object buttonProcesses[4] = {nilobj, nilobj, nilobj, nilobj};

/**
 * Add a button handler process
 * 
 * This function registers a Smalltalk process as a handler for a specific button.
 * The button is identified by an index (1-4), and the handler process is stored
 * in the buttonProcesses array.
 * 
 * @param arguments Array of Smalltalk objects as arguments:
 *        - arguments[1]: Button index (Integer)
 *        - arguments[2]: Handler process
 */
void addButtonHandlerProcess(object *arguments)
{
    sysWarn("start adding handler...", "addButtonHandlerProcess");
    checkIntArg(1);
    object handerProcess = arguments[2];
    
    // Check if button index is valid
    if (getIntArg(1) > 3)
        return;
        
    sysWarn("now adding handler...", "addButtonHandlerProcess");
    fprintf(stderr, "Button object handler process: %d", handerProcess);
    
    // Store the handler process
    buttonProcesses[getIntArg(1) - 1] = handerProcess;
}

/** Type definition for primitive functions */
typedef void (*primFunc_t)(object *);

/** External declaration for runSmalltalkProcess function */
extern void runSmalltalkProcess(void *process);

/** Array of M5 primitive functions */
primFunc_t m5PrimitiveFunctions[] = {&addButtonHandlerProcess};

/**
 * Run a Smalltalk process as a FreeRTOS task
 * 
 * This function executes a Smalltalk process in a FreeRTOS task.
 * When the process completes, the task is automatically deleted.
 * 
 * @param process Pointer to the Smalltalk process to run
 */
void runTask(void *process)
{
    runSmalltalkProcess(process);
    /* delete a task when finish */
    vTaskDelete(NULL);
}

#endif

/** External declaration for runBlockAfter function */
extern void runBlockAfter(object block, object arg, int ticks);

#endif

/** Global object for platform name string */
object platformNameStStr = nilobj;

/** Global object for current time string */
object currentTimeStStr = nilobj;

object sysPrimitive(int number, object *arguments)
{
    object returnedObject = nilobj;
    int funcNum;

    /* someday there will be more here */
    switch (number - 150)
    {
    case 0: /* do a system() call */
        returnedObject = newInteger(system(charPtr(arguments[0])));
        break;

#ifdef TARGET_ESP32
    case 1: /* prim 151 create a OS task with a ST process */
        ;   // Semicolon solves for "error: a label can only be part of a statement and a declaration is not a statement"
        // TaskHandle_t *taskHandle = NULL;;
        // BaseType_t xReturned = xTaskCreate(
        object processToRun = arguments[0];
        if (classField(processToRun) != findClass("Process"))
        {
            sysError("forkTask argument must be a process", "taskDelay");
        }
        xTaskCreate(
            runTask,      /* Task function. */
            "runTask",    /* name of task. */
            8096,         /* Stack size of task */
            arguments[0], /* parameter of the task (the Smalltalk process to run) */
            1,            /* priority of the task */
            NULL);        /* Task handle to keep track of created task */

        // We'd like to return the handle in order to manage the process.
        break;

    case 2: /* prim 152 delays the current OS task with a ST process for a given number of milliseconds */
        checkIntArg(1) if (arguments[0] == nilobj)
        {
            vTaskDelay(intValue(arguments[1]) / portTICK_PERIOD_MS);
        }
        else
        {
            runBlockAfter(arguments[0], nilobj, arguments[1] / portTICK_PERIOD_MS);
        }
        // We'd like to return the handle in order to manage the process.
        break;

#ifdef DEVICE_DISPLAY_TYPE

    /* prim 153 calls a display function (first arg). E.g. 0 is init display and must be called before displaying */
    // TODO: This should be in Smalltalk initialization as it inits more than the display on some targets
    case 3:
        checkIntArg(0)
        int funcNum = getIntArg(0);

        if (funcNum == 0) {
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
            SSD1306_Begin();
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
#ifndef TEST_M5STICK
            m5StickInit();
#endif

#endif
        } else if (funcNum == 1) {
            // Set backlight on or off based on arg(1)
            object backlightOn = arguments[1];
#if TARGET_DEVICE == DEVICE_M5STICKC
            m5display_set_backlight_level(backlightOn == falseobj ? 0 : 7);
#elif TARGET_DEVICE == DEVICE_T_WRISTBAND
            gpio_set_level(27, backlightOn == falseobj ? 0 : 1);
#endif
        } else if (funcNum == 2) {
            // Clear the display
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
            SSD1306_ClearDisplay();
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
            color_t bgColor = *current_paint.backgroundColor;
            TFT_fillScreen(bgColor);
#endif
        } else if (funcNum == 3) {
            // Render the buffer to the display
#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
            SSD1306_Display();
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
            // M5StickC doesn't have offscren render
#endif
        }
        break;

    // prim 154 Available for use
    case 4:
        break;

    // Prim 155 Available for use
    case 5:
        break;

    // Prim 156 String functions (arg 0 is function num) 
    case 6:
        checkIntArg(0)
        funcNum = getIntArg(0);

        if (funcNum == 0) {
            // Func 0 Display the string at the x,y location passed in
            checkIntArg(2)
            checkIntArg(3)

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
            SSD1306_DrawText(
                getIntArg(1),
                getIntArg(2),
                charPtr(arguments[0]),
                1);
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
            TFT_resetclipwin();
        // TFT_setFont(DEFAULT_FONT, NULL);
            TFT_print(charPtr(arguments[1]), getIntArg(2), getIntArg(3));
#endif

        } else if (funcNum == 1) {
            // Func 1 - Display the character at the x,y location passed in

        } else if (funcNum == 2) {
            // Func 1 - Return the width of the string passed in
            returnedObject = newInteger(TFT_getStringWidth(charPtr(arguments[1])));
        } else if (funcNum == 20) {
            // Func 1 - Set font to the font number second argument 
            checkIntArg(1)
            TFT_setFont(intValue(arguments[1]), NULL);
        } else if (funcNum == 21) {
            // Func 1 - Set 7 Seg params to the l, w, o arguments passed in
            _fg = TFT_ORANGE;
            checkIntArg(1)
            checkIntArg(2)
            checkIntArg(3)
            TFT_setFont(FONT_7SEG, NULL);
            set_7seg_font_atrib(getIntArg(1), getIntArg(2), getIntArg(3), TFT_DARKGREY);
        }
        break;

    // Prim 157 rectangleX: x y: y width: w height: h isFilled: aBoolean
    case 7:
        checkIntArg(0)
        checkIntArg(1)
        checkIntArg(2)
        checkIntArg(3)
        if (arguments[4] != trueobj && arguments[4] != falseobj)
        {
            sysError("non boolean argument", "isFilled");
        }

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        if (arguments[4] == trueobj)
        {
            SSD1306_FillRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3),
                oled_color_white);
        }
        else
        {
            SSD1306_DrawRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3));
        }
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
        if (arguments[4] == trueobj)
        {
            TFT_fillRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3),
                TFT_WHITE);
        }
        else
        {
            TFT_drawRect(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                getIntArg(3),
                TFT_WHITE);
        }
        break;

    // Prim 158 circleX: x y: y radius: r isFilled: aBoolean
    case 8:
        checkIntArg(0)
        checkIntArg(1)
        checkIntArg(2)

        if (arguments[3] != trueobj && arguments[3] != falseobj)
        {
            sysError("non boolean argument", "isFilled");
        }

#if TARGET_DEVICE == DEVICE_ESP32_SSD1306
        if (arguments[4] == trueobj)
        {
            SSD1306_FillCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                oled_color_white);
        }
        else
        {
            SSD1306_DrawCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2));
        }
#elif TARGET_DEVICE == DEVICE_M5STICKC || TARGET_DEVICE == DEVICE_T_WRISTBAND
        if (arguments[4] == trueobj)
        {
            TFT_fillCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                TFT_WHITE);
        }
        else
        {
            TFT_drawCircle(
                getIntArg(0),
                getIntArg(1),
                getIntArg(2),
                TFT_WHITE);
        }
#endif

        break;

#endif

#endif // DEVICE_DISPLAY_TYPE

    // Prim 159 set GPIO pin mode and direction in first arg to mode in second arg
    case 9:
        checkIntArg(0)
        checkIntArg(1)

                gpio_mode_t gpioMode;
        gpio_pad_select_gpio(getIntArg(0));

        switch (getIntArg(1))
        {
        case 0:
            gpioMode = GPIO_MODE_DISABLE;
            break; // disable input and output
        case 1:
            gpioMode = GPIO_MODE_INPUT;
            break; // input only
        case 2:
            gpioMode = GPIO_MODE_OUTPUT;
            break; // output only mode
        case 3:
            gpioMode = GPIO_MODE_OUTPUT_OD;
            break; // output only with open-drain mode
        case 4:
            gpioMode = GPIO_MODE_INPUT_OUTPUT_OD;
            break; // output and input with open-drain mode
        case 5:
            gpioMode = GPIO_MODE_INPUT_OUTPUT;
            break; // output and input mode
        default:
            gpioMode = GPIO_MODE_OUTPUT;
            break;
        }
        gpio_set_direction(getIntArg(0), gpioMode);
        break;

    // Prim 160 set GPIO pin level in first arg to value in second arg
    case 10:
        checkIntArg(0)
        checkIntArg(1)
        gpio_set_level(getIntArg(0), getIntArg(1));
        break;

    // Prim 170 ESP32 functions. First arg is function number, second and third are arguments to the function
    case 20:
        checkIntArg(0);
        funcNum = getIntArg(0);
        // function 0 wifi_start()
        if (funcNum == 0)
            wifi_start();
        // function 1 is set wifi ssid and password
        else if (funcNum == 1) {
            if (arguments[1] != nilobj)
                wifi_set_ssid(charPtr(arguments[1]));
            if (arguments[2] != nilobj)
                wifi_set_password(charPtr(arguments[2]));
        } else if (funcNum == 2) {
            wifi_connect();
        } else if (funcNum == 3) {
            returnedObject = wifi_scan();
        } else if(funcNum == 20) {
            // Get I2C Byte at the I2C Address in the second prim argument
            uint8_t dataByte = 0;
            esp_err_t e = i2cReadByte(intValue(arguments[1]), &dataByte);
            returnedObject = (e == ESP_OK) ? newInteger(dataByte) : newError(newInteger(e));
        } else if (funcNum == 21) {
            // Setup an I2C hander at the I2C Address in the second prim argument
            // For now let's just do it for the keyboard, then generalize it.
            esp_err_t e = setupI2CInterrupt(intValue(arguments[1]));

        } else if (funcNum == 22) {
            // Start the Card Keyboard input task
            BaseType_t r = xTaskCreate(read_card_kb_task, "card_kb_task", 2048, NULL, 20, NULL);
            if(r != pdPASS) {
                ESP_LOGE(TAG, "Error creating button_task");
                //return ESP_FAIL;
            }
        // 50's functions are for ESP32 Date/Time
        } else if (funcNum == 50) {
            // Sync time with SNTP server (assumes Wifi connected
            init_sntp_time();
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 51) {
            // Get SNTP based time (assuming init_sntp_time has been called)
            get_sntp_time();
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 52) {
            // Get ESP32 time (assuming get_sntp_time has been called)
            get_esp32_time();
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 53) {
            char *timeStr = current_time_string(charPtr(arguments[1]));
            returnedObject = timeStr == NULL ? nilobj : newStString(timeStr);
            // if (timeStr != NULL) {
            //     currentTimeStStr = newStString(timeStr);
            //     returnedObject = currentTimeStStr;
            // }
            break;
        }   else if (funcNum == 54) {
            setTimeZone(charPtr(arguments[1]));
        } else if (funcNum == 55) {
            returnedObject = newFloat((FLOAT) getEpochSeconds());
        } else if (funcNum == 56) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            returnedObject = newInteger( get_time_component(&epochSecs, intValue(arguments[2])) );
        } else if (funcNum == 57) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            char *timeStr = time_string(epochSecs, charPtr(arguments[2]));
            returnedObject = timeStr == NULL ? nilobj : newStString(timeStr);
        } else if (funcNum == 58) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            time_t newEpoch = setNewDate(&epochSecs, getIntArg(2), getIntArg(3), getIntArg(4));
            returnedObject = newFloat((FLOAT) newEpoch);
        } else if(funcNum == 59) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            time_t newEpoch = setNewTime(&epochSecs, getIntArg(2),getIntArg(3), getIntArg(4));
            returnedObject = newFloat((FLOAT) newEpoch);
        } else if (funcNum == 100) {
            returnedObject = newInteger(GET_FREE_HEAP_SIZE());
        }
        break;

    // Prim 181 M5 functions. First arg is function number, second and third are arguments to the function
    // Index 0 means restart the SoC/system
    case 31:
        sysWarn("in primitive 181", "sysPrimitive");
        checkIntArg(0);
        funcNum = getIntArg(0);
        // Function number 0 is restart the ESP32
        if (funcNum == 0)
            esp_restart();
        // Lookup the function pointer in a primitive function table (trying vs. a switch)
        // We'll normalize other prim function number based lookups to this if it feels better
        int funcIndex = funcNum - 1;
        if (funcIndex != 0)
            break;
        primFunc_t m5Func = m5PrimitiveFunctions[funcIndex];
        m5Func(arguments);
        break;

    // Prim 182 ESP NVS functions. First arg is function number, rest are arguments to the function
    case 32:
        checkIntArg(0);
        funcNum = getIntArg(0);
        returnedObject = nvsPrim(funcNum, arguments);
        break;

    // Prim 183 ESP HTTP functions. First arg is function number, rest are arguments to the function
    case 33:
        checkIntArg(0);
        funcNum = getIntArg(0);
        returnedObject = httpPrim(funcNum, arguments);
        break;

#else
    case 1: /* prim 151 create a OS task with a ST process */
        break;

    case 2: /* prim 152 delays the current OS task with a ST process for a given number of milliseconds */
        break;

    /* prim 153 initializes the OLED display. Must be called before displaying */
    case 3:
        break;

    // prim 154 Clear the display
    case 4:
        break;

    // Prim 155 Render the buffer to the display
    case 5:
        break;

    // Prim 156 Display the string at the x,y location passed in
    case 6:
        /* Set GPIO PIN in first argument to value in second argument */
        // gpio_set_level(intValue(arguments[0]), intValue(arguments[1]));

        // We'd like to return the handle in order to manage the process.
        break;

    // Prim 157 rectangleX: x y: y width: w height: h isFilled: aBoolean
    case 7:
        break;

    // Prim 158 circleX: x y: y radius: r isFilled: aBoolean
    case 8:
        break;

    // Prim 159 set GPIO pin in first arg to mode in second arg
    case 9:
        break;

    // Prim 160 set GPIO pin in first arg to value in second arg
    case 10:
        break;

    // Prim 170 ESP32 Prim functions (support Date time with POSIX)
    // TODO: breakout generic POSIX date time into a seperate prim
    case 20:
        checkIntArg(0);
        funcNum = getIntArg(0);
        // 50's functions are for ESP32 Date/Time
        if (funcNum == 50) {
            // Sync time with SNTP server (assumes Wifi connected
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 51) {
            // Get SNTP based time (assuming init_sntp_time has been called)
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 52) {
            // Get ESP32 time (assuming get_sntp_time has been called)
            get_esp32_time();
            returnedObject = trueobj;
            break;
        }  else if (funcNum == 53) {
            char *timeStr = current_time_string(charPtr(arguments[1]));
            returnedObject = timeStr == NULL ? nilobj : newStString(timeStr);
            // if (timeStr != NULL) {
            //     currentTimeStStr = newStString(timeStr);
            //     returnedObject = currentTimeStStr;
            // }
            break;
        }   else if (funcNum == 54) {
            setTimeZone(charPtr(arguments[1]));
        } else if (funcNum == 55) {
            returnedObject = newFloat((FLOAT) getEpochSeconds());
        } else if (funcNum == 56) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            returnedObject = newInteger( get_time_component(&epochSecs, intValue(arguments[2])) );
        } else if (funcNum == 57) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            char *timeStr = time_string(&epochSecs, charPtr(arguments[2]));
            returnedObject = timeStr == NULL ? nilobj : newStString(timeStr);
        } else if (funcNum == 58) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            time_t newEpoch = setNewDate(&epochSecs, getIntArg(2), getIntArg(3), getIntArg(4));
            returnedObject = newFloat((FLOAT) newEpoch);
        } else if(funcNum == 59) {
            time_t epochSecs = (time_t) floatValue(arguments[1]);
            time_t newEpoch = setNewTime(&epochSecs, getIntArg(2),getIntArg(3), getIntArg(4));
            returnedObject = newFloat((FLOAT) newEpoch);
        }
        break;

    // Prim 181 M5 functions. First arg is function number, second and third are arguments to the function
    // Index 0 means restart the SoC/system
    case 31:
        break;
#endif


    // Prim 200 Platform Infomration functions (Argument is function number)
    case 50:
        checkIntArg(0);
        funcNum = getIntArg(0);
        // Function 0 Return Platform Name String
        if (funcNum == 0) {
            if (platformNameStStr == nilobj) {
                platformNameStStr = newStString(PLATFORM_NAME_STRING);
                // Since VM will keep a reference to it and we don't want it reused
                incr(platformNameStStr);
            }
            returnedObject = platformNameStStr;
        }  else if (funcNum == 1) {
            // Function 1 return true if gizmo supports a card keyboard
#ifdef CARD_KB_SUPPORTED
            returnedObject = trueobj;
#else
            returnedObject = falseobj;
#endif
        } else if (funcNum == 2) {
            // Function 2 return true if gizmo supports a card keyboard/display termial
#ifdef DEVICE_TERMINAL_SUPPORTED
            returnedObject = trueobj;
#else
            returnedObject = falseobj;
#endif
        }
        break;

    default:
        sysWarn("unknown primitive", "sysPrimitive");
    }
    return (returnedObject);
}
