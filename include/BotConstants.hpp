#pragma once

namespace BotConstants {

    // need to add pin mappings for servo motors
    #define HELLO 0

    // MQTT client ID for the ESP. Must be unique on the broker; not a secret.
    constexpr const char *MQTT_CLIENT_ID = "coffeebot-esp32";
}
