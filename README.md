# ECE 4180 Remote Coffee Maker (firmware + web page)

A small rig that sits on top of a **Keurig K-Mini Go** and presses its buttons with micro servos, so coffee can be brewed remotely from a web page or automatically on a schedule. An **ESP32-C6-DevKitC-1** runs the firmware. It joins Georgia Tech's **GTother** WiFi, talks to a cloud **MQTT broker (EMQX Cloud Serverless)**, and the user interface is a static web page hosted on **GitHub Pages** from the `docs/` folder of this repo.

> **Status:** skeleton only. The folder layout, module boundaries, and READMEs are in place. Apart from `src/motor_test_main.cpp` (a serial-driven servo tester), nothing is implemented yet. Every README describes what the code is *meant* to do, so read them before writing code in a folder.

---

## 1. Hardware

| Part | Notes |
|---|---|
| ESP32-C6-DevKitC-1 | Single RISC-V core @ 160 MHz, WiFi 6 (2.4 GHz), 6 LEDC PWM channels. Needs Arduino-ESP32 **3.x** (the pioarduino platform in `platformio.ini` provides this). |
| 5x micro servos (SG90 / MG90S class) | One per Keurig button. 3 of the buttons are cup-size buttons spaced about 30 to 40 mm apart. Driven **directly from ESP32 GPIO with PWM (50 Hz)**. There is no motor driver, because hobby servos have their own driver built in. A PCA9685 board is an optional later upgrade, not part of the plan. |
| Separate 5 V, ~2 A supply for the servos | **Ground must be tied to the ESP32 ground.** Do not power the servos from the 3.3 V pin. A 470 to 1000 µF capacitor across the servo 5 V rail is recommended. |
| Display (LCD) | Shows READY / RELOAD, next scheduled brew, and status ("waiting for time", "asleep until ..."). **Exact LCD model is not decided yet** (see Open questions). |
| READY / RELOAD signal | Whether the Keurig has water and a fresh pod. **How this is detected is not decided yet** (see Open questions). |

No RTC module. A DS3231 was considered and scrapped. The ESP keeps time from NTP only.

Only one servo ever moves at a time. A "brew" is a **press sequence**, for example: press a button, wait, press another button.

---

## 2. Repository layout

```
ECE4180-Project-Firmware/
├── README.md              <- this file: architecture, topics, setup, open questions
├── platformio.ini         <- build environments (see section 6.2)
├── include/
│   ├── README             <- what belongs in include/
│   ├── BotConstants.hpp   <- pin map + tunable constants shared by the app
│   └── BrewControl.hpp    <- public interface of the brew/schedule logic
├── src/
│   ├── README.md          <- what belongs in src/
│   ├── main.cpp           <- creates modules, wires callbacks, runs the loop (keep it tiny)
│   ├── BrewControl.cpp    <- brew decisions, schedule, sleep schedule, NVS storage
│   └── motor_test_main.cpp<- standalone servo test program (env:motor_test only)
├── lib/
│   ├── README             <- rules for the subsystem libraries
│   ├── Display/           <- DisplayController: renders status on the LCD
│   ├── Motors/            <- MotorController: dumb servo wrapper, runs press sequences
│   └── Network/           <- WebInterface: WiFi + MQTT (TLS) + NTP time sync
└── docs/                  <- static web page served by GitHub Pages (HTML + MQTT.js)
```

Not present on purpose:
- `data/`: not needed. Nothing is stored in LittleFS and the ESP does **not** serve web pages.
- `test/`: there are no unit tests in this project.

---

## 3. Architecture

### 3.1 Module boundaries

```
                         ┌──────────────────────────┐
   GitHub Pages page ───►│   EMQX Cloud (MQTT/TLS)   │◄─── ESP32 (GTother WiFi)
   (docs/, MQTT.js, wss) └──────────────────────────┘
                                                        ┌──────────────────────┐
                                                        │       main.cpp       │
                                                        │  creates + wires all │
                                                        └──┬────┬────┬────┬────┘
                                         callbacks/calls   │    │    │    │
                 ┌─────────────────────────────────────────┘    │    │    └───────────────┐
                 ▼                          ▼                   ▼                         ▼
   lib/Network (WebInterface)   src/BrewControl          lib/Motors (MotorController)   lib/Display (DisplayController)
   WiFi, MQTT, NTP              decides IF/WHEN to brew  moves servos, nothing else     shows what it is told
   raw messages in/out          schedule + sleep in NVS
                                READY gate (one place)
```

