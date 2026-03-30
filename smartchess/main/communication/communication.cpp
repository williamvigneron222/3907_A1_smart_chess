#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "packet.h"
#include "crypto_wrap.h"
#include "communication.h"
#include "Arduino.h"

static const char *TAG = "COMM";

// !! PUT RECEIVER MAC HERE ON SENDER AND SENDER MAC HERE ON RECEIVER !!
//MAC: 94:b9:7e:e5:b7:d4 sender
//MAC: MAC: 08:b6:1f:7e:d7:5c rec
//0x94,0xb9,0x7e,0xe5,0xb7,0xd4
//0x08,0xb6,0x1f,0x7e,0xd7,0x5c
static uint8_t PEER_MAC[6] = { 0x94,0xb9,0x7e,0xe5,0xb7,0xd4 };

static bool          g_comm_ready = false;
static uint16_t      g_seq        = 0;
static audio_rx_cb_t g_rx_cb      = nullptr;
static QueueHandle_t g_rx_queue   = nullptr;
static volatile bool g_tx_done    = true;

// ─── Wi-Fi ────────────────────────────────────────────────────────────────────

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

// ─── ESP-NOW callbacks ────────────────────────────────────────────────────────

static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len)
{
    if (len != sizeof(enc_packet_t)) {
        ESP_LOGW(TAG, "RX bad len=%d expected=%d", len, (int)sizeof(enc_packet_t));
        return;
    }

    if (g_rx_queue == nullptr) return;

    enc_packet_t pkt;
    std::memcpy(&pkt, data, sizeof(pkt));

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(g_rx_queue, &pkt, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info,
                           esp_now_send_status_t status)
{
    (void)tx_info;
    g_tx_done = true;
    ESP_LOGI(TAG, "TX %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ─── RX processing task ───────────────────────────────────────────────────────

static void rx_task(void *arg)
{
    enc_packet_t pkt;

    while (true) {
        // Block until a packet arrives in the queue
        if (xQueueReceive(g_rx_queue, &pkt, portMAX_DELAY) != pdTRUE) continue;

        if (pkt.ct_bytes > PAYLOAD_MAX_BYTES ||
            pkt.pt_samples > PAYLOAD_MAX_SAMPLES) {
            ESP_LOGW(TAG, "Invalid packet sizes");
            continue;
        }

        int16_t samples[PAYLOAD_MAX_SAMPLES] = {};
        size_t  recovered_samples = 0;

        uint64_t nonce[2], tag[2];
        std::memcpy(nonce, pkt.nonce, sizeof(nonce));
        std::memcpy(tag,   pkt.tag,   sizeof(tag));

        bool ok = ascon_decrypt_audio(
            pkt.payload,  pkt.ct_bytes,
            nonce,        tag,
            samples,      PAYLOAD_MAX_SAMPLES,
            &recovered_samples, pkt.pt_samples
        );

        if (!ok) {
            ESP_LOGW(TAG, "Decrypt failed seq=%u", pkt.seq);
            continue;
        }

        ESP_LOGI(TAG, "RX OK seq=%u samples=%u",
                 pkt.seq, (unsigned)recovered_samples);

        if (g_rx_cb) {
            g_rx_cb(samples, recovered_samples);
        }
    }
}

// ─── Init helpers ─────────────────────────────────────────────────────────────

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

// ─── Boot self-test ───────────────────────────────────────────────────────────

static void local_crypto_test(void)
{
    int16_t test_samples[8] = { 100, -200, 300, -400, 500, -600, 700, -800 };

    enc_packet_t pkt = {};
    pkt.seq        = 1;
    pkt.pt_samples = 8;

    size_t ct_bytes = 0;
    bool enc_ok = ascon_encrypt_audio(
        test_samples, pkt.pt_samples, pkt.seq,
        pkt.nonce, pkt.tag,
        pkt.payload, PAYLOAD_MAX_BYTES,
        &ct_bytes
    );

    if (!enc_ok) { ESP_LOGE(TAG, "LOCAL TEST: encrypt failed"); return; }
    pkt.ct_bytes = static_cast<uint16_t>(ct_bytes);

    int16_t recovered[PAYLOAD_MAX_SAMPLES] = {};
    size_t  recovered_count = 0;

    uint64_t nonce[2], tag[2];
    std::memcpy(nonce, pkt.nonce, sizeof(nonce));
    std::memcpy(tag,   pkt.tag,   sizeof(tag));

    bool dec_ok = ascon_decrypt_audio(
        pkt.payload,  pkt.ct_bytes,
        nonce,        tag,
        recovered,    PAYLOAD_MAX_SAMPLES,
        &recovered_count, pkt.pt_samples
    );

    if (!dec_ok) { ESP_LOGE(TAG, "LOCAL TEST: decrypt failed"); return; }

    bool match = (std::memcmp(test_samples, recovered,
                              pkt.pt_samples * sizeof(int16_t)) == 0);
    ESP_LOGI(TAG, "LOCAL TEST %s: seq=%u samples=%u",
             match ? "OK" : "DATA MISMATCH",
             pkt.seq, (unsigned)recovered_count);
}

// ─── Public API ───────────────────────────────────────────────────────────────

void communication_setup(audio_rx_cb_t rx_callback)
{
    ESP_LOGI(TAG, "communication_setup()");

    g_rx_cb = rx_callback;

    // Create receive queue — holds up to 8 packets
    g_rx_queue = xQueueCreate(8, sizeof(enc_packet_t));
    if (g_rx_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return;
    }

    // Create dedicated RX processing task on core 1
    // Firmware runs on core 0, this runs on core 1 — no conflict
    xTaskCreatePinnedToCore(
        rx_task,        // function
        "rx_task",      // name
        4096,           // stack size
        nullptr,        // args
        5,              // priority
        nullptr,        // handle
        1               // core 1
    );

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK)
        ESP_LOGW(TAG, "nvs_flash_init: %s", esp_err_to_name(ret));

    wifi_init_sta();
    espnow_init_common();
    local_crypto_test();
    add_peer_or_die(PEER_MAC);

    g_comm_ready = true;
}

void communication_send(const int16_t *samples, size_t num_samples)
{
    if (!g_comm_ready || !samples || num_samples == 0) return;
    if (!g_tx_done) return;

    if (num_samples > PAYLOAD_MAX_SAMPLES) {
        ESP_LOGW(TAG, "Block too large: %u (max %d)",
                 (unsigned)num_samples, PAYLOAD_MAX_SAMPLES);
        return;
    }

    enc_packet_t pkt = {};
    pkt.seq        = g_seq++;
    pkt.pt_samples = static_cast<uint16_t>(num_samples);

    size_t ct_bytes = 0;
    uint64_t nonce[2] = {}, tag[2] = {};

    bool ok = ascon_encrypt_audio(
        samples, num_samples, pkt.seq,
        nonce, tag,
        pkt.payload, PAYLOAD_MAX_BYTES,
        &ct_bytes
    );

    if (!ok) { ESP_LOGW(TAG, "Encrypt failed"); return; }

    std::memcpy(pkt.nonce, nonce, sizeof(nonce));
    std::memcpy(pkt.tag,   tag,   sizeof(tag));
    pkt.ct_bytes = static_cast<uint16_t>(ct_bytes);

    g_tx_done = false;

    esp_err_t err = esp_now_send(PEER_MAC,
                                 reinterpret_cast<uint8_t*>(&pkt),
                                 sizeof(pkt));
    if (err != ESP_OK) {
        g_tx_done = true;
        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
    }
}

// communication_loop is now empty — rx_task handles everything
void communication_loop()
{
    vTaskDelay(1);  // just feed the watchdog
}
