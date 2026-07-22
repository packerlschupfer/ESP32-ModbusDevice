#ifndef MOCK_LOGGER_H
#define MOCK_LOGGER_H

#include <stdio.h>
#include <stdarg.h>

// Mock ESP log levels
typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

// Mock Logger class for testing
class Logger {
public:
    void log(esp_log_level_t level, const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        
        if (level <= ESP_LOG_INFO) {  // Only print errors, warnings, and info in tests
            printf("[%s] ", tag);
            vprintf(format, args);
            printf("\n");
        }
        
        va_end(args);
    }
    
    void logNnL(esp_log_level_t level, const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        
        if (level <= ESP_LOG_INFO) {
            printf("[%s] ", tag);
            vprintf(format, args);
        }
        
        va_end(args);
    }
    
    void logInL(esp_log_level_t level, const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        
        if (level <= ESP_LOG_INFO) {
            vprintf(format, args);
            printf("\n");
        }
        
        va_end(args);
    }
};

// Global logger instance
extern Logger logger;

#endif // MOCK_LOGGER_H