Rules (these matter more than any single function name):

1. **Library modules in `lib/` never include each other and never include `BrewControl.hpp`.** Each one only knows about its own hardware or service.
2. **`main.cpp` owns everything and connects modules with callbacks** (for example, Network's "message received on coffee/brew" callback is pointed at `BrewControl`'s brew request). Main should read as a wiring diagram, not contain logic.
3. **There is exactly ONE brew function** (in `BrewControl`). Remote brews and scheduled brews both go through it. It checks READY (and that we are not already brewing) **before** anything moves. Nothing else may tell the motors to brew.
4. **MotorController is "dumb".** It runs the press sequence it is given and does not know about READY, schedules, or the network.
5. **DisplayController is "dumb".** It renders the state it is given and makes no decisions.
6. **Pins and tunables live in `include/BotConstants.hpp`.** Main passes pins into each library's `begin(...)`, so libraries don't depend on project headers.
7. **Secrets are never committed** (see section 6.3).

The existing `MotorController.hpp` uses `etl::singleton` (Embedded Template Library) so there is one global instance per module. Follow the same pattern for the other controllers for consistency.

### 3.2 Runtime behavior

**Boot (power-on or wake from deep sleep, both start in `setup()`):**
1. Init display. Show "booting".
2. `BrewControl` loads the brew schedule and sleep schedule from NVS (`Preferences`). These survive power loss and work with no internet.
3. Init servos (all at rest position, then PWM detached).
4. Network connects to GTother, then syncs time with NTP (`configTzTime`, US Eastern), then connects to the MQTT broker over TLS and subscribes.
5. Until NTP succeeds, the clock is not trusted: **scheduled brews are skipped** and the display shows "waiting for time". Remote brews still work as soon as MQTT is connected.
6. Publish a fresh retained status.

**Loop (non-blocking):** Network keeps WiFi and MQTT alive (reconnect with backoff) and dispatches incoming messages. BrewControl checks once per loop/second whether a scheduled brew is due and whether it's time to sleep. The display refreshes on state changes. NTP resyncs every few hours (the SNTP client can do this automatically).

**Brew request (remote or scheduled), the single path:**
1. If time-dependent (scheduled) and the clock isn't synced, skip.
2. If state is not READY, or a brew is already in progress, refuse and publish the reason in status.
3. Otherwise, set state to BREWING, run the press sequence through MotorController, then set state to RELOAD (a K-Mini Go is single-serve and needs a new pod and water after every brew).
4. Publish the updated retained status and update the display.

**Sleep schedule (optional, set from the web page):**
- When enabled and the sleep time arrives, BrewControl computes `seconds until (wake time - margin)` where the margin is 5 to 10 minutes, to absorb RC-clock drift in deep sleep.
- If the wake time has already passed (for example, a late schedule edit), **don't sleep**.
- Before sleeping: publish a retained "asleep until HH:MM" status, let MQTT flush, turn off the display, detach servos, then `esp_sleep_enable_timer_wakeup(us)` and `esp_deep_sleep_start()`.
- On wake the chip reboots into `setup()` and goes through the boot steps above, including an NTP resync.
- While asleep, **remote brews are impossible** (WiFi is off). Schedule changes made while it sleeps are retained on the broker and arrive on reconnect.

**Power loss:** schedules survive in NVS. The time does not, so scheduled brews are skipped until NTP syncs.

---

## 4. MQTT topics (proposed, finalize before coding the web page)

Broker: EMQX Cloud Serverless. The ESP uses **MQTT over TLS, port 8883**. The web page uses **MQTT over secure WebSockets, port 8084** (`wss://<broker-host>:8084/mqtt`).

