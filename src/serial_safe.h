#pragma once
// ---------------------------------------------------------------------------
// serial_safe.h — mutex-protected Serial output
//
// Two FreeRTOS tasks write to USB-CDC concurrently in this project:
//   * loopTask    (Core 0) — main loop's Serial.printf for [Sensors] etc.
//   * lvgl_render (Core 1) — LVGL's log callback (warnings, errors)
//
// Without serialization, the second writer's first 1–4 characters get
// dropped because both tasks fight for the same TX buffer pointer.
// Every Serial.printf in the project should go through these helpers.
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>

extern SemaphoreHandle_t g_serial_mutex; // created in setup()

inline void serial_safe_begin() {
    if (g_serial_mutex == nullptr) {
        g_serial_mutex = xSemaphoreCreateMutex();
    }
}

inline void serial_safe_printf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (g_serial_mutex && xSemaphoreTake(g_serial_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.write((const uint8_t*)buf, (size_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1));
        xSemaphoreGive(g_serial_mutex);
    } else {
        Serial.write((const uint8_t*)buf, (size_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1));
    }
}

inline void serial_safe_println(const char* s) {
    if (g_serial_mutex && xSemaphoreTake(g_serial_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println(s);
        xSemaphoreGive(g_serial_mutex);
    } else {
        Serial.println(s);
    }
}

inline void serial_safe_write(const char* s) {
    if (g_serial_mutex && xSemaphoreTake(g_serial_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.print(s);
        xSemaphoreGive(g_serial_mutex);
    } else {
        Serial.print(s);
    }
}
