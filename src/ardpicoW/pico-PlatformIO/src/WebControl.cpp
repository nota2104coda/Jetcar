#include "WebControl.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "pico/cyw43_arch.h"

// WiFi credentials
const char* WIFI_SSID = "yourRouterName";
const char* WIFI_PASSWORD = "yourWifiPasswd";
#define WIFI_CONNECT_TIMEOUT_MS 30000

// Web server on port 80 and WebSockets on port 81
WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

static uint32_t wifi_start_time = 0;
static bool wifi_init_started = false;
static bool server_started = false;
static uint32_t wifi_last_status_log = 0;
static bool wifi_scan_started = false;
static uint32_t wifi_scan_start_time = 0;
static bool wifi_scan_reported = false;
static bool wifi_begin_issued = false;

// Control mode: 0=Web, 1=UART
static volatile uint8_t controlMode = 0;  
// Robot enabled state
static volatile bool robotEnabled = true;

// HTML page with control interface that uses WebSockets
const char CONTROL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>PicoW Robot Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }
        h1 { color: #333; }
        .status-box { margin: 20px 0; padding: 10px; background: #fff; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .controls { margin: 20px auto; max-width: 300px; }
        button { 
            width: 100%; height: 60px; margin: 5px 0; font-size: 18px; 
            border: none; border-radius: 5px; cursor: pointer;
            background: #4CAF50; color: white; transition: background 0.3s;
        }
        button:active { background: #45a049; }
        .dir-btn { background: #2196F3; }
        .dir-btn:active { background: #0b7dda; }
        .stop-btn { background: #f44336; }
        .stop-btn:active { background: #da190b; }
        .toggle-btn { background: #ff9800; }
        .toggle-btn:active { background: #e68900; }
        .row { display: flex; gap: 5px; }
        .row button { flex: 1; }
        #ws-status { font-weight: bold; }
        .connected { color: #4CAF50; }
        .disconnected { color: #f44336; }
    </style>
</head>
<body>
    <h1>PicoW Robot Control</h1>
    <div class="status-box">
        <div>WebSocket: <span id="ws-status" class="disconnected">Disconnected</span></div>
        <div>Mode: <span id="mode">Web Control</span></div>
        <div>Robot: <span id="enabled">Enabled</span></div>
    </div>
    <div class="controls">
        <button class="dir-btn" onmousedown="sendCmd('forward')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('forward')" ontouchend="sendCmd('stop')">Forward</button>
        <div class="row">
            <button class="dir-btn" onmousedown="sendCmd('left')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('left')" ontouchend="sendCmd('stop')">Left</button>
            <button class="stop-btn" onclick="sendCmd('stop')">STOP</button>
            <button class="dir-btn" onmousedown="sendCmd('right')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('right')" ontouchend="sendCmd('stop')">Right</button>
        </div>
        <button class="dir-btn" onmousedown="sendCmd('backward')" onmouseup="sendCmd('stop')" ontouchstart="sendCmd('backward')" ontouchend="sendCmd('stop')">Backward</button>
        <button class="toggle-btn" onclick="sendCmd('toggle_enable')">Toggle Enable/Disable</button>
        <button class="toggle-btn" onclick="sendCmd('toggle_mode')">Toggle Web/UART</button>
    </div>
    <script>
        var gateway = `ws://${window.location.hostname}:81/`;
        var websocket;
        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }
        function onOpen(event) {
            console.log('Connection opened');
            document.getElementById('ws-status').textContent = 'Connected';
            document.getElementById('ws-status').className = 'connected';
        }
        function onClose(event) {
            console.log('Connection closed');
            document.getElementById('ws-status').textContent = 'Disconnected';
            document.getElementById('ws-status').className = 'disconnected';
            setTimeout(initWebSocket, 2000);
        }
        function onMessage(event) {
            var data = JSON.parse(event.data);
            if (data.enabled !== undefined) {
                document.getElementById('enabled').textContent = data.enabled ? 'Enabled' : 'Disabled';
            }
            if (data.mode !== undefined) {
                document.getElementById('mode').textContent = data.mode === 0 ? 'Web Control' : 'UART Control';
            }
        }
        function sendCmd(action) {
            var msg = JSON.stringify({ "action": action });
            websocket.send(msg);
        }
        window.onload = initWebSocket;
        // Prevent default touch behavior
        document.querySelectorAll('button').forEach(btn => {
            btn.addEventListener('touchstart', (e) => e.preventDefault());
            btn.addEventListener('touchend', (e) => e.preventDefault());
        });
    </script>
</body>
</html>
)rawliteral";

static const char* wifi_status_to_string(wl_status_t status) {
    switch (status) {
        case WL_NO_SHIELD: return "WL_NO_SHIELD";
        case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
        case WL_CONNECTED: return "WL_CONNECTED";
        case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED: return "WL_DISCONNECTED";
        default: return "WL_UNKNOWN";
    }
}

bool is_robot_enabled() {
    return robotEnabled;
}

uint8_t get_control_mode() {
    return controlMode;
}

void handle_command(const String& action) {
    Serial.print("[WebControl] Command: ");
    Serial.println(action);
    
    // Movement commands
    if (robotEnabled && controlMode == 0) {
        if (action == "forward") {
            queue_motor_command(0.5f, 0.5f, 0.5f, 0.5f);
        } else if (action == "backward") {
            queue_motor_command(-0.5f, -0.5f, -0.5f, -0.5f);
        }
        else if (action == "left") {
            queue_motor_command(-0.3f, 0.3f, -0.3f, 0.3f);
        } else if (action == "right") {
            queue_motor_command(0.3f, -0.3f, 0.3f, -0.3f);
        } else if (action == "stop") {
            queue_motor_command(0.0f, 0.0f, 0.0f, 0.0f);
        }
    }
    
    // Toggle commands
    if (action == "toggle_enable") {
        robotEnabled = !robotEnabled;
        if (!robotEnabled) {
            queue_motor_command(0.0f, 0.0f, 0.0f, 0.0f);
        }
        Serial.print("[WebControl] Robot ");
        Serial.println(robotEnabled ? "ENABLED" : "DISABLED");
    } else if (action == "toggle_mode") {
        controlMode = (controlMode == 0) ? 1 : 0;
        Serial.print("[WebControl] Mode: ");
        Serial.println(controlMode == 0 ? "WEB" : "UART");
    }

    // Broadcast updated state to all clients
    JsonDocument doc;
    doc["enabled"] = (bool)robotEnabled;
    doc["mode"] = (int)controlMode;
    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
                // Send initial state
                JsonDocument doc;
                doc["enabled"] = (bool)robotEnabled;
                doc["mode"] = (int)controlMode;
                String output;
                serializeJson(doc, output);
                webSocket.sendTXT(num, output);
            }
            break;
        case WStype_TEXT:
            {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload);
                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    return;
                }
                if (doc.containsKey("action")) {
                    handle_command(doc["action"].as<String>());
                }
            }
            break;
    }
}

void start_web_control() {
    Serial.println("[WebControl] Starting WiFi connection...");
    Serial.print("[WebControl] SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("[WebControl] Status (before): ");
    Serial.println(wifi_status_to_string(static_cast<wl_status_t>(WiFi.status())));

    wifi_init_started = true;
    wifi_last_status_log = 0;
    wifi_begin_issued = false;
}

void process_web_clients() {
    // Required for Pico W CYW43 architecture if polling is enabled
    cyw43_arch_poll();

    if (wifi_init_started && !wifi_begin_issued) {
        Serial.println("[WebControl] WiFi.beginNoBlock()...");
        WiFi.beginNoBlock(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("[WebControl] WiFi.beginNoBlock() returned");
        wifi_start_time = millis();
        wifi_begin_issued = true;
    }

    if (!wifi_scan_started) {
        Serial.println("[WebControl] Scanning for WiFi networks...");
        WiFi.scanNetworks(true);
        wifi_scan_started = true;
        wifi_scan_start_time = millis();
        wifi_scan_reported = false;
    }

    if (wifi_scan_started && !wifi_scan_reported) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            if (n == 0) {
                Serial.println("[WebControl] No networks found");
            } else {
                Serial.print("[WebControl] Networks found: ");
                Serial.println(n);
                bool ssidFound = false;
                for (int i = 0; i < n; ++i) {
                    Serial.print("  ");
                    Serial.print(i + 1);
                    Serial.print(": ");
                    Serial.print(WiFi.SSID(i));
                    Serial.print(" (RSSI: ");
                    Serial.print(WiFi.RSSI(i));
                    Serial.print(" dBm, ch ");
                    Serial.print(WiFi.channel(i));
                    Serial.println(")");
                    if (WiFi.SSID(i) == WIFI_SSID) {
                        ssidFound = true;
                    }
                }
                Serial.print("[WebControl] Target SSID visible: ");
                Serial.println(ssidFound ? "YES" : "NO");
            }
            WiFi.scanDelete();
            wifi_scan_reported = true;
        } else if ((millis() - wifi_scan_start_time) > 8000) {
            Serial.println("[WebControl] WiFi scan timeout");
            WiFi.scanDelete();
            wifi_scan_reported = true;
        }
    }

    if (wifi_init_started && !server_started) {
        if ((millis() - wifi_last_status_log) > 1000) {
            wifi_last_status_log = millis();
            Serial.print("[WebControl] WiFi status: ");
            Serial.print(wifi_status_to_string(static_cast<wl_status_t>(WiFi.status())));
            Serial.print(" RSSI: ");
            Serial.println(WiFi.RSSI());
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WebControl] WiFi connected!");
            Serial.print("[WebControl] IP address: ");
            Serial.println(WiFi.localIP());
            
            server.begin();
            webSocket.begin();
            webSocket.onEvent(onWebSocketEvent);
            
            Serial.println("[WebControl] HTTP server started on port 80");
            Serial.println("[WebControl] WebSocket server started on port 81");
            server_started = true;
        } else if ((millis() - wifi_start_time) > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("\n[WebControl] WiFi connection timeout");
            server_started = true; 
        }
        return;
    }
    
    if (!server_started) return;
    
    webSocket.loop();
    
    WiFiClient client = server.accept();
    if (client) {
        String request = client.readStringUntil('\r');
        client.flush();
        if (request.indexOf("GET / ") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.print(CONTROL_PAGE);
        }
        client.stop();
    }
}

