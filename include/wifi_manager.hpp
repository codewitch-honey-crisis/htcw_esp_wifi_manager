#pragma once
#if __has_include(<Arduino.h>)
#include <Arduino.h>
namespace arduino {
#else
#include <stdint.h>
#include <stddef.h>
#include <esp_wifi.h>
#include <lwip/dns.h>
namespace esp_idf {
#endif

    enum struct wifi_manager_state {
        error = -1,
        disconnected = 0,
        connecting = 1,
        connected = 2
    };
    class wifi_manager {
#ifdef ARDUINO
        int m_state;
#else
        EventGroupHandle_t m_wifi_event_group;
        int m_retry_count;
        int m_state;
        static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
#endif
        wifi_manager(const wifi_manager& rhs)=delete;
        wifi_manager& operator=(const wifi_manager& rhs)=delete;
        wifi_manager(wifi_manager&& rhs)=delete;
        wifi_manager& operator=(wifi_manager&& rhs)=delete;
    public:    
        wifi_manager();
        void connect(const char* ssid, const char* pass);
        void disconnect(bool radio_off = false);
        wifi_manager_state state() const;
    };
}
#ifndef ESP_PLATFORM
#error "This library requires an ESP32 line MCU"
#endif
