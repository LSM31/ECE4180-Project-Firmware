// LED test page: talks to the ESP32 only through the EMQX broker.
// coffee/led/set  -> ESP  ("on" | "off" | "toggle", NOT retained)
// coffee/led      <- ESP  ("on" | "off", retained)
// coffee/online   <- ESP  ("online" | "offline", retained, LWT)
const TOPIC_SET = "coffee/led/set";
const TOPIC_STATE = "coffee/led";
const TOPIC_ONLINE = "coffee/online";

const $ = (id) => document.getElementById(id);
const buttons = ["toggle", "on", "off"].map($);

function setDot(dot, textEl, ok, text) {
  dot.className = "dot " + (ok === null ? "" : ok ? "ok" : "bad");
  textEl.textContent = text;
}

function setButtons(enabled) {
  buttons.forEach((b) => (b.disabled = !enabled));
}

function showLed(state) {
  const bulb = $("bulb");
  bulb.className = "bulb" + (state === "on" ? " on" : state === "off" ? "" : " unknown");
  $("ledText").textContent = state ? `LED is ${state.toUpperCase()}` : "LED state unknown";
}

const cfg = window.APP_CONFIG;
const client = mqtt.connect(cfg.brokerUrl, {
  username: cfg.username,
  password: cfg.password,
  clientId: "web-" + Math.random().toString(16).slice(2, 10),
  clean: true,
  reconnectPeriod: 3000,
});

client.on("connect", () => {
  setDot($("brokerDot"), $("brokerText"), true, "Broker connected");
  client.subscribe([TOPIC_STATE, TOPIC_ONLINE], { qos: 1 });
  setButtons(true);
});

client.on("reconnect", () => setDot($("brokerDot"), $("brokerText"), null, "Reconnecting…"));
client.on("close", () => { setDot($("brokerDot"), $("brokerText"), false, "Broker offline"); setButtons(false); });
client.on("error", (err) => { console.error(err); setDot($("brokerDot"), $("brokerText"), false, "Broker error: " + err.message); });

client.on("message", (topic, payload) => {
  const msg = payload.toString().trim().toLowerCase();
  if (topic === TOPIC_STATE) showLed(msg);
  if (topic === TOPIC_ONLINE) setDot($("espDot"), $("espText"), msg === "online", msg === "online" ? "ESP online" : "ESP offline");
});

function send(cmd) {
  client.publish(TOPIC_SET, cmd, { qos: 1, retain: false });
}

$("toggle").onclick = () => send("toggle");
$("on").onclick = () => send("on");
$("off").onclick = () => send("off");
