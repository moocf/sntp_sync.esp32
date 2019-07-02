#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <lwip/sys.h>
#include <esp_event.h>
#include <esp_sntp.h>
#include "macros.h"


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
  time_t now = 0;
  struct tm timeinfo = {0};
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_set_time_sync_notification_cb(on_sntp_sync);
  sntp_init();
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
    printf(": waiting to sync with SNTP ...\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  time(&now);
  localtime_r(&now, &timeinfo);
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
    ERETV( sntp_print() );
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
  ESP_ERROR_CHECK( nvs_init() );
  ESP_ERROR_CHECK( wifi_init() );
}
