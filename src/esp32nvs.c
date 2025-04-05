/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

    ESP32 Non-Volatile Storage Implementation
    
    This module provides access to the ESP32's Non-Volatile Storage (NVS) functionality
    from Smalltalk. NVS is used to store persistent data like settings, configurations,
    and other values that need to survive power cycles.
    
    The implementation supports:
    - Reading and writing integers, strings, and byte arrays
    - Initializing the NVS system
    - Erasing specific keys
    - Error handling for NVS operations
    
    NVS operations are exposed to Smalltalk through primitive functions that can
    be called from Smalltalk code.
*/

#include "nvs_flash.h"
#include "env.h"
#include "memory.h"
#include "names.h"

/** Handle for NVS operations */
nvs_handle nvsHandle;

/** Length of value being read from NVS */
size_t valLength = 0;

/**
 * NVS primitive functions for Smalltalk interface
 * 
 * This function implements the following primitive operations for NVS:
 * - 0: Initialize NVS
 * - 1: Write an object to NVS
 * - 2: Read an object from NVS
 * - 3: Erase a key from NVS
 * 
 * @param funcNumber The primitive function number
 * @param arguments Array of Smalltalk objects as arguments
 * @return Result object from the primitive operation
 */
object nvsPrim(int funcNumber, object *arguments)
{
    switch (funcNumber)
    {
        case 0:  // Initialize NVS
            return nvs_init();
            
        case 1:  // Write object to NVS
            return writeObject(charPtr(arguments[1]), arguments[2]);
            
        case 2:  // Read object from NVS
            return readObject(charPtr(arguments[1]), arguments[2]);
            
        case 3:  // Erase key from NVS
            return eraseKey(charPtr(arguments[1]));
            
        default:
            break;
    }
    
    return nilobj;
}

/**
 * Erase a key from NVS
 * 
 * This function removes a specific key and its associated value from NVS.
 * 
 * @param key Name of the key to erase
 * @return trueobj if successful, falseobj if failed
 */
object eraseKey(char *key)
{
    esp_err_t err = nvs_erase_key(nvsHandle, key);
    err = nvs_check_error(err, true, key);
    return err == ESP_OK ? trueobj : falseobj;
}

/**
 * Write a Smalltalk object to NVS
 * 
 * This function writes a Smalltalk object to NVS based on its class type:
 * - Integer: Written as a 32-bit integer
 * - String: Written as a string
 * - ByteArray: Written as a binary blob
 * 
 * @param key Name of the key to store the value under
 * @param obj The Smalltalk object to store
 * @return trueobj if successful, falseobj if failed
 */
object writeObject(char *key, object obj)
{
    // Default error if object passed isn't a supported class
    esp_err_t err = ESP_ERR_INVALID_ARG;
    object c = getClass(obj);
    
    if (isClassNameEqual(c, "Integer")) {
        err = nvs_write_int32(key, intValue(obj));
    } else if (isClassNameEqual(c, "String")) {
        err = nvs_write_string(key, charPtr(obj));
    } else if (isClassNameEqual(c, "ByteArray")) {
        err = nvs_write_byte_array(key, charPtr(obj), sizeField(obj));
    }
    
    return err == ESP_OK ? trueobj : falseobj;
}

/**
 * Read a Smalltalk object from NVS
 * 
 * This function reads a value from NVS and creates a Smalltalk object
 * of the specified class type:
 * - Integer: Read as a 32-bit integer
 * - String: Read as a string
 * - ByteArray: Read as a binary blob
 * 
 * @param key Name of the key to read
 * @param c Class of the object to create (determines how to read the value)
 * @return The Smalltalk object containing the value or nilobj if failed
 */
object readObject(char *key, object c)
{
    object obj = nilobj;
    esp_err_t err;
    
    if (isClassNameEqual(c, "Integer")) {
        // Handle integer values
        int value;
        err = nvs_read_int32(key, &value);
        if (err == ESP_OK) obj = newInteger(value);
    } else if (isClassNameEqual(c, "String") || isClassNameEqual(c, "ByteArray")) {
        // Handle string and byte array values
        boolean isString = isClassNameEqual(c, "String");
        
        // Get the length of the data first
        valLength = 0;
        err = isString ? nvs_read_string_length(key) : nvs_read_byte_array_length(key);
        
        if (valLength > 0) {
            // Allocate appropriate sized object
            obj = allocByte(valLength);
            char* strPtr = charPtr(obj);
            
            // Read the actual data
            err = isString ? nvs_read_string(key, strPtr) : nvs_read_byte_array(key, strPtr);
            setClass(obj, c);
            
            if (err != ESP_OK) {
                nvs_check_error(err, false, key);
                obj = nilobj;  
            }         
        } else {
            nvs_check_error(err, false, key);
        }
    }
    
