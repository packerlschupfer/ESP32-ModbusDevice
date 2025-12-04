#ifndef MODBUS_DEVICE_LOGGING_H
#define MODBUS_DEVICE_LOGGING_H

#define MODBUSD_LOG_TAG "ModbusD"

// Define log levels based on debug flag
#ifdef MODBUSDEVICE_DEBUG
    // Debug mode: Show all levels
    #define MODBUSD_LOG_LEVEL_E ESP_LOG_ERROR
    #define MODBUSD_LOG_LEVEL_W ESP_LOG_WARN
    #define MODBUSD_LOG_LEVEL_I ESP_LOG_INFO
    #define MODBUSD_LOG_LEVEL_D ESP_LOG_DEBUG
    #define MODBUSD_LOG_LEVEL_V ESP_LOG_VERBOSE
#else
    // Release mode: Only Error, Warn, Info
    #define MODBUSD_LOG_LEVEL_E ESP_LOG_ERROR
    #define MODBUSD_LOG_LEVEL_W ESP_LOG_WARN
    #define MODBUSD_LOG_LEVEL_I ESP_LOG_INFO
    #define MODBUSD_LOG_LEVEL_D ESP_LOG_NONE  // Suppress
    #define MODBUSD_LOG_LEVEL_V ESP_LOG_NONE  // Suppress
#endif

// Route to custom logger or ESP-IDF
#ifdef USE_CUSTOM_LOGGER
    #include <LogInterface.h>
    #define MODBUSD_LOG_E(...) LOG_WRITE(MODBUSD_LOG_LEVEL_E, MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_W(...) LOG_WRITE(MODBUSD_LOG_LEVEL_W, MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_I(...) LOG_WRITE(MODBUSD_LOG_LEVEL_I, MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_D(...) LOG_WRITE(MODBUSD_LOG_LEVEL_D, MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_V(...) LOG_WRITE(MODBUSD_LOG_LEVEL_V, MODBUSD_LOG_TAG, __VA_ARGS__)
#else
    // ESP-IDF logging with compile-time suppression
    #include <esp_log.h>
    #define MODBUSD_LOG_E(...) ESP_LOGE(MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_W(...) ESP_LOGW(MODBUSD_LOG_TAG, __VA_ARGS__)
    #define MODBUSD_LOG_I(...) ESP_LOGI(MODBUSD_LOG_TAG, __VA_ARGS__)
    #ifdef MODBUSDEVICE_DEBUG
        #define MODBUSD_LOG_D(...) ESP_LOGD(MODBUSD_LOG_TAG, __VA_ARGS__)
        #define MODBUSD_LOG_V(...) ESP_LOGV(MODBUSD_LOG_TAG, __VA_ARGS__)
    #else
        #define MODBUSD_LOG_D(...) ((void)0)
        #define MODBUSD_LOG_V(...) ((void)0)
    #endif
#endif

// Optional: Protocol-level debugging for Modbus
#ifdef MODBUSDEVICE_DEBUG_PROTOCOL
    #define MODBUSD_LOG_PROTO(...) MODBUSD_LOG_D("PROTO: " __VA_ARGS__)
#else
    #define MODBUSD_LOG_PROTO(...) ((void)0)
#endif

// Optional: Timing debug for performance analysis
#ifdef MODBUSDEVICE_DEBUG_TIMING
    #define MODBUSD_TIME_START() unsigned long _modbus_start = millis()
    #define MODBUSD_TIME_END(msg) MODBUSD_LOG_D("Timing: %s took %lu ms", msg, millis() - _modbus_start)
#else
    #define MODBUSD_TIME_START() ((void)0)
    #define MODBUSD_TIME_END(msg) ((void)0)
#endif

// Optional: Buffer dumps for protocol debugging
#ifdef MODBUSDEVICE_DEBUG_BUFFER
    #define MODBUSD_DUMP_BUFFER(msg, buf, len) do { \
        MODBUSD_LOG_D("%s (%d bytes):", msg, len); \
        for (int i = 0; i < len; i++) { \
            MODBUSD_LOG_D("  [%02d] = 0x%02X", i, buf[i]); \
        } \
    } while(0)
#else
    #define MODBUSD_DUMP_BUFFER(msg, buf, len) ((void)0)
#endif

#endif // MODBUS_DEVICE_LOGGING_H