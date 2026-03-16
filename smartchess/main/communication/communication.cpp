#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "packet.h"
#include "crypto_wrap.h"
#include "communication.h"

static const char *TAG = "ENC_TEST";

#define ROLE_SENDER 0

static uint8_t PEER_MAC[6] = { 0x00,0x00,0x00,0x00,0x00,0x00 };

static bool g_comm_ready = false;
static uint16_t g_seq = 0;

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    ESP_LOGI(TAG, "My STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(enc_packet_t)) {
        ESP_LOGW(TAG, "RX len=%d (expected %d)", len, (int)sizeof(enc_packet_t));
        return;
    }

    enc_packet_t pkt;
    std::memcpy(&pkt, data, sizeof(pkt));

    if (pkt.ct_len > PAYLOAD_MAX || pkt.pt_len > PAYLOAD_MAX) {
        ESP_LOGW(TAG, "Invalid packet lengths");
        return;
    }

    uint8_t plaintext[PAYLOAD_MAX] = {0};
    size_t out_len = 0;

    bool ok = ascon_decrypt_bytes(pkt.payload, pkt.ct_len,
                                  pkt.nonce, pkt.tag,
                                  plaintext, sizeof(plaintext),
                                  &out_len, pkt.pt_len);

    if (!ok) {
        ESP_LOGW(TAG, "Decrypt failed / tag mismatch");
        return;
    }

    ESP_LOGI(TAG,
             "RX from %02X:%02X:%02X:%02X:%02X:%02X seq=%u msg='%.*s'",
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5],
             pkt.seq, (int)out_len, reinterpret_cast<char*>(plaintext));
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    ESP_LOGI(TAG, "TX status=%s",
             status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

static void espnow_init_common(void)
{
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);
    esp_now_register_send_cb(espnow_send_cb);
    ESP_LOGI(TAG, "ESP-NOW init done");
}

static void add_peer_or_die(const uint8_t peer_mac[6])
{
    esp_now_peer_info_t peer = {};
    std::memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGI(TAG, "Peer already exists");
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "Peer added");
}

static void local_crypto_test(void)
{
    const char *msg = "HELLO";
    enc_packet_t pkt = {};
    pkt.seq = 1;
    pkt.pt_len = static_cast<uint16_t>(std::strlen(msg));

    size_t ct_len = 0;
    bool enc_ok = ascon_encrypt_bytes(
        reinterpret_cast<const uint8_t*>(msg),
        pkt.pt_len,
        pkt.seq,
        pkt.nonce,
        pkt.tag,
        pkt.payload,
        PAYLOAD_MAX,
        &ct_len
    );

    if (!enc_ok) {
        ESP_LOGE(TAG, "LOCAL TEST: encrypt failed");
        return;
    }

    pkt.ct_len = static_cast<uint16_t>(ct_len);

    uint8_t plaintext[PAYLOAD_MAX] = {0};
    size_t out_len = 0;

    bool dec_ok = ascon_decrypt_bytes(
        pkt.payload,
        pkt.ct_len,
        pkt.nonce,
        pkt.tag,
        plaintext,
        sizeof(plaintext),
        &out_len,
        pkt.pt_len
    );

    if (!dec_ok) {
        ESP_LOGE(TAG, "LOCAL TEST: decrypt failed");
        return;
    }

    ESP_LOGI(TAG, "LOCAL TEST OK: seq=%u msg='%.*s'",
             pkt.seq, (int)out_len, reinterpret_cast<char*>(plaintext));
}

void communication_setup()
{
    ESP_LOGI(TAG, "communication_setup()");

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
    }

    wifi_init_sta();
    espnow_init_common();
    local_crypto_test();

#if ROLE_SENDER
    add_peer_or_die(PEER_MAC);
#endif

    g_comm_ready = true;
}

void communication_loop()
{
    if (!g_comm_ready) {
        return;
    }

#if ROLE_SENDER
    static uint32_t last_send_ms = 0;
    uint32_t now = millis();

    if (now - last_send_ms >= 500) {
        last_send_ms = now;

        const char *msg = "HELLO";

        enc_packet_t pkt = {};
        pkt.seq = g_seq++;
        pkt.pt_len = static_cast<uint16_t>(std::strlen(msg));

        size_t ct_len = 0;
        bool ok = ascon_encrypt_bytes(
            reinterpret_cast<const uint8_t*>(msg),
            pkt.pt_len,
            pkt.seq,
            pkt.nonce,
            pkt.tag,
            pkt.payload,
            PAYLOAD_MAX,
            &ct_len
        );

        if (!ok) {
            ESP_LOGW(TAG, "Encrypt failed");
            return;
        }

        pkt.ct_len = static_cast<uint16_t>(ct_len);

        esp_err_t err = esp_now_send(PEER_MAC, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
        }
    }
#endif
}