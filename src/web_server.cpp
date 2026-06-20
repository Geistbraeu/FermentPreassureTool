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
    float maxPressureThreshold = 0.0f;
    int pressureUnit = 0;
    float hysteresis = 0.0f;
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    unsigned int medianSampleCount = ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT;
    unsigned long medianSampleDelayMs = ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS;
    unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
    unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
    float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
    float tempOffset = 0.0f;
    bool useTempSensor = true;
    String tsApiKey;
    String bfStreamId;
    String bfDeviceName;
    bool tsEnabled = false;
    bool bfEnabled = false;
    bool httpEnabled = false;
    String httpServer;
    String httpPath = "/";
    String httpBodyTemplate;
    unsigned long httpIntervalSeconds = CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC;
    String devName = wifiSettings.devName;

    if (xSemaphoreTake(runtimeState.dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        p = runtimeState.currentPressure;
        v = runtimeState.currentVoltage;
        mOverride = runtimeState.manualOverride;
        mOn = runtimeState.manualOn;
        mStart = runtimeState.manualStartTime;
        xSemaphoreGive(runtimeState.dataMutex);
    }

    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
        maxPressureThreshold = settings.maxPressureThreshold;
        pressureUnit = settings.pressureUnit;
        hysteresis = settings.hysteresis;
        updateIntervalMs = settings.updateIntervalMs;
        medianSampleCount = settings.medianSampleCount;
        medianSampleDelayMs = settings.medianSampleDelayMs;
        tsIntervalSeconds = settings.tsIntervalSeconds;
        bfIntervalMinutes = settings.bfIntervalMinutes;
        offsetVoltage = settings.offsetVoltage;
        tempOffset = settings.tempOffset;
        useTempSensor = settings.useTempSensor;
        tsApiKey = settings.tsApiKey;
        bfStreamId = settings.bfStreamId;
        bfDeviceName = settings.bfDeviceName;
        tsEnabled = settings.tsEnabled;
        bfEnabled = settings.bfEnabled;
        httpEnabled = settings.httpEnabled;
        httpServer = settings.httpServer;
        httpPath = settings.httpPath;
        httpBodyTemplate = settings.httpBodyTemplate;
        httpIntervalSeconds = settings.httpIntervalSeconds;
        devName = wifiSettings.devName;
        xSemaphoreGive(runtimeState.settingsMutex);
    }
    
    server.send(200, "text/html", getHtml(p, p * SensorConfig::PSI_TO_BAR, v, mOverride, mOn, mStart,
        maxPressureThreshold, pressureUnit, hysteresis,
        updateIntervalMs, medianSampleCount, medianSampleDelayMs,
        tsIntervalSeconds, bfIntervalMinutes,
        offsetVoltage, tempOffset, useTempSensor,
        tsApiKey, bfStreamId, bfDeviceName,
        tsEnabled, bfEnabled,
        httpEnabled, httpServer, httpPath,
        httpBodyTemplate, httpIntervalSeconds,
        devName));
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
        
        float maxPressureThreshold = 0.0f;
        int pressureUnit = 0;
        float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
        bool useTempSensor = true;
        if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            maxPressureThreshold = settings.maxPressureThreshold;
            pressureUnit = settings.pressureUnit;
            offsetVoltage = settings.offsetVoltage;
            useTempSensor = settings.useTempSensor;
            xSemaphoreGive(runtimeState.settingsMutex);
        }

        String json = "{\"pressure\":" + String(p, 2) + 
                       ",\"voltage\":" + String(v, 2) + 
                       ",\"maxPressure\":" + String(maxPressureThreshold, 2) + 
                       ",\"pressureUnit\":" + String(pressureUnit) +
                       ",\"offsetVoltage\":" + String(offsetVoltage, 3) + 
                       ",\"useTempSensor\":" + (useTempSensor ? "true" : "false") +
                       ",\"manualOverride\":" + (mOverride ? "true" : "false") +
                       ",\"manualOn\":" + (mOn ? "true" : "false") +
                       ",\"remainingTime\":" + String(remaining) + "}";
        server.send(200, "application/json", json);
    } else if (server.method() == HTTP_POST) {
        String errors = "";
        bool hasErrors = false;

        auto appendSaveError = [&](const char* fieldName) {
            errors += String(fieldName) + " save failed,";
            hasErrors = true;
        };
        
        if (server.hasArg("cmd")) {
            String cmd = server.arg("cmd");
            if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (cmd == "manual_on") {
                    runtimeState.manualOverride = true;
                    runtimeState.manualOn = true;
                    runtimeState.manualStartTime = millis();
                } else if (cmd == "manual_off") {
                    runtimeState.manualOverride = false;
                }
                xSemaphoreGive(runtimeState.dataMutex);
            }
        }
        
        if (server.hasArg("pressure")) {
            float val = server.arg("pressure").toFloat();
            if (!Validation::isValidPressure(val)) {
                errors += "pressure invalid range (0.5-25.0),";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMaxPressureThreshold(val)) appendSaveError("pressure");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
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
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setPressureUnit(val)) appendSaveError("pUnit");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("hysteresis")) {
            float val = server.arg("hysteresis").toFloat();
            if (!Validation::isValidHysteresis(val)) {
                errors += "hysteresis must be 0-2.0,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHysteresis(val)) appendSaveError("hysteresis");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("updateInterval")) {
            unsigned long val = server.arg("updateInterval").toInt();
            if (!Validation::isValidUpdateInterval(val)) {
                errors += "updateInterval must be 50-5000ms,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setUpdateIntervalMs(val)) appendSaveError("updateInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("medianSampleCount")) {
            unsigned int val = server.arg("medianSampleCount").toInt();
            if (!Validation::isValidMedianSampleCount(val)) {
                errors += "medianCount must be odd 3-31,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMedianSampleCount(val)) appendSaveError("medianSampleCount");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("medianSampleDelay")) {
            unsigned long val = server.arg("medianSampleDelay").toInt();
            if (!Validation::isValidMedianSampleDelay(val)) {
                errors += "medianDelay must be 1-1000ms,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setMedianSampleDelayMs(val)) appendSaveError("medianSampleDelay");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("tsInterval")) {
            unsigned long val = server.arg("tsInterval").toInt();
            if (!Validation::isValidTsInterval(val)) {
                errors += "tsInterval must be 15-3600sec,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTsIntervalSeconds(val)) appendSaveError("tsInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("bfInterval")) {
            unsigned long val = server.arg("bfInterval").toInt();
            if (!Validation::isValidBfInterval(val)) {
                errors += "bfInterval must be 5-1440min,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfIntervalMinutes(val)) appendSaveError("bfInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("offset")) {
            float val = server.arg("offset").toFloat();
            if (!Validation::isValidOffsetVoltage(val)) {
                errors += "offset must be 0-4.5V,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setOffsetVoltage(val)) appendSaveError("offset");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("tempOffset")) {
            float val = server.arg("tempOffset").toFloat();
            if (!Validation::isValidTempOffset(val)) {
                errors += "tempOffset must be -50 to +50C,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTempOffset(val)) appendSaveError("tempOffset");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("useTemp")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setUseTempSensor(server.arg("useTemp").toInt() == 1)) appendSaveError("useTemp");
                xSemaphoreGive(runtimeState.settingsMutex);
            }
        }
        
        if (server.hasArg("tsApiKey")) {
            String val = server.arg("tsApiKey");
            if (!Validation::isValidApiKey(val)) {
                errors += "tsApiKey too long,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setTsApiKey(val)) appendSaveError("tsApiKey");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("bfStreamId")) {
            String val = server.arg("bfStreamId");
            if (!Validation::isValidApiKey(val)) {
                errors += "bfStreamId too long,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfStreamId(val)) appendSaveError("bfStreamId");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("bfDeviceName")) {
            String val = server.arg("bfDeviceName");
            if (!Validation::isValidDeviceName(val)) {
                errors += "bfDeviceName invalid,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setBfDeviceName(val)) appendSaveError("bfDeviceName");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("tsEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setTsEnabled(server.arg("tsEnabled").toInt() == 1)) appendSaveError("tsEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            }
        }
        
        if (server.hasArg("bfEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setBfEnabled(server.arg("bfEnabled").toInt() == 1)) appendSaveError("bfEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            }
        }
        
        if (server.hasArg("httpEnabled")) {
            if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                if (!settings.setHttpEnabled(server.arg("httpEnabled").toInt() == 1)) appendSaveError("httpEnabled");
                xSemaphoreGive(runtimeState.settingsMutex);
            }
        }
        
        if (server.hasArg("httpServer")) {
            String val = server.arg("httpServer");
            if (!Validation::isValidHttpServer(val)) {
                errors += "httpServer too long,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpServer(val)) appendSaveError("httpServer");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("httpPath")) {
            String val = server.arg("httpPath");
            if (!Validation::isValidHttpPath(val)) {
                errors += "httpPath invalid,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpPath(val)) appendSaveError("httpPath");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("httpBodyTemplate")) {
            String val = server.arg("httpBodyTemplate");
            if (!Validation::isValidHttpBodyTemplate(val)) {
                errors += "httpBody too long,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpBodyTemplate(val)) appendSaveError("httpBodyTemplate");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
            }
        }
        
        if (server.hasArg("httpInterval")) {
            unsigned long val = server.arg("httpInterval").toInt();
            if (!Validation::isValidCustomHttpInterval(val)) {
                errors += "httpInterval must be 15-3600sec,";
                hasErrors = true;
            } else {
                if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                    if (!settings.setHttpIntervalSeconds(val)) appendSaveError("httpInterval");
                    xSemaphoreGive(runtimeState.settingsMutex);
                }
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