| Topic | Direction | Retained | Payload (proposal) | Purpose |
|---|---|---|---|---|
| `coffee/brew` | page → ESP | **NO** | `{"size":8}` (oz; or a button/preset id) | Brew now. **Must never be retained**, or the ESP would brew again on every reconnect or wake. Use a clean session so queued brews aren't replayed. |
| `coffee/schedule/set` | page → ESP | yes | `{"version":<unix ts>, "brews":[{"days":"MTWTF..", "time":"07:00", "size":8}, ...]}` | Page requests a new brew schedule. Retained so the ESP gets it after sleep or a reboot. The ESP applies it only if `version` is newer than what's in NVS. |
| `coffee/schedule` | ESP → page | yes | same shape as above | The schedule the ESP **actually** has saved. The page displays this, not what it sent. |
| `coffee/sleep/set` | page → ESP | yes | `{"version":..., "enabled":true, "sleep":"23:00", "wake":"06:30"}` | Request a new sleep schedule. |
| `coffee/sleep` | ESP → page | yes | same shape | Sleep schedule the ESP actually has saved. |
| `coffee/status` | ESP → page | yes | `{"state":"READY\|RELOAD\|BREWING\|WAITING_FOR_TIME\|ASLEEP", "next_brew":"Tue 07:00", "asleep_until":"06:30", "msg":"..."}` | Current device state. Republished on every change. |
| `coffee/online` | ESP → page | yes | `"online"` / `"offline"` | Set `"offline"` as the MQTT **Last Will** so the page knows when the ESP drops off unexpectedly. |

Broker users and ACLs (create these in EMQX "Access Control"):
- **ESP user:** publish and subscribe on `coffee/#`.
- **Web page user:** publish only `coffee/brew`, `coffee/schedule/set`, `coffee/sleep/set`. Subscribe only `coffee/status`, `coffee/schedule`, `coffee/sleep`, `coffee/online`. The page's password is visible to anyone who views the page source, so the ACL is the real protection.

Implementation notes:
- **PubSubClient's default packet buffer is 256 bytes.** Call `setBufferSize(1024)` (or larger) or JSON schedules will be silently dropped.
- ArduinoJson is the suggested JSON parser on the ESP (not yet in `lib_deps`).
- Times are local US Eastern. Use `configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov")` so daylight saving is handled.

---

## 5. Web page (docs/)

A static HTML/JS page served by GitHub Pages from `docs/` on `main`. It uses **MQTT.js** to connect to the broker over `wss://...:8084/mqtt` with the web-page user. It:
- shows `coffee/status`, `coffee/online`, the current schedule and sleep schedule;
- has a "Brew now" control (publishes to `coffee/brew`);
- edits the brew schedule and sleep schedule (publishes retained `.../set`).

See `docs/README.md` for details. The ESP does **not** host any web server.

---

## 6. Setup

### 6.1 Toolchain
- VS Code + PlatformIO extension.
- `platformio.ini` uses the **pioarduino** `platform-espressif32` (stable), which provides Arduino-ESP32 3.x and supports the C6. The stock PlatformIO espressif32 platform does **not** support Arduino on the C6.
- USB CDC on boot is enabled, so serial output comes over the board's USB port at 115200.

### 6.2 Build environments
- `env:motor_test`: builds only `src/motor_test_main.cpp`. Type a pulse width in µs into the serial monitor to move a servo on `MOTOR_SIG` and find the rest and press positions.
- **Missing: a main firmware environment** (see To-do). Running `pio run` today only builds `motor_test`.

### 6.3 Secrets (not created yet, see To-do)
WiFi password, broker host, ESP broker username/password, and the broker CA certificate should live in `include/secrets.hpp`, which is listed in `.gitignore`. Commit an `include/secrets.example.hpp` with placeholder values so a new clone knows what to fill in. **Never commit real credentials.** If one gets pushed by accident, rotate it in EMQX or LAWN; deleting the commit is not enough.

