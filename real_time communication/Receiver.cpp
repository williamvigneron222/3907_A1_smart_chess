#include <WiFi.h>
#include <esp_now.h>

void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.print("Received bytes: ");
  Serial.println(len);

  Serial.print("Message: ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)incomingData[i]);
  }
  Serial.println();
  Serial.println("------------------");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(onRecv);

  Serial.println("Receiver Ready.");
}

void loop() {
  delay(1000);
}