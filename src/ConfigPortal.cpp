#include "ConfigPortal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;
extern bool isOledConnected;

static WebServer portalServer(80);
static DNSServer dnsServer;

static void handleRoot() {
    String html = "<html><body>"
                  "<h1>Fermenter Setup</h1>"
                  "<form action='/save' method='POST'>"
                  "Device Name: <input type='text' name='devName'><br>"
                  "SSID: <input type='text' name='ssid'><br>"
                  "Password: <input type='password' name='pass'><br>"
                  "<input type='submit' value='Save'>"
                  "</form></body></html>";
    portalServer.send(200, "text/html", html);
}

static void handleSave() {
    if (portalServer.hasArg("ssid") && portalServer.hasArg("pass") && portalServer.hasArg("devName")) {
        Preferences prefs;
        prefs.begin("wifi_conf", false);
        prefs.putString("ssid", portalServer.arg("ssid"));
        prefs.putString("pass", portalServer.arg("pass"));
        prefs.putString("devName", portalServer.arg("devName"));
        prefs.end();

        portalServer.send(200, "text/html", "Settings saved. Restarting...");
        delay(1000);
        ESP.restart();
    } else {
        portalServer.send(400, "text/plain", "Invalid arguments");
    }
}

bool ConfigPortal::connect() {
    Preferences prefs;
    prefs.begin("wifi_conf", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    String devName = prefs.getString("devName", "ferment01");
    prefs.end();

    if (ssid == "") return false;

    WiFi.disconnect();
    WiFi.setHostname(devName.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    // Wait up to 10 seconds for connection
    for (int i = 0; i < 20; i++) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[Wi-Fi] Подключено: " + String(WiFi.localIP().toString()));
            return true;
        }
        delay(500);
        Serial.print(".");
    }
    
    return false;
}

bool ConfigPortal::begin() {
    if (!connect()) {
        startSetupMode();
        return false;
    }
    return true;
}

void ConfigPortal::startSetupMode() {
    WiFi.softAP("Ferment_Setup");
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    portalServer.on("/", handleRoot);
    portalServer.on("/save", handleSave);
    portalServer.onNotFound(handleRoot);
    portalServer.begin();

    if (isOledConnected) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("AP Mode: Ferment_Setup");
        display.println("Connect and config");
        display.display();
    }

    unsigned long startMillis = millis();
    while (millis() - startMillis < 300000) { // 5 minutes
        dnsServer.processNextRequest();
        portalServer.handleClient();
        delay(10);
    }

    // Timeout
    WiFi.softAPdisconnect(true);
    portalServer.stop();
    
    if (isOledConnected) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("No Network");
        display.display();
    }
    
    while (true) {
        delay(1000);
    }
}
