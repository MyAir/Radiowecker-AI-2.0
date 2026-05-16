#include "NetworkManager.h"
#include <algorithm>

static constexpr uint8_t  DNS_PORT   = 53;

// ---------------------------------------------------------------------------
// Embedded captive-portal HTML
// Network <option> elements are injected between PORTAL_HEAD and PORTAL_TAIL.
// ---------------------------------------------------------------------------
static const char PORTAL_HEAD[] =
R"html(<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radiowecker &#x2013; WiFi Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0f0f1a;color:#e0e0f0;
     min-height:100vh;display:flex;flex-direction:column;
     align-items:center;padding:40px 16px}
h1{font-size:1.6rem;color:#7eb3ff;margin-bottom:8px}
p.sub{color:#8888aa;margin-bottom:28px;font-size:.9rem}
form{width:100%;max-width:380px;background:#1a1a30;border-radius:12px;
     padding:24px;box-shadow:0 4px 24px rgba(0,0,0,.5)}
label{display:block;font-size:.82rem;color:#9999bb;margin-bottom:5px}
select,input[type=password]{width:100%;padding:10px 12px;margin-bottom:18px;
     border-radius:8px;border:1px solid #333;background:#0f0f1a;
     color:#e0e0f0;font-size:1rem}
select:focus,input:focus{outline:none;border-color:#7eb3ff}
button{width:100%;padding:12px;border:none;border-radius:8px;
       background:#3a7bd5;color:#fff;font-size:1rem;
       font-weight:600;cursor:pointer}
button:hover{background:#2e64b0}
</style></head><body>
<h1>&#x1F4F6; WiFi Setup</h1>
<p class="sub">Select your network and enter the password.</p>
<form method="POST" action="/save">
<label for="s">Network</label>
<select name="ssid" id="s">
)html";

static const char PORTAL_TAIL[] =
R"html(</select>
<label for="pw">Password</label>
<input type="password" name="password" id="pw"
       placeholder="Leave empty for open networks"
       autocomplete="current-password">
<button type="submit">Connect &amp; Save</button>
</form></body></html>)html";

static const char PORTAL_OK[] =
R"html(<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>
body{font-family:system-ui,sans-serif;background:#0f0f1a;color:#e0e0f0;
     display:flex;align-items:center;justify-content:center;
     min-height:100vh;padding:24px}
.card{background:#1a1a30;border-radius:12px;padding:32px;
      max-width:360px;text-align:center}
h1{color:#4caf50;margin-bottom:12px}
p{color:#8888aa;line-height:1.5}
</style></head><body>
<div class="card">
<h1>&#x2705; Saved!</h1>
<p>Credentials saved. The device will restart and connect to your network.</p>
</div></body></html>)html";

// ---------------------------------------------------------------------------
// public: connect
// ---------------------------------------------------------------------------
void WiFiConnector::connect(uint32_t timeoutMs) {
    String ssid, password;
    if (_loadCredentials(ssid, password) && ssid.length() > 0) {
        Serial.printf("[Network] Connecting to \"%s\" ...\n", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
            delay(250);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Network] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        const char* reason = (ssid.length() == 0) ? "no credentials on SD card"
                                                   : "connection timed out";
        Serial.printf("[Network] %s — starting captive portal\n", reason);
        _startPortal();
    }
}

// ---------------------------------------------------------------------------
// public: loop
// ---------------------------------------------------------------------------
void WiFiConnector::loop() {
    if (!_portalActive) return;

    if (_dns)    _dns->processNextRequest();
    if (_server) _server->handleClient();

    if (_restartAt && millis() >= _restartAt) {
        Serial.println("[Network] Restarting ...");
        ESP.restart();
    }
}

// ---------------------------------------------------------------------------
// public: accessors
// ---------------------------------------------------------------------------
bool WiFiConnector::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiConnector::localIP() const {
    return WiFi.localIP().toString();
}

// ---------------------------------------------------------------------------
// private: _loadCredentials — reads from SD_WIFI_FILE on SD card
// ---------------------------------------------------------------------------
bool WiFiConnector::_loadCredentials(String& ssid, String& password) {
    File f = SD.open(SD_WIFI_FILE, FILE_READ);
    if (!f) {
        Serial.println("[Network] " SD_WIFI_FILE " not found on SD card");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Network] JSON parse error: %s\n", err.c_str());
        return false;
    }

    ssid     = doc["ssid"]     | "";
    password = doc["password"] | "";
    return true;
}

// ---------------------------------------------------------------------------
// private: _saveCredentials — writes ssid + password to SD_WIFI_FILE
// ---------------------------------------------------------------------------
bool WiFiConnector::_saveCredentials(const String& ssid, const String& password) {
    File f = SD.open(SD_WIFI_FILE, "w");
    if (!f) {
        Serial.println("[Network] Cannot write " SD_WIFI_FILE " to SD card");
        return false;
    }

    JsonDocument doc;
    doc["ssid"]     = ssid;
    doc["password"] = password;
    serializeJson(doc, f);
    f.close();

    Serial.printf("[Network] Credentials saved for \"%s\"\n", ssid.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// private: _buildNetworkOptions — <option> list from cached scan results
// ---------------------------------------------------------------------------
void WiFiConnector::_buildNetworkOptions(String& out) {
    out = "";
    for (const auto& net : _networks) {
        // HTML-escape SSID for safe embedding in attribute value and text
        String esc = net.ssid;
        esc.replace("&",  "&amp;");
        esc.replace("\"", "&quot;");
        esc.replace("<",  "&lt;");
        esc.replace(">",  "&gt;");
        out += "<option value=\"" + esc + "\">" + esc
            + " (" + String(net.rssi) + " dBm)</option>\n";
    }
    if (out.isEmpty()) {
        out = "<option value=''>No networks found</option>\n";
    }
}

// ---------------------------------------------------------------------------
// private: _startPortal
// ---------------------------------------------------------------------------
void WiFiConnector::_startPortal() {
    // Scan while still in STA mode for best results
    Serial.println("[Network] Scanning for WiFi networks ...");
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    _networks.clear();

    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        bool dup = false;
        for (const auto& e : _networks) {
            if (e.ssid == ssid) { dup = true; break; }
        }
        if (!dup) _networks.push_back({ssid, WiFi.RSSI(i)});
    }
    WiFi.scanDelete();

    // Sort by signal strength, strongest first
    std::sort(_networks.begin(), _networks.end(),
        [](const NetworkEntry& a, const NetworkEntry& b) { return a.rssi > b.rssi; });

    Serial.printf("[Network] Found %d unique networks\n", (int)_networks.size());

    // Start SoftAP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_AP_SSID);
    delay(100);

    const IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    Serial.printf("[Network] AP \"%s\" up at %s\n", WIFI_AP_SSID, apIP.toString().c_str());

    // DNS: send every hostname to the portal
    _dns = new DNSServer();
    _dns->start(DNS_PORT, "*", apIP);

    // HTTP server
    _server = new WebServer(80);

    // Main portal page
    _server->on("/", HTTP_GET, [this]() {
        String options;
        _buildNetworkOptions(options);
        String page;
        page.reserve(strlen(PORTAL_HEAD) + options.length() + strlen(PORTAL_TAIL));
        page += PORTAL_HEAD;
        page += options;
        page += PORTAL_TAIL;
        _server->send(200, "text/html", page);
    });

    // Save credentials and reboot
    _server->on("/save", HTTP_POST, [this]() {
        String ssid     = _server->arg("ssid");
        String password = _server->arg("password");

        if (ssid.isEmpty()) {
            _server->send(400, "text/plain", "SSID is required");
            return;
        }
        if (_saveCredentials(ssid, password)) {
            _server->send(200, "text/html", PORTAL_OK);
            _restartAt = millis() + 2000;
        } else {
            _server->send(500, "text/plain", "Failed to save — check SD card");
        }
    });

    // Captive-portal detection: redirect all probe URLs to the portal page
    auto doRedirect = [this]() {
        _server->sendHeader("Location", "http://192.168.4.1/");
        _server->send(302, "text/plain", "");
    };
    _server->on("/generate_204",        HTTP_GET, doRedirect);  // Android
    _server->on("/hotspot-detect.html", HTTP_GET, doRedirect);  // iOS / macOS
    _server->on("/ncsi.txt",            HTTP_GET, doRedirect);  // Windows
    _server->on("/connecttest.txt",     HTTP_GET, doRedirect);  // Windows
    _server->on("/redirect",            HTTP_GET, doRedirect);
    _server->onNotFound([this]() {
        _server->sendHeader("Location", "http://192.168.4.1/");
        _server->send(302, "text/plain", "");
    });

    _server->begin();
    _portalActive = true;
    Serial.println("[Network] Captive portal active."
                   " Connect to \"" WIFI_AP_SSID "\" and open http://192.168.4.1");
}

