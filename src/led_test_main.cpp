// LED test: control the ESP32-C6's onboard RGB LED from the GitHub Pages site via EMQX.
// Standalone program, built only by env:led_test. Proves the WiFi -> MQTT/TLS -> web page path.
//
// Topics:
//   coffee/led/set  page -> ESP   "on" | "off" | "toggle"      (NOT retained)
//   coffee/led      ESP  -> page  "on" | "off"                 (retained, the LED's real state)
//   coffee/online   ESP  -> page  "online" | "offline"         (retained, "offline" is the Last Will)
//
// Needs include/secrets.hpp (copy include/secrets.example.hpp and fill it in).
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.hpp"

constexpr uint8_t  LED_PIN        = 8;      // onboard WS2812 RGB LED on the C6 DevKitC-1
constexpr uint8_t  LED_BRIGHTNESS = 40;     // 0-255, it's very bright at full
constexpr char     TOPIC_SET[]    = "coffee/led/set";
constexpr char     TOPIC_STATE[]  = "coffee/led";
constexpr char     TOPIC_ONLINE[] = "coffee/online";
constexpr uint32_t RETRY_MS       = 5000;

static WiFiClientSecure tls;
static PubSubClient     mqtt(tls);
static bool             ledOn = false;
static uint32_t         lastWifiTry = 0, lastMqttTry = 0;
static bool             wifiWasUp = false;

static void applyLed()
{
    if (ledOn) rgbLedWrite(LED_PIN, LED_BRIGHTNESS, LED_BRIGHTNESS, LED_BRIGHTNESS); // white
    else       rgbLedWrite(LED_PIN, 0, 0, 0);
}

static void publishState()
{
    mqtt.publish(TOPIC_STATE, ledOn ? "on" : "off", true);
}

static void onMessage(char *topic, byte *payload, unsigned int len)
{
    String msg;
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    msg.trim();
    msg.toLowerCase();
    Serial.printf("[mqtt] %s: %s\n", topic, msg.c_str());

    if (strcmp(topic, TOPIC_SET) != 0) return;
    if      (msg == "on")     ledOn = true;
    else if (msg == "off")    ledOn = false;
    else if (msg == "toggle") ledOn = !ledOn;
    else { Serial.println("[mqtt] unknown command, ignoring"); return; }

    applyLed();
    publishState();
}

static void connectMqtt()
{
    String clientId = "esp32c6-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.printf("[mqtt] connecting to %s:%u ... ", Secrets::MQTT_HOST, Secrets::MQTT_PORT);

    // Clean session + retained "offline" Last Will on coffee/online
    if (mqtt.connect(clientId.c_str(), Secrets::MQTT_USER, Secrets::MQTT_PASSWORD,
                     TOPIC_ONLINE, 1, true, "offline", true))
    {
        Serial.println("connected");
        mqtt.subscribe(TOPIC_SET, 1);
        mqtt.publish(TOPIC_ONLINE, "online", true);
        publishState();
    }
    else
    {
        // -4 timeout, -2 network/TLS failure, 4 bad user/pass, 5 not authorized
        Serial.printf("failed, state=%d\n", mqtt.state());
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=== LED test ===");

    applyLed();

    WiFi.mode(WIFI_STA);
    Serial.printf("[wifi] MAC %s, joining %s\n", WiFi.macAddress().c_str(), Secrets::WIFI_SSID);
    WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
    lastWifiTry = millis();

    if (strlen(Secrets::MQTT_CA_CERT) > 0) tls.setCACert(Secrets::MQTT_CA_CERT);
    else
    {
        Serial.println("[tls] no CA cert in secrets.hpp, NOT verifying the broker (testing only)");
        tls.setInsecure();
    }

    mqtt.setServer(Secrets::MQTT_HOST, Secrets::MQTT_PORT);
    mqtt.setCallback(onMessage);
    mqtt.setBufferSize(1024);
}

void loop()
{
    uint32_t now = millis();

    // WiFi upkeep (non-blocking)
    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiWasUp) { Serial.println("[wifi] lost connection"); wifiWasUp = false; }
        if (now - lastWifiTry > 15000)
        {
            Serial.println("[wifi] retrying");
            WiFi.disconnect();
            WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);
            lastWifiTry = now;
        }
        return;
    }
    if (!wifiWasUp)
    {
        Serial.printf("[wifi] connected, IP %s\n", WiFi.localIP().toString().c_str());
        wifiWasUp = true;
        lastMqttTry = now - RETRY_MS; // try MQTT right away
    }

    // MQTT upkeep
    if (!mqtt.connected())
    {
        if (now - lastMqttTry >= RETRY_MS)
        {
            lastMqttTry = now;
            connectMqtt();
        }
        return;
    }
    mqtt.loop();
}
