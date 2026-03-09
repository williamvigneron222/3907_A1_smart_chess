#include <string.h>
#include <inttypes.h>

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"


#define ROLE_SENDER   0   // set to 1 on the sender ESP32


// Sender must set this to the RECEIVER MAC address printed on the receiver
static uint8_t PEER_MAC[6] = { 0x94,0xB9,0x7E,0xE5,0xB7,0xD4 };

static const char *TAG = "ESPNOW_TEST";

typedef struct __attribute__((packed)) {
    uint32_t counter;
} msg_t;

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "My STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(msg_t)) {
        ESP_LOGW(TAG, "RX len=%d (expected %d)", len, (int)sizeof(msg_t));
        return;
    }

    msg_t msg;
    memcpy(&msg, data, sizeof(msg));

    ESP_LOGI(TAG, "RX from %02X:%02X:%02X:%02X:%02X:%02X  counter=%" PRIu32,
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5],
             msg.counter);
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    ESP_LOGI(TAG, "TX status=%s", status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

static void espnow_init_common(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_LOGI(TAG, "ESP-NOW init done");
}

static void add_peer_or_die(const uint8_t peer_mac[6])
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0;     // same channel as current WiFi
    peer.encrypt = false; // no built-in encryption (we're just testing)

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGI(TAG, "Peer already exists");
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "Peer added");
}


void setup()
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    espnow_init_common();

#if ROLE_SENDER
    ESP_LOGI(TAG, "ROLE = SENDER");

    // IMPORTANT: set PEER_MAC first (receiver MAC)
    add_peer_or_die(PEER_MAC);

    uint32_t counter = 0;
    // while (1) {
    //     msg_t msg = { .counter = counter++ };
    //     esp_err_t err = esp_now_send(PEER_MAC, (uint8_t*)&msg, sizeof(msg));
    //     if (err != ESP_OK) {
    //         ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }

#else
    ESP_LOGI(TAG, "ROLE = RECEIVER (waiting...)");
    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
#endif
}

void loop()
{
    #if ROLE_SENDER
    // ESP_LOGI(TAG, "ROLE = SENDER");

    // IMPORTANT: set PEER_MAC first (receiver MAC)
    // add_peer_or_die(PEER_MAC);

    uint32_t counter = 0;
    while (1) {
        msg_t msg = { .counter = counter++ };
        esp_err_t err = esp_now_send(PEER_MAC, (uint8_t*)&msg, sizeof(msg));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

#else
    // ESP_LOGI(TAG, "ROLE = RECEIVER (waiting...)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}

// void app_main()
// {
//     ESP_ERROR_CHECK(nvs_flash_init());

//     wifi_init_sta();
//     espnow_init_common();

// #if ROLE_SENDER
//     ESP_LOGI(TAG, "ROLE = SENDER");

//     // IMPORTANT: set PEER_MAC first (receiver MAC)
//     add_peer_or_die(PEER_MAC);

//     uint32_t counter = 0;
//     while (1) {
//         msg_t msg = { .counter = counter++ };
//         esp_err_t err = esp_now_send(PEER_MAC, (uint8_t*)&msg, sizeof(msg));
//         if (err != ESP_OK) {
//             ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
//         }
//         vTaskDelay(pdMS_TO_TICKS(500));
//     }

// #else
//     ESP_LOGI(TAG, "ROLE = RECEIVER (waiting...)");
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// #endif
// }