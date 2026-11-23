// Template for include/secrets.hpp.
// Copy this file to include/secrets.hpp (which is gitignored) and fill in real values.
// NEVER commit secrets.hpp.
#pragma once

namespace Secrets
{
    // WiFi (GTother: the board's MAC must be registered at https://getonline.gatech.edu/other)
    constexpr const char *WIFI_SSID     = "GTother";
    constexpr const char *WIFI_PASSWORD = "CHANGE_ME";

    // EMQX Cloud Serverless: deployment overview -> "Connection Information"
    constexpr const char *MQTT_HOST     = "CHANGE_ME.emqxsl.com";
    constexpr uint16_t    MQTT_PORT     = 8883;              // MQTT over TLS
    constexpr const char *MQTT_USER     = "CHANGE_ME";       // the ESP's broker user, NOT the web page's
    constexpr const char *MQTT_PASSWORD = "CHANGE_ME";

    // CA certificate (PEM) downloaded from the EMQX deployment overview.
    // Leave empty ("") to skip certificate checking while testing. Still encrypted, just not verified.
    constexpr const char *MQTT_CA_CERT = "";
    // Example:
    // constexpr const char *MQTT_CA_CERT = R"PEM(
    // -----BEGIN CERTIFICATE-----
    // ...
    // -----END CERTIFICATE-----
    // )PEM";
}
