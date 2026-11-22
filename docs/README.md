**Hello Evan and Claude**

This is where the HTML files should live for the website

It's named docs/ instead of web/ since this site will need to live on GitHub pages.

---

# docs/: the web page (GitHub Pages)

The remote control for the coffee rig. It's a **static** site (HTML + CSS + JavaScript, no build step required) served by GitHub Pages from this folder. It never talks to the ESP32 directly. It talks to the **EMQX Cloud MQTT broker** using **MQTT.js over secure WebSockets**, and the ESP talks to the same broker. See the top-level `README.md` for the full architecture and topic table.

> This folder is **not** flashed to the ESP. There is no LittleFS / `data/` folder and the ESP runs no web server.

## What the page does
1. **Connect** to `wss://<broker-host>:8084/mqtt` with the **web-page broker user** (not the ESP's user).
2. **Show device state** (subscribe):
   - `coffee/status` (retained): READY / RELOAD / BREWING / WAITING_FOR_TIME / ASLEEP, next brew, "asleep until".
   - `coffee/online` (retained, also the ESP's Last Will): show clearly when the device is offline or asleep.
   - `coffee/schedule` and `coffee/sleep` (retained): the schedules the ESP **actually saved**. Display these, not what the page last sent.
3. **Brew now** (publish to `coffee/brew`, **retain = false**, with the cup size). Disable or warn when status isn't READY or the device is offline/asleep. The ESP makes the final decision anyway.
4. **Edit the brew schedule** (publish **retained** to `coffee/schedule/set`), including a `version` (for example, the current Unix time) so the ESP can ignore stale copies.
5. **Edit the sleep schedule** (publish **retained** to `coffee/sleep/set`): enabled on/off, sleep time, wake time. Remote brews are impossible while the ESP sleeps; the UI should say so.

Payload formats are proposals in the top-level README, section 4. **Agree on them with the firmware side before building forms.**

## Suggested files
```
docs/
├── index.html     UI
├── app.js         MQTT.js connection, subscriptions, publish helpers
├── style.css
└── config.js      broker host + web-page user (see security note)
```
Load MQTT.js from a CDN with a pinned version, for example `https://unpkg.com/mqtt@5/dist/mqtt.min.js` (pin the exact version).

## Security note
Anything in this folder is public once Pages serves it, including the web-page user's broker password. That's acceptable **only** because that user is restricted by an EMQX ACL to:
- publish: `coffee/brew`, `coffee/schedule/set`, `coffee/sleep/set`
- subscribe: `coffee/status`, `coffee/schedule`, `coffee/sleep`, `coffee/online`

Never put the **ESP's** broker credentials or the WiFi password here. Anyone with the URL can brew. Whether to add a PIN or similar is an open question.

## Deploying
Repo Settings → Pages → "Deploy from a branch" → `main`, folder `/docs`. A push to `main` updates the site within a minute or two, and no reflashing is needed. Pages on a private repo requires GitHub Pro (free with the Student Developer Pack).
