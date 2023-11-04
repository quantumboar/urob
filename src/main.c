/*
Copyright (c) 2023 Quantumboar <quantum@quantumboar.net>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "urob_http_server.h"
#include "urob_http_client_test.h"

#include "lwip/dns.h"

#define WIFI_SSID               "YOUR_SSID"
#define WIFI_PASSWORD           "YOUR_PWD"

#define WIFI_CONNECT_MAX_RETRIES  (2)
#define WIFI_CONNECTED_FLAG      BIT0
#define WIFI_ERROR_FLAG          BIT1

#define TAG "main"

typedef struct 
{
    EventGroupHandle_t wifi_group;
    int retries;
    esp_event_handler_instance_t wifi_event_any_id_instance;
    esp_event_handler_instance_t ip_event_sta_got_id_instance;
    bool keep_going;

    urob_http_server server;
    urob_http_client_test http_client_test;
} urob_main;

static void _urob_event_handler(
    void* arg, 
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data)
{
    urob_main * urob = (urob_main *) arg;

    if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED))
    {
        if (urob->retries < WIFI_CONNECT_MAX_RETRIES)
        {
            ESP_LOGW(TAG, "trying wifi connection again..");
            urob->retries++;
            esp_wifi_connect();
        } else
        {
            ESP_LOGE(TAG, "unable to connect to wifi");
            xEventGroupSetBits(urob->wifi_group, WIFI_ERROR_FLAG);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(urob->wifi_group, WIFI_CONNECTED_FLAG);
    }
}

static void _urob_init_wifi(urob_main * urob)
{
    // Init interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Init wifi
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));

    // Register event handlers for any_id and got_ip
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &_urob_event_handler,
        urob,
        &urob->wifi_event_any_id_instance));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &_urob_event_handler,
        urob,
        &urob->ip_event_sta_got_id_instance));

    // Configure and start wifi in STA
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection or error
    EventBits_t state = xEventGroupWaitBits(
        urob->wifi_group,
        WIFI_CONNECTED_FLAG | WIFI_ERROR_FLAG,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (state & WIFI_CONNECTED_FLAG) {
        ESP_LOGI(TAG, "connection successful");
        return;
    }

    if (state & WIFI_ERROR_FLAG) {
        ESP_LOGE(TAG, "error connecting");
        return;
    }

    ESP_LOGE(TAG, "unknown connection state: 0x%x", state);
}

void urob_init(urob_main * urob)
{
    * urob = (urob_main) {0};
    urob->wifi_group = xEventGroupCreate();
    urob->keep_going = true;

    _urob_init_wifi(urob);

    urob_http_server_init(&urob->server);
    urob_http_client_test_init(&urob->http_client_test);
}

void netconn_thread(void *arg)
{
    urob_main * urob = (urob_main *) arg;

    while(urob->keep_going)
    {
        urob_http_server_loop(&urob->server);
        urob_http_client_test_loop(&urob->http_client_test);
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    urob_main * urob = (urob_main *)malloc(sizeof(urob_main));
    urob_init(urob);

    // tskIDLE_PRIORITY won't trigger the watchdog
    xTaskCreate(netconn_thread, "http server", 2048, urob, tskIDLE_PRIORITY, NULL);
}