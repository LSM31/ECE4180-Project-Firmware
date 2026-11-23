// Prints the ESP32-C6's MAC addresses over Serial.
// Use the "WiFi Station (STA)" address to register the board on GTother.
// Build/upload with:  pio run -e mac_address -t upload -t monitor
#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_wifi.h>

static void printMac(const char *label, const uint8_t mac[6])
{
    Serial.printf("%-22s %02X:%02X:%02X:%02X:%02X:%02X\n", label,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void printAllMacs()
{
    uint8_t mac[6];

    Serial.println();
    Serial.println("===== ESP32-C6 MAC addresses =====");

    if (esp_efuse_mac_get_default(mac) == ESP_OK)
        printMac("Base (eFuse):", mac);

    // This is the one to register on GTother
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
        printMac("WiFi Station (STA):", mac);

    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK)
        printMac("WiFi SoftAP:", mac);

    if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK)
        printMac("Bluetooth (BLE):", mac);

    Serial.println("==================================");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // WiFi must be started for esp_wifi_get_mac() to work
    WiFi.mode(WIFI_STA);
    delay(100);

    printAllMacs();
}

void loop()
{
    // Re-print every few seconds so you don't miss it when the monitor opens
    delay(5000);
    printAllMacs();
}
