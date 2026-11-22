# Display: `DisplayController`

Drives the LCD on the rig. It is a **dumb view**: it shows the state it is given and makes no decisions.

## What the screen shows
- **Main state:** `READY` (pod and water loaded, OK to brew) or `RELOAD` (needs a new pod and water). Also `BREWING...` while the servos run.
- **Next brew:** for example `Next: Tue 07:00`, or `No brews scheduled`.
- **Status line** for special cases:
  - `Waiting for time`: after power-on, before the first NTP sync. Scheduled brews are paused.
  - `No WiFi` / `No broker`: network problems. Scheduled brews still run from NVS if the time is known.
  - `Asleep until 06:30`: shown just before deep sleep (then the display is turned off).
  - optional brief messages like `Brew refused: RELOAD`.

## Responsibilities
- `begin(pins...)`: init the LCD (pins/address passed in by `main` from `BotConstants.hpp`).
- `showStatus(const DisplayStatus&)`: render a full snapshot (state, next brew text, status line). Redraw only what changed, to avoid flicker.
- `off()` / `on()`: backlight/power control around deep sleep.
- `update()`: optional, for things like scrolling text or timing out a temporary message.

Suggested input: a plain struct built by main from BrewControl's status, so this library never needs BrewControl's types:
```cpp
struct DisplayStatus { const char* state; const char* nextBrew; const char* statusLine; };
```

## Must NOT
- decide anything (whether to brew, what state we're in). It only renders;
- include Network, Motors, or BrewControl headers;
- block for long in any call.

## Open question
**The LCD model isn't chosen yet.** A 16x2 I2C character LCD (HD44780 + PCF8574 backpack) is the simplest. It uses 2 pins and fits the text above in two lines. The choice decides which library goes in `lib_deps` and which pins go in `BotConstants.hpp`. On the C6, `Wire.begin(sda, scl)` lets you pick the I2C pins.
