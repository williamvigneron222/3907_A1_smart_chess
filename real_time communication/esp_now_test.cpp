/*
  ESP-NOW TEST 
*/

#include <esp_now.h>
#include <WiFi.h>
#include <driver/dac.h>
#include <driver/adc.h>

#define NODE_TYPE_SLAVE /* Comment out for master mode. */

#define LED_PIN  (2)

#define WIFI_CHANNEL (1)
esp_now_peer_info_t peer;
bool isPaired = false;

void InitESPNow(void)
{
  WiFi.disconnect();
  if (esp_now_init()!=ESP_OK) 
  {
    Serial.println("ESP-NOW init failed");
    ESP.restart();
  }
}

bool esp_now_check_result(esp_err_t result)
{
  if (result==ESP_OK)
  {
    Serial.println("ok");
    return true;
  }
  else if (result==ESP_ERR_ESPNOW_NOT_INIT)
  {
    // How did we get so far!!
    Serial.println("ESP-NOW not initialized");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_ARG)
  {
    Serial.println("invalid argument");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_NO_MEM)
  {
    Serial.println("out of memory");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_FULL)
  {
    Serial.println("peer list full");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_NOT_FOUND)
  {
    Serial.println("peer not found");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_INTERNAL)
  {
    Serial.println("internal error");
    return false;
  }
  else if (result==ESP_ERR_ESPNOW_EXIST)
  {
    Serial.println("peer exists");
    return true;
  }
  else if (result==ESP_ERR_ESPNOW_IF)
  {
    Serial.println("interface error");
    return false;
  }
  else
  {
    // 0x306c = 0x3000 + 0x6c = 0x3000 + 108
    Serial.print("unhandled error ");
    Serial.println(result);
    return false;
  }
}

#define DELETE_BEFORE_PAIR (0)

#ifdef NODE_TYPE_SLAVE

bool ping = false;

void configDeviceAP(void)
{
  const char *SSID = "Slave_1";
  bool result = WiFi.softAP(SSID,"Slave_1_Password",WIFI_CHANNEL,0);
  if (!result)
  {
    Serial.println("AP config failed.");
  }
  else
  {
    Serial.println("My SSID: " + String(SSID));
    Serial.print("My channel: ");
    Serial.println(WiFi.channel());
  }
}
#else /* NODE_TYPE_SLAVE */

#include <esp_wifi.h> // only for esp_wifi_set_channel()

#define PRINT_SCAN_RESULTS (0)

bool ScanForPeer(void)
{
  // Scearch for peers in AP mode.
  // Scan only on one channel
  int16_t scanResults = WiFi.scanNetworks(false,false,false,300,WIFI_CHANNEL);
  // reset on each scan
  bool peerFound = 0;
  memset(&peer,0,sizeof(peer));

  Serial.println("");
  if (scanResults==0)
  {
    Serial.println("No WiFi devices in AP mode found");
  }
  else
  {
    Serial.print("Found ");
    Serial.print(scanResults);
    Serial.println(" devices ");
    for (int i=0; i<scanResults; i++)
    {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINT_SCAN_RESULTS)
      {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.println(")");
      }
      delay(10);
      
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave")==0)
      {
        // SSID of interest
        Serial.println("Found a slave.");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" [");
        Serial.print(BSSIDstr);
        Serial.print("]");
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
        
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if (6==sscanf(BSSIDstr.c_str(),"%x:%x:%x:%x:%x:%x",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5])) 
        {
          for (int j=0; j<6; j++)
          {
            peer.peer_addr[j] = (uint8_t) mac[j];
          }
        }

        peer.channel = WIFI_CHANNEL; // pick a channel
        peer.encrypt = 0; // no encryption

        peerFound = 1;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }

  if (!peerFound)
  {
    Serial.println("Peer not found, trying again.");
  }

  // Clean up memory.
  WiFi.scanDelete();

  return peerFound;
}

#endif /* NODE_TYPE_SLAVE */

// Check if the peer is already paired with the master.
// If not, pair it.
bool managePeer(void)
{
  if (peer.channel==WIFI_CHANNEL)
  {
    if (DELETE_BEFORE_PAIR)
    {
      deletePeer();
    }

    Serial.print("Peer status: ");
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(peer.peer_addr);
    if (exists)
    {
      // Slave already paired.
      Serial.println("already paired");
      return true;
    }
    else
    {
      // Peer not paired, attempting to pair
      esp_err_t result = esp_now_add_peer(&peer);
      return esp_now_check_result(result);
    }
  }
  else
  {
    // No peer found to process.
    Serial.println("No peer found to process");
  }
  return false;
}

bool deletePeer(void)
{
  esp_err_t result = esp_now_del_peer(peer.peer_addr);
  Serial.print("Peer delete status: ");
  return esp_now_check_result(result);
}

void rx_callback(const uint8_t *p_mac, const uint8_t *p_data, int data_size)
{
#ifdef NODE_TYPE_SLAVE
  ping = true;
#endif
}

void tx_callback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
}

bool sendData(uint8_t *p_data, uint8_t data_size)
{
  const uint8_t *peer_addr = peer.peer_addr;
  esp_err_t result = esp_now_send(peer_addr,p_data,data_size);
  return result==ESP_OK;
}

void setup(void)
{
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LOW);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("ESP-NOW range test");
  
#ifdef NODE_TYPE_SLAVE

  Serial.println("Slave");
  // Set device in AP mode
  WiFi.mode(WIFI_AP);
  // Configure device AP mode
  configDeviceAP();
  // This is the MAC address of the peer in AP Mode
  Serial.print("My mac: ");
  Serial.println(WiFi.softAPmacAddress());
  memset(&peer,0,sizeof(peer));
  peer.channel = WIFI_CHANNEL;
  peer.ifidx = (wifi_interface_t)ESP_IF_WIFI_AP;
#else /* NODE_TYPE_SLAVE */

  Serial.println("Master");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(WIFI_CHANNEL,WIFI_SECOND_CHAN_NONE);
  Serial.print("My channel: ");
  Serial.println(WiFi.channel());
  // This is the MAC address of the master in Station Mode
  Serial.print("My mac: ");
  Serial.println(WiFi.macAddress());
  
#endif /* NODE_TYPE_SLAVE */
  
  // Init ESP-NOW.
  InitESPNow();

  // Register send & receive callbacks.
  esp_now_register_recv_cb(rx_callback);
  esp_now_register_send_cb(tx_callback);

  Serial.println("Running...");
}

void loop(void)
{
#ifndef NODE_TYPE_SLAVE
  if (isPaired==false)
  {
    // Scan for peer.
    if (ScanForPeer())
    {
      // Peer found, populate 'peer' variable.
      if (peer.channel==WIFI_CHANNEL)
      {
        // Add peer if it has not been added already
        isPaired = managePeer();
        if (isPaired==false)
        {
          // Pair failed
          Serial.println("Pair failed");
        }
      }
    }
    else delay(3000); // Try again in 3 seconds.
  }
  else
  {
    static uint8_t dummy_data = 0x55;
    digitalWrite(LED_PIN,HIGH);
    sendData(&dummy_data,sizeof(dummy_data));
    delay(500);
    digitalWrite(LED_PIN,LOW);
    delay(500);
  }
#endif

#ifdef NODE_TYPE_SLAVE
  if (ping==true)
  {
    digitalWrite(LED_PIN,HIGH);
    delay(100);
    digitalWrite(LED_PIN,LOW);
    ping = false;
  }
#endif

}
