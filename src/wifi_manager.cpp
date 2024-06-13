#include <wifi_manager.hpp>
#ifdef ARDUINO
using namespace arduino;
#include <WiFi.h>
#else
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_event.h>
#include <lwip/inet.h>
#include <lwip/ip4_addr.h>
#include <lwip/dns.h>
#include <nvs_flash.h>
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
using namespace esp_idf;
#endif
#ifdef ARDUINO
wifi_manager::wifi_manager() : m_state(0) {

}
wifi_manager_state wifi_manager::state() const {
    if(m_state==0) {
        return wifi_manager_state::disconnected;
    } else {
        switch(WiFi.status()) {
            
            case WL_CONNECTED:
                return wifi_manager_state::connected;
            case WL_DISCONNECTED:
                if(m_state==1) {
                    return wifi_manager_state::connecting;
                }
                return wifi_manager_state::disconnected;
            case WL_CONNECTION_LOST:
            case WL_SCAN_COMPLETED:
                return wifi_manager_state::disconnected;
            case WL_CONNECT_FAILED:
            case WL_NO_SHIELD:
            case WL_NO_SSID_AVAIL:
                //printf("WiFi Error: %d",(int)WiFi.status());
                return wifi_manager_state::error;
            case WL_IDLE_STATUS:
                return wifi_manager_state::connecting;
        }
    }
}
void wifi_manager::connect(const char* ssid, const char* pass) {
    if(m_state==1) {
        disconnect();
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid,pass);
    m_state = 1;
}
void wifi_manager::disconnect(bool radio_off) {
    if(m_state==1) {
        WiFi.disconnect(radio_off,false);
        m_state = 0;
    }
}
#else
wifi_manager::wifi_manager() : m_wifi_event_group(nullptr),m_retry_count(0),m_state(0) {

}
void wifi_manager::event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    
    wifi_manager& ths = *(wifi_manager*)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (ths.m_retry_count < 3) {
            esp_wifi_connect();
            ++ths.m_retry_count;
        } else {
            xEventGroupSetBits(ths.m_wifi_event_group, WIFI_FAIL_BIT);
        }
        //ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ths.m_retry_count = 0;
        xEventGroupSetBits(ths.m_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
wifi_manager_state wifi_manager::state() const {
    switch(m_state) {
        case 0:
        case 1:
            return wifi_manager_state::disconnected;
        default:
            break;
    }
    auto bits = xEventGroupGetBits(m_wifi_event_group)&(WIFI_CONNECTED_BIT|WIFI_FAIL_BIT);
    if(bits==WIFI_CONNECTED_BIT) {
        return wifi_manager_state::connected;
    }
    if(bits==WIFI_FAIL_BIT) {
        return wifi_manager_state::error;
    }
    return wifi_manager_state::connecting;
}
void wifi_manager::connect(const char* ssid, const char* pass) {
    if(m_state==0) {
        nvs_flash_init();
        m_wifi_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            this,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            this,
                                                            &instance_got_ip));
        m_state = 1;
    }
    if(m_state<2) {
        wifi_config_t wifi_config;
        memset(&wifi_config,0,sizeof(wifi_config));
        memcpy(wifi_config.sta.ssid,ssid,strlen(ssid)+1);
        memcpy(wifi_config.sta.password,pass,strlen(pass)+1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        wifi_config.sta.sae_pwe_h2e = wifi_sae_pwe_method_t::WPA3_SAE_PWE_BOTH;
        wifi_config.sta.sae_h2e_identifier[0]=0;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );
        m_state = 2;
    }
}

void wifi_manager::disconnect(bool radio_off) {
    if(m_state>1)  {
        //puts("Disconnecting");
        esp_wifi_disconnect();
        if(radio_off) {
            esp_wifi_stop();
        }
        xEventGroupClearBits(m_wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        m_state = 1;
    }
}

#endif
