#include <WebServer.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "config.h"
#include "validation.h"
#include "web_server.h"
#include "html_template.h"
#include "Settings.h"
#include "RuntimeState.h"

extern RuntimeState runtimeState;

WebServer server(80);

void handleRoot() {
    float p = 0.0, v = 0.0;
    bool mOverride = false, mOn = false;
    unsigned long mStart = 0;
    if (xSemaphoreTake(runtimeState.dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        p = runtimeState.currentPressure;
        v = runtimeState.currentVoltage;
        mOverride = runtimeState.manualOverride;
        mOn = runtimeState.manualOn;
        mStart = runtimeState.manualStartTime;
        xSemaphoreGive(runtimeState.dataMutex);
    }
    
    server.send(200, "text/html", getHtml(p, p * 0.0689476, v, mOverride, mOn, mStart,
        settings.maxPressureThreshold, settings.pressureUnit, settings.hysteresis,
        settings.updateIntervalMs, settings.medianSampleCount, settings.medianSampleDelayMs,
        settings.tsIntervalSeconds, settings.bfIntervalMinutes,
        settings.offsetVoltage, settings.tempOffset, settings.useTempSensor,
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
        if (xSemaphoreTake(runtimeState.dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            p = runtimeState.currentPressure;
            v = runtimeState.currentVoltage;
            mOverride = runtimeState.manualOverride;
            mOn = runtimeState.manualOn;
            mStart = runtimeState.manualStartTime;
            xSemaphoreGive(runtimeState.dataMutex);
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
        String errors = "";
        bool hasErrors = false;
        
        if (server.hasArg("cmd")) {
            String cmd = server.arg("cmd");
            if (cmd == "manual_on") {
                runtimeState.manualOverride = true;
                runtimeState.manualOn = true;
                runtimeState.manualStartTime = millis();
            } else if (cmd == "manual_off") {
                runtimeState.manualOverride = false;
            }
        }
        
        if (server.hasArg("pressure")) {
            float val = server.arg("pressure").toFloat();
            if (!Validation::isValidPressure(val)) {
                errors += "pressure invalid range (0.5-25.0),";
                hasErrors = true;
            } else {
                settings.setMaxPressureThreshold(val);
            }
        }
        
        if (server.hasArg("devName")) {
            String newDevName = server.arg("devName");
            if (!Validation::isValidHostname(newDevName)) {
                errors += "devName invalid,";
                hasErrors = true;
            } else {
                wifiSettings.save(wifiSettings.ssid, wifiSettings.pass, newDevName);
                WiFi.setHostname(newDevName.c_str());
            }
        }
        
        if (server.hasArg("pUnit")) {
            int val = server.arg("pUnit").toInt();
            if (!Validation::isValidPressureUnit(val)) {
                errors += "pUnit must be 0 or 1,";
                hasErrors = true;
            } else {
                settings.setPressureUnit(val);
            }
        }
        
        if (server.hasArg("hysteresis")) {
            float val = server.arg("hysteresis").toFloat();
            if (!Validation::isValidHysteresis(val)) {
                errors += "hysteresis must be 0-2.0,";
                hasErrors = true;
            } else {
                settings.setHysteresis(val);
            }
        }
        
        if (server.hasArg("updateInterval")) {
            unsigned long val = server.arg("updateInterval").toInt();
            if (!Validation::isValidUpdateInterval(val)) {
                errors += "updateInterval must be 50-5000ms,";
                hasErrors = true;
            } else {
                settings.setUpdateIntervalMs(val);
            }
        }
        
        if (server.hasArg("medianSampleCount")) {
            unsigned int val = server.arg("medianSampleCount").toInt();
            if (!Validation::isValidMedianSampleCount(val)) {
                errors += "medianCount must be odd 3-31,";
                hasErrors = true;
            } else {
                settings.setMedianSampleCount(val);
            }
        }
        
        if (server.hasArg("medianSampleDelay")) {
            unsigned long val = server.arg("medianSampleDelay").toInt();
            if (!Validation::isValidMedianSampleDelay(val)) {
                errors += "medianDelay must be 1-1000ms,";
                hasErrors = true;
            } else {
                settings.setMedianSampleDelayMs(val);
            }
        }
        
        if (server.hasArg("tsInterval")) {
            unsigned long val = server.arg("tsInterval").toInt();
            if (!Validation::isValidTsInterval(val)) {
                errors += "tsInterval must be 15-3600sec,";
                hasErrors = true;
            } else {
                settings.setTsIntervalSeconds(val);
            }
        }
        
        if (server.hasArg("bfInterval")) {
            unsigned long val = server.arg("bfInterval").toInt();
            if (!Validation::isValidBfInterval(val)) {
                errors += "bfInterval must be 5-1440min,";
                hasErrors = true;
            } else {
                settings.setBfIntervalMinutes(val);
            }
        }
        
        if (server.hasArg("offset")) {
            float val = server.arg("offset").toFloat();
            if (!Validation::isValidOffsetVoltage(val)) {
                errors += "offset must be 0-4.5V,";
                hasErrors = true;
            } else {
                settings.setOffsetVoltage(val);
            }
        }
        
        if (server.hasArg("tempOffset")) {
            float val = server.arg("tempOffset").toFloat();
            if (!Validation::isValidTempOffset(val)) {
                errors += "tempOffset must be -50 to +50C,";
                hasErrors = true;
            } else {
                settings.setTempOffset(val);
            }
        }
        
        if (server.hasArg("useTemp")) {
            settings.setUseTempSensor(server.arg("useTemp").toInt() == 1);
        }
        
        if (server.hasArg("tsApiKey")) {
            String val = server.arg("tsApiKey");
            if (!Validation::isValidApiKey(val)) {
                errors += "tsApiKey too long,";
                hasErrors = true;
            } else {
                settings.setTsApiKey(val);
            }
        }
        
        if (server.hasArg("bfStreamId")) {
            String val = server.arg("bfStreamId");
            if (!Validation::isValidApiKey(val)) {
                errors += "bfStreamId too long,";
                hasErrors = true;
            } else {
                settings.setBfStreamId(val);
            }
        }
        
        if (server.hasArg("bfDeviceName")) {
            String val = server.arg("bfDeviceName");
            if (!Validation::isValidDeviceName(val)) {
                errors += "bfDeviceName invalid,";
                hasErrors = true;
            } else {
                settings.setBfDeviceName(val);
            }
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
            String val = server.arg("httpServer");
            if (!Validation::isValidHttpServer(val)) {
                errors += "httpServer too long,";
                hasErrors = true;
            } else {
                settings.setHttpServer(val);
            }
        }
        
        if (server.hasArg("httpPath")) {
            String val = server.arg("httpPath");
            if (!Validation::isValidHttpPath(val)) {
                errors += "httpPath invalid,";
                hasErrors = true;
            } else {
                settings.setHttpPath(val);
            }
        }
        
        if (server.hasArg("httpBodyTemplate")) {
            String val = server.arg("httpBodyTemplate");
            if (!Validation::isValidHttpBodyTemplate(val)) {
                errors += "httpBody too long,";
                hasErrors = true;
            } else {
                settings.setHttpBodyTemplate(val);
            }
        }
        
        if (server.hasArg("httpInterval")) {
            unsigned long val = server.arg("httpInterval").toInt();
            if (!Validation::isValidCustomHttpInterval(val)) {
                errors += "httpInterval must be 15-3600sec,";
                hasErrors = true;
            } else {
                settings.setHttpIntervalSeconds(val);
            }
        }
        
        if (hasErrors) {
            String response = "{\"success\":false,\"errors\":\"" + errors + "\"}";
            server.send(400, "application/json", response);
        } else {
            server.sendHeader("Location", "/", true);
            server.send(303, "text/plain", "OK");
        }
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