### 6.4 GTother WiFi (Georgia Tech)
1. Get the ESP32's WiFi MAC address. Print `WiFi.macAddress()` over serial.
2. Register that MAC in the **LAWN** portal (https://getonline.gatech.edu/other) for the **GTother** network. Registration lasts about 4 months and can be renewed.
3. Put the GTother password in `secrets.hpp`. Do **not** use eduroam (it is WPA2-Enterprise and would require a personal GT login on the device).
4. GTother allows outgoing connections, which is all MQTT needs. Inbound connections to the ESP are not expected to work, and we don't rely on them.

### 6.5 EMQX Cloud Serverless
1. Deployment: Serverless, North America, **spend limit $0** (cannot be charged). Already created by Landon.
2. Access Control: two users, one for the ESP and one for the web page, with the ACLs in section 4.
3. From the deployment overview, copy the broker hostname and download the **CA certificate**. The ESP passes it to `WiFiClientSecure::setCACert()`.
4. Ports: 8883 (MQTT/TLS, ESP) and 8084 (WSS, web page). If GTother ever blocks 8883, the ESP can fall back to WebSockets on 8084.

### 6.6 GitHub Pages
Repo Settings → Pages → Source: "Deploy from a branch" → `main` / `/docs`. The site appears at `https://<user>.github.io/ECE4180-Project-Firmware/`. Pages on a **private** repo needs GitHub Pro (free with the GitHub Student Developer Pack).

---

## 7. To-do before / while writing code

These are structural gaps found while writing these READMEs. They were intentionally **not** fixed, because this pass only touched documentation.

- [ ] **Add a main build env** to `platformio.ini`, e.g. `[env:main]` with `build_src_filter = +<*> -<motor_test_main.cpp>`. Without the filter, `main.cpp` and `motor_test_main.cpp` both define `setup()`/`loop()` and the link fails.
- [ ] **Add `lib_deps`** for the main env: `knolleary/PubSubClient`, `bblanchon/ArduinoJson`, and the LCD library once the display is chosen. (`WiFi`, `WiFiClientSecure`, `Preferences`, and SNTP are built into Arduino-ESP32.)
- [ ] **Create `include/secrets.example.hpp`** and add `include/secrets.hpp` to `.gitignore`.
- [ ] **Fill in the pin map** in `include/BotConstants.hpp` (5 servo pins, LCD pins, READY input). Prefer `constexpr` over `#define`. On the C6, avoid the strapping pins (GPIO 4, 5, 8, 9, 15) and the USB pins (GPIO 12, 13) where possible; note that `motor_test` currently uses GPIO 5.
- [ ] Consider renaming `lib/Network/WebInterface.*` to something like `NetworkController`, since it is an MQTT client and not a web server. (Only a naming issue.)
- [ ] `src/main.cpp` is still the PlatformIO template.

## 8. Open questions

1. **How is READY detected?** Options: a physical "reloaded" button on the rig, a sensor (pod lid switch, water level), or a "I reloaded it" button on the web page. Until decided, BrewControl should treat it as an input it is given, so the source can change later.
2. **Which LCD?** (16x2 I2C character LCD is the simplest. This changes DisplayController and the pin map.)
3. **Exact press sequences** for each cup size, and the servo rest/press pulse widths (use `motor_test` to measure).
4. **What are the other two buttons** on the rig used for (power, and one other), and do they appear in any sequence?
5. **Payload formats** in section 4 are proposals. Lock them in before the ESP side and the web page are written in parallel.
6. **How many schedule entries** to support (affects NVS layout and payload size).
7. Should the page have any extra protection beyond the broker ACL (for example, a PIN)? Anyone with the page URL can currently brew.
8. Is the repo public or private? (This affects GitHub Pages; see 6.6.)

---

## Notes for AI assistants working in this repo

- Read the README in a folder before editing code there. Each one lists what the module must and must not do.
- Respect the module boundaries in section 3.1. If a change seems to need one library to include another, route it through a callback in `main.cpp` instead.
- Keep `main.cpp` minimal: construction, wiring, `loop()` calling each module's `update()`.
- Don't block in `loop()` for long. Use `millis()`-based timing (servo waits inside a press sequence are the one acceptable short block, unless we later move sequences to a FreeRTOS task).
- Never commit secrets, never retain `coffee/brew`, and never bypass the single brew function.
- The board is an ESP32-**C6** on Arduino-ESP32 **3.x**. APIs differ from 2.x (for example, LEDC is `ledcAttach(pin, freq, bits)` / `ledcWrite(pin, duty)`, with no channel numbers).
