/* LwIP SNTP example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "macros.h"


static const char *TAG = "example";


static void on_sntp_sync(struct timeval *tv) {
  printf("@ SNTP sync\n");
}


static esp_err_t nvs_init() {
  printf("- Init NVS\n");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ERET( nvs_flash_erase() );
    ERET( nvs_flash_init() );
  }
  ERET( ret );
  return ESP_OK;
}


static esp_err_t sntp_start() {
  printf("- Start SNTP\n");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_set_time_sync_notification_cb(on_sntp_sync);
  sntp_init();
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
    printf(": waiting to sync with SNTP ...\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  printf(": SNTP complete\n");
  return ESP_OK;
}


static esp_err_t sntp_print() {
  time_t now = 0;
  struct tm timeinfo = {0};
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  char strftime_buf[64];
  setenv("TZ", "CST-8", 1);;
  tzset();
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  printf("the current date/time in shanghai is: %s\n", strftime_buf);
  return ESP_OK;
}


static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data) {
  switch (id) {
    case WIFI_EVENT_STA_START:
    printf("@ WiFi station mode started\n");
    ERETV( esp_wifi_connect() );
    break;
    case WIFI_EVENT_STA_CONNECTED: {
    wifi_event_sta_connected_t *d = (wifi_event_sta_connected_t*) data;
    printf("@ Connected to WiFi AP '%s'\n", d->ssid);
    } break;
    case WIFI_EVENT_STA_DISCONNECTED: {
    wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t*) data;
    printf(" @ Disconnected from WiFi AP '%s'\n", d->ssid);
    ERETV( esp_wifi_connect() );
    } break;
  }
}


static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data) {
  switch (id) {
    case IP_EVENT_STA_GOT_IP: {
    ip_event_got_ip_t *d = (ip_event_got_ip_t*) data;
    printf("@ Got IP from WiFi AP\n");
    printf(": ip=%s\n", ip4addr_ntoa(&d->ip_info.ip));
    ERETV( sntp_start() );
    time_t now = time(NULL);
    printf(": Calendar time = %ld\n", now);
    } break;
  }
}


static esp_err_t wifi_init() {
  printf("- Initialize TCP/IP adapter\n");
  tcpip_adapter_init();
  printf("- Create default event loop\n");
  ERET( esp_event_loop_create_default() );
  printf("- Initialize WiFi with default config\n");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ERET( esp_wifi_init(&cfg) );
  printf("- Register WiFi event handler\n");
  ERET( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, NULL) );
  printf("- Register IP event handler\n");
  ERET( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip, NULL) );
  printf("- Set WiFi mode as station\n");
  ERET( esp_wifi_set_mode(WIFI_MODE_STA) );
  printf("- Set WiFi configuration\n");
  wifi_config_t wifi_config = {.sta = {
    .ssid = "belkin.a58",
    .password = "ceca966f",
    .scan_method = WIFI_ALL_CHANNEL_SCAN,
    .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
    .threshold.rssi = -127,
    .threshold.authmode = WIFI_AUTH_OPEN,
  }};
  ERET( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  printf("- Start WiFi\n");
  ERET( esp_wifi_start() );
  printf("- Connect to set AP\n");
  ERET( esp_wifi_connect() );
  return ESP_OK;
}


void app_main() {
  time_t now;
  time(&now);
  printf(": Calendar time = %ld\n", now);
  ERETV( nvs_init() );
  ERETV( wifi_init() );
}




#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL);
   ESP_LOGI(TAG, "Time is synchronized from custom code");
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

void app_main_old() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        // obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    else {
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(TAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
            int64_t error_time = cpu_time + 500 * 1000L;
            struct timeval tv_error = { .tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L };
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in New York is: %s", strftime_buf);

    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                        outdelta.tv_sec,
                        outdelta.tv_usec/1000,
                        outdelta.tv_usec%1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    const int deep_sleep_sec = 10;
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", deep_sleep_sec);
    esp_deep_sleep(1000000LL * deep_sleep_sec);
}
