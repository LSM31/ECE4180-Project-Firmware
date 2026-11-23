// LED color test page: talks to the ESP32 only through the EMQX broker.
// coffee/test/led        -> ESP  "#rrggbb" or "off"  (NOT retained)
// coffee/test/led/state  <- ESP  "#rrggbb"           (retained, the color the LED is really showing)
// coffee/online          <- ESP  "online"/"offline"  (retained, Last Will)
const TOPIC_CMD = "coffee/test/led";
const TOPIC_STATE = "coffee/test/led/state";
const TOPIC_ONLINE = "coffee/online";
const SWATCHES = ["#ff0000", "#ff8800", "#ffff00", "#00ff00", "#0088ff", "#ff00ff"];

const $ = (id) => document.getElementById(id);
const picker = $("picker");
const offBtn = $("off");
const swatchBox = $("swatches");

SWATCHES.forEach((c) => {
  const b = document.createElement("button");
  b.style.background = c;
  b.title = c;
  b.disabled = true;
  b.onclick = () => { picker.value = c; send(c); };
  swatchBox.appendChild(b);
});

function setDot(dot, textEl, ok, text) {
  dot.className = "dot " + (ok === null ? "" : ok ? "ok" : "bad");
  textEl.textContent = text;
}

function setEnabled(on) {
  [picker, offBtn, ...swatchBox.children].forEach((el) => (el.disabled = !on));
}

function showLed(hex) {
  const bulb = $("bulb");
  if (!/^#[0-9a-f]{6}$/i.test(hex)) { bulb.className = "bulb unknown"; bulb.style = ""; $("ledText").textContent = "LED color unknown"; return; }
  bulb.className = "bulb";
  const isOff = hex.toLowerCase() === "#000000";
  bulb.style.background = isOff ? "#555" : hex;
  bulb.style.boxShadow = isOff ? "none" : `0 0 40px 10px ${hex}aa`;
  $("ledText").textContent = isOff ? "LED is OFF" : `LED is ${hex.toUpperCase()}`;
}

const cfg = window.APP_CONFIG;
const client = mqtt.connect(cfg.brokerUrl, {
  username: cfg.username,
  password: cfg.password,
  clientId: "coffeebot-web-" + Math.random().toString(16).slice(2, 10), // random so two tabs don't kick each other off
  clean: true,
  reconnectPeriod: 3000,
});

client.on("connect", () => {
  setDot($("brokerDot"), $("brokerText"), true, "Broker connected");
  client.subscribe([TOPIC_STATE, TOPIC_ONLINE], { qos: 1 });
  setEnabled(true);
});
client.on("reconnect", () => setDot($("brokerDot"), $("brokerText"), null, "Reconnecting…"));
client.on("close", () => { setDot($("brokerDot"), $("brokerText"), false, "Broker offline"); setEnabled(false); });
client.on("error", (err) => { console.error(err); setDot($("brokerDot"), $("brokerText"), false, "Broker error: " + err.message); });

client.on("message", (topic, payload) => {
  const msg = payload.toString().trim().toLowerCase();
  if (topic === TOPIC_STATE) showLed(msg);
  if (topic === TOPIC_ONLINE) setDot($("espDot"), $("espText"), msg === "online", msg === "online" ? "ESP online" : "ESP offline");
});

function send(cmd) {
  client.publish(TOPIC_CMD, cmd, { qos: 1, retain: false });
}

// Throttle while dragging the picker so we don't flood the broker; always send the final value.
let lastSent = 0, pending = null;
picker.addEventListener("input", () => {
  const now = Date.now();
  clearTimeout(pending);
  if (now - lastSent > 150) { lastSent = now; send(picker.value); }
  else pending = setTimeout(() => { lastSent = Date.now(); send(picker.value); }, 150);
});
offBtn.onclick = () => send("off");
