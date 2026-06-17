#include <WebServer.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "web_server.h"

extern float currentVoltage;
extern float currentPressure;
extern SemaphoreHandle_t dataMutex;
extern float maxPressureThreshold;
extern float offsetVoltage;

WebServer server(80);

void handleRoot() {
    float p = 0.0, v = 0.0;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        p = currentPressure;
        v = currentVoltage;
        xSemaphoreGive(dataMutex);
    }
    
    String html = "<html><body>";
    html += "<h1>Pressure Sensor (" + String(HOSTNAME) + ")</h1>";
    html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    html += "<p>Pressure: " + String(p, 2) + " PSI</p>";
    html += "<p>Voltage: " + String(v, 2) + " V</p>";
    html += "<p>Max Pressure Threshold: " + String(maxPressureThreshold, 2) + " PSI</p>";
    html += "<p>Voltage Offset: " + String(offsetVoltage, 3) + " V</p>";
    html += "<form action='/api' method='POST'>";
    html += "New Max Pressure: <input type='number' step='0.1' name='pressure'><br>";
    html += "New Voltage Offset: <input type='number' step='0.001' name='offset'><br>";
    html += "<input type='submit' value='Set'>";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleApi() {
    if (server.method() == HTTP_GET) {
        float p = 0.0, v = 0.0;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            p = currentPressure;
            v = currentVoltage;
            xSemaphoreGive(dataMutex);
        }
        String json = "{\"pressure\":" + String(p, 2) + 
                      ",\"voltage\":" + String(v, 2) + 
                      ",\"maxPressure\":" + String(maxPressureThreshold, 2) + 
                      ",\"offsetVoltage\":" + String(offsetVoltage, 3) + "}";
        server.send(200, "application/json", json);
    } else if (server.method() == HTTP_POST) {
        if (server.hasArg("pressure")) {
            maxPressureThreshold = server.arg("pressure").toFloat();
            Preferences prefs;
            prefs.begin("config", false);
            prefs.putFloat("maxPressure", maxPressureThreshold);
            prefs.end();
        }
        if (server.hasArg("offset")) {
            offsetVoltage = server.arg("offset").toFloat();
            Preferences prefs;
            prefs.begin("config", false);
            prefs.putFloat("offsetVoltage", offsetVoltage);
            prefs.end();
        }
        // Redirect to root
        server.sendHeader("Location", "/", true);
        server.send(303, "text/plain", "OK");
    }
}

void webServerTask(void *pvParameters) {
    server.on("/", handleRoot);
    server.on("/api", handleApi);
    server.begin();
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initWebServer() {
    xTaskCreatePinnedToCore(
        webServerTask,
        "WebServerTask",
        4096,
        NULL,
        1,
        NULL,
        0 // Core 0
    );
}
