// Broker settings for the web page. EVERYTHING HERE IS PUBLIC once GitHub Pages serves it.
// Use the WEB PAGE's broker user (restricted by EMQX ACL), never the ESP's user.
window.APP_CONFIG = {
  // EMQX deployment overview -> Connection Information -> "WebSocket over TLS/SSL" (port 8084)
  brokerUrl: "wss://CHANGE_ME.emqxsl.com:8084/mqtt",
  username: "CHANGE_ME",
  password: "CHANGE_ME",
};
