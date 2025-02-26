#pragma once
#include <iostream>
#include <cstdarg>
#include <cstdio>

// Buffer size for formatting
#define LOG_BUFFER_SIZE 256

// Helper function to format and print logs
inline void log_print(const char* level, const char* tag, const char* fmt, va_list args) {
    char buffer[LOG_BUFFER_SIZE];
    vsnprintf(buffer, LOG_BUFFER_SIZE, fmt, args);
    std::cout << "[" << level << "][" << tag << "] " << buffer << std::endl;
}

#define ESP_LOGI(tag, fmt, ...) do { \
    printf("[INFO][%s] ", tag); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

#define ESP_LOGW(tag, fmt, ...) do { \
    printf("[WARN][%s] ", tag); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

#define ESP_LOGE(tag, fmt, ...) do { \
    printf("[ERROR][%s] ", tag); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

#define ESP_LOGD(tag, fmt, ...) do { \
    printf("[DEBUG][%s] ", tag); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

#define ESP_LOGV(tag, fmt, ...) do { \
    printf("[VERBOSE][%s] ", tag); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\n"); \
} while(0)

extern "C" {
    typedef enum {
        ESP_LOG_NONE,
        ESP_LOG_ERROR,
        ESP_LOG_WARN,
        ESP_LOG_INFO,
        ESP_LOG_DEBUG,
        ESP_LOG_VERBOSE
    } esp_log_level_t;

    inline void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);

        switch (level) {
            case ESP_LOG_ERROR:
                log_print("ERROR", tag, format, args);
                break;
            case ESP_LOG_WARN:
                log_print("WARN", tag, format, args);
                break;
            case ESP_LOG_INFO:
                log_print("INFO", tag, format, args);
                break;
            case ESP_LOG_DEBUG:
                log_print("DEBUG", tag, format, args);
                break;
            case ESP_LOG_VERBOSE:
                log_print("VERBOSE", tag, format, args);
                break;
            default:
                break;
        }

        va_end(args);
    }
}