    return obj;
}

/**
 * Read a 32-bit integer from NVS
 * 
 * @param key Name of the key to read
 * @param value Pointer to store the read value
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_read_int32(char *key, int32_t *value)
{
    esp_err_t err = nvs_get_i32(nvsHandle, key, value);
    err = nvs_check_error(err, false, key);
    return err;
}

/**
 * Initialize the NVS system
 * 
 * This function initializes the NVS flash system and opens the default "nvs" namespace
 * for reading and writing. If initialization fails due to no free pages or a new
 * version being found, it will erase the flash and retry.
 * 
 * @return trueobj if successful, falseobj if failed
 */
object nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    
    // Handle common initialization failures by erasing and retrying
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }

    // Open the default NVS namespace
    if (err == ESP_OK) {
        err = nvs_open("nvs", NVS_READWRITE, &nvsHandle);
        if (err != ESP_OK) {
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }
    }
    
    return err == ESP_OK ? trueobj : falseobj;
}

/**
 * Write a 32-bit integer to NVS
 * 
 * @param key Name of the key to write to
 * @param value The integer value to write
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_write_int32(char *key, int32_t value)
{
    esp_err_t err = nvs_set_i32(nvsHandle, key, value);
    err = nvs_check_error(err, true, key);
    return err;
}

/**
 * Get the length of a string stored in NVS
 * 
 * @param key Name of the key to read
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_read_string_length(char *key)
{
    esp_err_t err = nvs_get_str(nvsHandle, key, NULL, &valLength);
    err = nvs_check_error(err, false, key);
    return err;
}

/**
 * Read a string from NVS
 * 
 * @param key Name of the key to read
 * @param string Buffer to store the read string
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_read_string(char *key, char *string)
{
    esp_err_t err = nvs_get_str(nvsHandle, key, string, &valLength);
    err = nvs_check_error(err, false, key);
    return err;
}

/**
 * Write a string to NVS
 * 
 * @param key Name of the key to write to
 * @param value The string value to write
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_write_string(char *key, char *value)
{
    esp_err_t err = nvs_set_str(nvsHandle, key, value);
    err = nvs_check_error(err, true, key);
    return err;
}

/**
 * Get the length of a byte array stored in NVS
 * 
 * @param key Name of the key to read
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_read_byte_array_length(char *key)
{
    esp_err_t err = nvs_get_blob(nvsHandle, key, NULL, &valLength);
    err = nvs_check_error(err, false, key);
    return err;
}

/**
 * Read a byte array from NVS
 * 
 * @param key Name of the key to read
 * @param ba Buffer to store the read byte array
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_read_byte_array(char *key, uint8_t *ba)
{
    esp_err_t err = nvs_get_blob(nvsHandle, key, ba, &valLength);
    err = nvs_check_error(err, false, key);
    return err;
}

/**
 * Write a byte array to NVS
 * 
 * @param key Name of the key to write to
 * @param value Pointer to the byte array to write
 * @param length Length of the byte array
 * @return ESP_OK if successful, error code otherwise
 */
esp_err_t nvs_write_byte_array(char *key, void *value, size_t length)
{
    esp_err_t err = nvs_set_blob(nvsHandle, key, value, length);
    err = nvs_check_error(err, true, key);
    return err;
}

/**
 * Check NVS errors and handle them appropriately
 * 
 * This function checks for common NVS errors and takes appropriate actions:
 * - For successful operations, commits changes if requested
 * - For not found errors, reports the missing key
 * - For other errors, reports the error and key
 * 
 * @param err The error code to check
 * @param doCommit Whether to commit changes to NVS if successful
 * @param key The key being operated on (for error reporting)
 * @return The final error code (may be different if commit was requested)
 */
esp_err_t nvs_check_error(esp_err_t err, boolean doCommit, char *key)
{
    esp_err_t newErr = err;
    
    switch (err)
    {
        case ESP_OK:
            // If operation was successful and commit was requested, commit changes
            if (doCommit) newErr = nvs_commit(nvsHandle);
            break;
            
        case ESP_ERR_NVS_NOT_FOUND:
            // Key not found, report it
            printf("NVS: ESP_ERR_NVS_NOT_FOUND Key: %s!\n", key);
            break;
            
        default:
            // Other error, report it
            printf("Error (%s) reading! Key: %s\n", esp_err_to_name(err), key);
    }
    
    return newErr;
}

/**
 * Close the NVS handle
 * 
 * This function closes the NVS handle and releases associated resources.
 * It should be called when NVS operations are no longer needed.
 */
void nvsClose(void) 
{
    nvs_close(nvsHandle);
    nvsHandle = NULL;
}
