# Motors: `MotorController`

Drives the 5 micro servos (SG90 / MG90S class) that press the Keurig K-Mini Go's buttons. This is a **dumb hardware wrapper**: it presses what it is told to press and knows nothing about READY, schedules, or the network.

## Hardware it assumes
- 5 servos, one per button. 3 of the buttons are cup-size buttons spaced about 30 to 40 mm apart. Which servo maps to which button is defined in `include/BotConstants.hpp` and passed in by `main`.
- Each servo signal goes straight to an ESP32-C6 GPIO, driven with LEDC PWM at **50 Hz** (20 ms period). **No motor driver.** The C6 has 6 LEDC channels, enough for 5 servos.
- Servo power is a **separate 5 V ~2 A supply with ground shared with the ESP32**. Only one servo moves at a time.
- Arduino-ESP32 3.x LEDC API: `ledcAttach(pin, 50, 14)`, `ledcWrite(pin, duty)`, `ledcDetach(pin)`. Pulse width to duty: `duty = us * ((1 << bits) - 1) / 20000` (see `src/motor_test_main.cpp`).

## Responsibilities
- `begin(pins, rest/press pulse widths, timings)`: set up the servos, move each to its rest position, then stop driving it.
- **Press one button:** attach PWM, move to the press position, hold briefly (~150 to 300 ms), return to rest, then **detach PWM** so the servo doesn't sit buzzing or holding current against the button.
- **Run a press sequence:** an ordered list of steps like *press button A, wait N ms, press button B*. A brew is one of these sequences. The sequence for each cup size is defined by the caller (BrewControl/main), not hard-coded here.
- Report when a sequence has **finished** (return value or callback), so BrewControl can move to `RELOAD`.
- `isBusy()`, so a second sequence can't start while one is running.
- `detachAll()`, called before deep sleep.

## Intended interface (suggestion)
```cpp
struct PressStep { uint8_t servo; uint16_t waitAfterMs; };
bool begin(const uint8_t pins[5], const uint16_t restUs[5], const uint16_t pressUs[5]);
bool runSequence(const PressStep* steps, size_t count); // false if busy
bool isBusy() const;
void update();       // advances a non-blocking sequence, if implemented that way
void detachAll();
```
A blocking `runSequence` is acceptable for a first version (a brew sequence is a few seconds). A non-blocking `millis()` step machine driven by `update()` is better, because MQTT keeps running during the brew.

## Must NOT
- check READY/RELOAD or decide whether a brew is allowed. That's BrewControl's single brew function;
- include Network, Display, or BrewControl headers;
- move more than one servo at the same time;
- leave a servo stalled against a button.

## Bring-up
Use `env:motor_test` (`src/motor_test_main.cpp`) to find each servo's rest and press pulse widths by typing microseconds into the serial monitor, then record them in `BotConstants.hpp`.
