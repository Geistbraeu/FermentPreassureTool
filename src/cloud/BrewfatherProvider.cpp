#include "cloud/BrewfatherProvider.h"

#include "Settings.h"
#include "RuntimeState.h"
#include "config.h"
#include "debug.h"

extern RuntimeState runtimeState;

void BrewfatherProvider::init() {
  client.setInsecure();
  lastSendMs = 0;
}

void BrewfatherProvider::send(const CloudPayload& payload) {
  bool bfEnabled = false;
  bool useTempSensor = true;
  unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
  String bfStreamId;
  String bfDeviceName;

  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    bfEnabled = settings.bfEnabled;
    useTempSensor = settings.useTempSensor;
    bfIntervalMinutes = settings.bfIntervalMinutes;
    bfStreamId = settings.bfStreamId;
    bfDeviceName = settings.bfDeviceName;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  if (!bfEnabled || bfStreamId.isEmpty()) {
    return;
  }

  unsigned long nowMs = millis();
  unsigned long intervalMs = bfIntervalMinutes * 60000UL;
  if (!isIntervalElapsed(nowMs, intervalMs)) {
    return;
  }
  markSent(nowMs);

  client.stop();
  DBG("\n[Brewfather] connecting (HTTPS)...");

  if (client.connect(CloudConfig::BREWFATHER_SERVER, CloudConfig::BREWFATHER_PORT)) {
    String jsonBody;
    jsonBody.reserve(220);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + bfDeviceName + "\",";
    jsonBody += "\"pressure\":" + String(payload.pressurePsi, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    if (useTempSensor) {
      jsonBody += "\"temp\":" + String(payload.temperatureC, 2) + ",";
      jsonBody += "\"temp_unit\":\"C\",";
    }
    jsonBody += "\"battery\":" + String(payload.voltage, 2) + ",";
    jsonBody += "\"comment\":\"Voltage: " + String(payload.voltage, 2) + "V";
    if (useTempSensor) {
      jsonBody += ", Temp: " + String(payload.temperatureC, 2) + "C";
    }
    jsonBody += "\"}";

    String request;
    request.reserve(340);
    request += "POST /stream?id=" + bfStreamId + " HTTP/1.1\r\n";
    request += "Host: " + String(CloudConfig::BREWFATHER_SERVER) + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + String(jsonBody.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += jsonBody + "\r\n";

    client.print(request);
    client.flush();
    DBG("[Brewfather] JSON POST sent");
  } else {
    DBG("[Brewfather] HTTPS connection failed");
  }
}
