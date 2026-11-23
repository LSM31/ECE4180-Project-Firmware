// Webserver test: set the ESP32-C6's onboard RGB LED color from the GitHub Pages site via EMQX.
// Standalone program, built only by env:webserver_test (pio run -e webserver_test -t upload).
// The ESP hosts no web server: the page (docs/color.html) and the ESP both talk to the MQTT broker.
//
// Topics:
//   coffee/test/led        page -> ESP   "#RRGGBB" (e.g. "#ff8800"), or "off"   (NOT retained)
//   coffee/test/led/state  ESP  -> page  "#RRGGBB" the LED is actually showing   (retained)
//   coffee/online          ESP  -> page  "online" | "offline"                    (retained, "offline" = Last Will)
//
// Needs include/secrets.hpp (copy include/secrets.example.hpp and fill it in).
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "BotConstants.hpp"
#include "secrets.hpp"

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 8   // onboard WS2812 on the C6 DevKitC-1
#endif

constexpr uint8_t  MAX_BRIGHTNESS = 60;   // scale 0-255 colors down; the LED is blinding at full
constexpr char     TOPIC_CMD[]    = "coffee/test/led";
constexpr char     TOPIC_STATE[]  = "coffee/test/led/state";
constexpr char     TOPIC_ONLINE[] = "coffee/online";
constexpr uint32_t WIFI_RETRY_MS  = 15000;
constexpr uint32_t MQTT_RETRY_MS  = 5000;

static WiFiClientSecure tls;
static PubSubClient     mqtt(tls);
static uint8_t          r = 0, g = 0, b = 0;
static uint32_t         lastWifiTry = 0, lastMqttTry = 0;
static bool             wifiWasUp = false;

static void applyLed()
{
    rgbLedWrite(RGB_BUILTIN, r * MAX_BRIGHTNESS / 255, g * MAX_BRIGHTNESS / 255, b * MAX_BRIGHTNESS / 255);
}

static void publishState()
{
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
    mqtt.publish(TOPIC_STATE, hex, true);
}

// Accepts "#RRGGBB", "RRGGBB", or "off".
static bool parseColor(String s, uint8_t &rr, uint8_t &gg, uint8_t &bb)
{
    s.trim();
    s.toLowerCase();
    if (s == "off") { rr = gg = bb = 0; return true; }
    if (s.startsWith("#")) s.remove(0, 1);
    if (s.length() != 6) return false;
    for (char c : s) if (!isxdigit((unsigned char)c)) return false;
    uint32_t v = strtoul(s.c_str(), nullptr, 16);
    rr = (v >> 16) & 0xff; gg = (v >> 8) & 0xff; bb = v & 0xff;
    return true;
}

static void onMessage(char *topic, byte *payload, unsigned int len)
{
    String msg;
    msg.reserve(len);
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    Serial.printf("[mqtt] %s: %s\n", topic, msg.c_str());

    if (strcmp(topic, TOPIC_CMD) != 0) return;
    if (!parseColor(msg, r, g, b)) { Serial.println("[mqtt] not a #RRGGBB color, ignoring"); return; }

    applyLed();
    publishState();
}

static void connectMqtt()
{
    Serial.printf("[mqtt] connecting to %s:%u as %s ... ", Secrets::MQTT_HOST, Secrets::MQTT_PORT,
                  BotConstants::MQTT_CLIENT_ID);

    // Clean session + retained "offline" Last Will on coffee/online
    if (mqtt.connect(BotConstants::MQTT_CLIENT_ID, Secrets::MQTT_USER, Secrets::MQTT_PASSWORD,
                     TOPIC_ONLINE, 1, true, "offline", true))
    {
        Serial.println("connected");
        mqtt.subscribe(TOPIC_CMD, 1);
        mqtt.publish(TOPIC_ONLINE, "online", true);
        publishState();
    }
    else
    {
        // -4 timeout, -2 network/TLS failure, 4 bad user/pass, 5 not authorized (check EMQX ACL)
        Serial.printf("failed, state=%d\n", mqtt.state());
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=== Webserver (MQTT LED color) test ===");

    applyLed(); // start dark

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
    mqtt.setBufferSize(512);
}

void loop()
{
    uint32_t now = millis();

    // WiFi upkeep (non-blocking)
    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiWasUp) { Serial.println("[wifi] lost connection"); wifiWasUp = false; }
        if (now - lastWifiTry > WIFI_RETRY_MS)
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
        lastMqttTry = now - MQTT_RETRY_MS; // try MQTT right away
    }

    // MQTT upkeep
    if (!mqtt.connected())
    {
        if (now - lastMqttTry >= MQTT_RETRY_MS)
        {
            lastMqttTry = now;
            connectMqtt();
        }
        return;
    }
    mqtt.loop();
}
