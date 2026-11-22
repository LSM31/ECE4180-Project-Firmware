# src/

Application code: the entry point and the brew/schedule logic. Hardware and network drivers live in `lib/`, not here. See the top-level `README.md` for the full architecture and MQTT topics.

## `main.cpp`: wiring only
Keep this file as small as possible. It should read like a wiring diagram:
1. `setup()`: create and `begin()` each controller (Display, Motors, Network, BrewControl), passing pins and settings from `BotConstants.hpp` and credentials from `secrets.hpp`.
2. Connect modules with callbacks, for example:
   - Network message on `coffee/brew` → `BrewControl::requestBrew(size, REMOTE)`
   - Network message on `coffee/schedule/set` → `BrewControl::onScheduleMessage(json)`
   - Network message on `coffee/sleep/set` → `BrewControl::onSleepMessage(json)`
   - Network "time synced" → `BrewControl::setTimeValid(true)`
   - BrewControl "run this press sequence" → `MotorController`
   - BrewControl "status changed" → `DisplayController` **and** Network publish to `coffee/status` (retained)
   - BrewControl "go to sleep for N seconds" → main's sleep routine (below)
3. `loop()`: call each module's `update()`. No logic of its own.

**Go-to-sleep routine** (lives in main because it touches every module): publish retained `ASLEEP` status with the "asleep until" time, let MQTT flush, disconnect, turn off the display, detach servos, `esp_sleep_enable_timer_wakeup(us)`, `esp_deep_sleep_start()`. Waking from deep sleep is a reboot, so it comes back through `setup()`.

Currently still the PlatformIO template.

## `BrewControl.cpp`: the decision layer (currently empty)
Everything about *whether* and *when* to brew. Interface: `include/BrewControl.hpp`.

**Responsibilities**
- **State machine:** `WAITING_FOR_TIME` (clock not synced since power-on) → `READY` ⇄ `BREWING` → `RELOAD` → (reload confirmed) → `READY`. Plus `ASLEEP`, published just before deep sleep.
- **The single brew function** (`requestBrew`): used by both remote and scheduled brews. It refuses if not `READY` or already `BREWING`. Scheduled brews are also refused if the time isn't valid. On success, it asks the motor layer to run the right press sequence, then moves to `RELOAD`.
- **Brew schedule:** parse the JSON from `coffee/schedule/set`, ignore it if its `version` isn't newer than the saved one, save it to NVS (`Preferences`), and report the saved copy so main can publish it retained on `coffee/schedule`. Each loop, check whether a scheduled brew is due. Make sure each entry fires **once** per occurrence, even if `update()` runs many times in that minute, and across a reboot.
- **Sleep schedule:** same storage pattern via `coffee/sleep/set` → NVS → `coffee/sleep`. When enabled and the sleep time is reached, compute seconds until `wake - margin` (margin from `BotConstants`, 5 to 10 min). **If the wake time has already passed, do not sleep.** Then ask main to sleep.
- **Next brew:** compute the next scheduled brew time for the display and status.
- Load everything from NVS in `begin()`, so schedules work fully offline after a power loss. Only the time is lost, which is why `WAITING_FOR_TIME` exists.

**Must NOT**
- include any `lib/` header (Display, Motors, Network). Talk to them only through the handlers main gives it;
- touch GPIO, WiFi, or MQTT directly;
- block. Use `time()`/`localtime_r()` for wall-clock checks and `millis()` for intervals.

**Open:** where the READY signal comes from (button, sensor, or web message). Keep `setReady()` as the only entry point so the source can change later.

## `motor_test_main.cpp`: servo bring-up tool
Standalone program built only by `env:motor_test` (`build_src_filter = -<*> +<motor_test_main.cpp>`). Over the serial monitor (115200), type a pulse width in microseconds and the servo on `MOTOR_SIG` moves there. Use it to find each servo's rest and press pulse widths, then record them in `BotConstants.hpp`. It must stay excluded from the main firmware env, because it defines its own `setup()`/`loop()`.
