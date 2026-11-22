# Network: `WebInterface`

Everything that touches the internet: joining **GTother** WiFi, keeping an **MQTT over TLS** connection to **EMQX Cloud Serverless**, and **NTP time sync**. It moves messages in and out. It doesn't interpret brew logic.

> Despite the file name, **the ESP does not host a web server.** The web page is static on GitHub Pages (`docs/`), and both sides talk through the MQTT broker. Consider renaming this to `NetworkController` later. That's naming only, and it hasn't been done.

## Responsibilities
**WiFi**
- Connect to `GTother` (WPA2-Personal password). The ESP's MAC must be registered in GT's LAWN portal first. Do **not** use eduroam.
- Reconnect automatically with backoff. Never block `loop()` waiting for WiFi.

**Time (NTP)**
- After WiFi is up: `configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov")`. The SNTP client resyncs periodically on its own; the interval should be a few hours.
- Detect the first successful sync (for example, `time(nullptr)` past 2024, or an SNTP sync callback) and fire an `onTimeSynced` callback, so main can tell BrewControl the clock is valid.

**MQTT**
- Libraries: `PubSubClient` + `WiFiClientSecure` with the broker's CA cert (`setCACert`). Port **8883**. (Fallback if GTother blocks 8883: WebSockets on 8084, which needs a different client library.)
- `setBufferSize(1024)` or more. The 256-byte default silently drops schedule JSON.
- Connect with the **ESP's own broker user** (credentials passed in by main from `secrets.hpp`), a **clean session**, and a **Last Will** of `"offline"` (retained) on `coffee/online`. Publish retained `"online"` after connecting.
- Subscribe to `coffee/brew`, `coffee/schedule/set`, `coffee/sleep/set`.
- Hand each incoming message to main through a callback: `(topic, payload)`. Parsing JSON is BrewControl's job, not this module's.
- `publish(topic, payload, retained)` for main to send `coffee/status`, `coffee/schedule`, `coffee/sleep` (all retained).
- Reconnect with backoff. Call `client.loop()` from `update()`.
- `prepareForSleep()`: flush pending publishes, disconnect cleanly, WiFi off.

See the topic table in the top-level `README.md` (section 4). Key rule: **`coffee/brew` is never retained.**

## Intended interface (suggestion)
```cpp
struct NetConfig { const char* ssid; const char* wifiPass; const char* host; uint16_t port;
                   const char* user; const char* pass; const char* caCert; const char* tz; };
bool begin(const NetConfig& cfg);
void update();                                             // WiFi/MQTT upkeep, non-blocking
bool publish(const char* topic, const char* payload, bool retained);
void onMessage(std::function<void(const char* topic, const char* payload)> cb);
void onTimeSynced(std::function<void()> cb);
bool isTimeValid() const;
bool isConnected() const;                                  // MQTT up
void prepareForSleep();
```

## Must NOT
- decide whether to brew or interpret schedule contents;
- include Motors, Display, or BrewControl headers;
- contain credentials. They come from `secrets.hpp` through main, and `secrets.hpp` is gitignored;
- run an HTTP server or open any inbound port;
- block `loop()` during reconnects.

## Setup reminders
- EMQX deployment: Serverless, spend limit $0. Create separate users for the ESP and the web page with topic ACLs. Download the CA cert from the deployment overview.
- Print `WiFi.macAddress()` once over serial to register the device in LAWN (registration lasts about 4 months).
