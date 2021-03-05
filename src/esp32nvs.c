/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021
*/

#include "nvs_flash.h"
#include "env.h"
#include "memory.h"
#include "names.h"

nvs_handle nvsHandle;
size_t valLength = 0;

object nvsPrim(int funcNumber, object *arguments)
{
    switch (funcNumber)
    {
        case 0:
            return nvs_init();
            break;

        case 1:
            return writeObject(charPtr(arguments[1]), arguments[2]);
            break;

        case 2:
            return readObject(charPtr(arguments[1]), arguments[2]);
            break;

        case 3:
            return eraseKey(charPtr(arguments[1]));
            break;

        default:
            break;
    }
    
    return nilobj;
}

object eraseKey(char *key)
{
    esp_err_t err = nvs_erase_key(nvsHandle, key);
    err = nvs_check_error(err, true);
    return err == ESP_OK ? trueobj : falseobj;
}

object writeObject(char *key, object obj)
{
    // Default error if object pass isn't a supported class
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

object readObject(char *key, object c)
{
    object obj = nilobj;
    esp_err_t err;
    if (isClassNameEqual(c, "Integer")) {
        int value;
        err = nvs_read_int32(key, &value);
        if (err = ESP_OK) obj = newInteger(value);
    } else if (isClassNameEqual(c, "String") || isClassNameEqual(c, "ByteArray")) {
        boolean isString = isClassNameEqual(c, "String");
        // len will include the string zero terminator if c is String
        valLength = 0;
        err = isString ? nvs_read_string_length(key) : nvs_read_byte_array_length(key);
        if (valLength > 0) {
            obj = allocByte(valLength);
            // Because of the test we know that c is the correct class
            char* strPtr = charPtr(obj);
            err = isString ? nvs_read_string(key, strPtr) : nvs_read_byte_array(key, strPtr);
            setClass(obj, c);
            if (err != ESP_OK) {
                nvs_check_error(err, false);
                obj = nilobj;  
            }         
        } else {
            nvs_check_error(err, false);
        }
    }
    return obj;
}

esp_err_t nvs_read_int32(char *key, int32_t value)
{
    esp_err_t err = nvs_get_i32(nvsHandle, key, &value);
    err = nvs_check_error(err, false);
    return err;
}

// Initialize NVS
object nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        err = nvs_open("nvs", NVS_READWRITE, &nvsHandle);
        if (err != ESP_OK) {
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }
    }
    return err == ESP_OK ? trueobj : falseobj;
}

esp_err_t nvs_write_int32(char *key, int32_t value)
{
    esp_err_t err = nvs_set_i32(nvsHandle, key, value);
    err = nvs_check_error(err, true);
    return err;
}

esp_err_t nvs_read_string_length(char *key)
{
    esp_err_t err = nvs_get_str(nvsHandle, key, NULL, &valLength);
    err = nvs_check_error(err, false);
    return err;
}

esp_err_t nvs_read_string(char *key, char *string)
{
    esp_err_t err = nvs_get_str(nvsHandle, key, string, &valLength);
    err = nvs_check_error(err, false);
    return err;
}

esp_err_t nvs_write_string(char *key, char *value)
{
    esp_err_t err = nvs_set_str(nvsHandle, key, value);
    err = nvs_check_error(err, true);
    return err;
}

esp_err_t nvs_read_byte_array_length(char *key)
{
    esp_err_t err = nvs_get_blob(nvsHandle, key, NULL, &valLength);
    err = nvs_check_error(err, false);
    return err;
}

esp_err_t nvs_read_byte_array(char *key, uint8_t *ba)
{
    esp_err_t err = nvs_get_blob(nvsHandle, key, ba, &valLength);
    err = nvs_check_error(err, false);
    return err;
}

esp_err_t nvs_write_byte_array(char *key, void *value, size_t length)
{
    esp_err_t err = nvs_set_blob(nvsHandle, key, value, length);
    err = nvs_check_error(err, true);
    return err;
}

esp_err_t nvs_check_error(esp_err_t err, boolean doCommit)
{
    esp_err_t newErr = err;
    switch (err)
    {
        case ESP_OK:
            if (doCommit) newErr = nvs_commit(nvsHandle);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("NVS: ESP_ERR_NVS_NOT_FOUND!\n");
            break;
        default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
    return newErr;
}

void nvsClose(void) 
{
    nvs_close(nvsHandle);
    nvsHandle = NULL;
}