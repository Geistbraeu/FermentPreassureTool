#include <WebServer.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "web_server.h"
#include "html_template.h"
#include "Settings.h"

extern float currentVoltage;
extern float currentPressure;
extern SemaphoreHandle_t dataMutex;
extern bool manualOverride;
extern bool manualOn;
extern unsigned long manualStartTime;



WebServer server(80);

void handleRoot() {
    float p = 0.0, v = 0.0;
    bool mOverride = false, mOn = false;
    unsigned long mStart = 0;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        p = currentPressure;
        v = currentVoltage;
        mOverride = manualOverride;
        mOn = manualOn;
        mStart = manualStartTime;
        xSemaphoreGive(dataMutex);
    }
    
    server.send(200, "text/html", getHtml(p, p * 0.0689476, v, mOverride, mOn, mStart,
        settings.maxPressureThreshold, settings.pressureUnit, settings.hysteresis,
        settings.sensorInterval, settings.tsIntervalSeconds, settings.bfIntervalMinutes,
        settings.offsetVoltage, settings.useTempSensor,
        settings.tsApiKey, settings.bfStreamId, settings.bfDeviceName,
        settings.tsEnabled, settings.bfEnabled,
        settings.httpEnabled, settings.httpServer, settings.httpPath,
        settings.httpBodyTemplate, settings.httpIntervalSeconds,
        wifiSettings.devName));
}

void handleApi() {
    if (server.method() == HTTP_GET) {
        float p = 0.0, v = 0.0;
        bool mOverride = false, mOn = false;
        unsigned long mStart = 0;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            p = currentPressure;
            v = currentVoltage;
            mOverride = manualOverride;
            mOn = manualOn;
            mStart = manualStartTime;
            xSemaphoreGive(dataMutex);
        }
        long remaining = -1;
        if (mOverride) {
            remaining = 10000L - (long)(millis() - mStart);
            if (remaining < 0) remaining = 0;
        }
        
        String json = "{\"pressure\":" + String(p, 2) + 
                       ",\"voltage\":" + String(v, 2) + 
                       ",\"maxPressure\":" + String(settings.maxPressureThreshold, 2) + 
                       ",\"pressureUnit\":" + String(settings.pressureUnit) +
                       ",\"offsetVoltage\":" + String(settings.offsetVoltage, 3) + 
                       ",\"useTempSensor\":" + (settings.useTempSensor ? "true" : "false") +
                       ",\"manualOverride\":" + (mOverride ? "true" : "false") +
                       ",\"manualOn\":" + (mOn ? "true" : "false") +
                       ",\"remainingTime\":" + String(remaining) + "}";
        server.send(200, "application/json", json);
    } else if (server.method() == HTTP_POST) {
        if (server.hasArg("cmd")) {
            String cmd = server.arg("cmd");
            if (cmd == "manual_on") {
                manualOverride = true;
                manualOn = true;
                manualStartTime = millis();
            } else if (cmd == "manual_off") {
                manualOverride = false;
            }
        }
        if (server.hasArg("pressure")) {
            settings.setMaxPressureThreshold(server.arg("pressure").toFloat());
        }
        if (server.hasArg("devName")) {
            String newDevName = server.arg("devName");
            if (!newDevName.isEmpty()) {
                wifiSettings.save(wifiSettings.ssid, wifiSettings.pass, newDevName);
                WiFi.setHostname(newDevName.c_str());
            }
        }
        if (server.hasArg("pUnit")) {
            settings.setPressureUnit(server.arg("pUnit").toInt());
        }
        if (server.hasArg("hysteresis")) {
            settings.setHysteresis(server.arg("hysteresis").toFloat());
        }
        if (server.hasArg("sInterval")) {
            settings.setSensorInterval(server.arg("sInterval").toInt());
        }
        if (server.hasArg("tsInterval")) {
            unsigned long val = server.arg("tsInterval").toInt();
            if (val < 15) val = 15;
            settings.setTsIntervalSeconds(val);
        }
        if (server.hasArg("bfInterval")) {
            unsigned long val = server.arg("bfInterval").toInt();
            if (val < 15) val = 15;
            settings.setBfIntervalMinutes(val);
        }
        if (server.hasArg("offset")) {
            settings.setOffsetVoltage(server.arg("offset").toFloat());
        }
        if (server.hasArg("useTemp")) {
            settings.setUseTempSensor(server.arg("useTemp").toInt() == 1);
        }
        if (server.hasArg("tsApiKey")) {
            settings.setTsApiKey(server.arg("tsApiKey"));
        }
        if (server.hasArg("bfStreamId")) {
            settings.setBfStreamId(server.arg("bfStreamId"));
        }
        if (server.hasArg("bfDeviceName")) {
            settings.setBfDeviceName(server.arg("bfDeviceName"));
        }
        if (server.hasArg("tsEnabled")) {
            settings.setTsEnabled(server.arg("tsEnabled").toInt() == 1);
        }
        if (server.hasArg("bfEnabled")) {
            settings.setBfEnabled(server.arg("bfEnabled").toInt() == 1);
        }
        if (server.hasArg("httpEnabled")) {
            settings.setHttpEnabled(server.arg("httpEnabled").toInt() == 1);
        }
        if (server.hasArg("httpServer")) {
            settings.setHttpServer(server.arg("httpServer"));
        }
        if (server.hasArg("httpPath")) {
            settings.setHttpPath(server.arg("httpPath"));
        }
        if (server.hasArg("httpBodyTemplate")) {
            settings.setHttpBodyTemplate(server.arg("httpBodyTemplate"));
        }
        if (server.hasArg("httpInterval")) {
            unsigned long val = server.arg("httpInterval").toInt();
            if (val < 15) val = 15;
            settings.setHttpIntervalSeconds(val);
        }
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
