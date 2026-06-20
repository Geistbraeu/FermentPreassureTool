#include "cloud/ThingSpeakProvider.h"

#include "Settings.h"
#include "RuntimeState.h"
#include "config.h"
#include "debug.h"

extern RuntimeState runtimeState;

void ThingSpeakProvider::init() {
  client.setInsecure();
  lastSendMs = 0;
}

void ThingSpeakProvider::send(const CloudPayload& payload) {
  bool tsEnabled = false;
  bool useTempSensor = true;
  unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
  String tsApiKey;

  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    tsEnabled = settings.tsEnabled;
    useTempSensor = settings.useTempSensor;
    tsIntervalSeconds = settings.tsIntervalSeconds;
    tsApiKey = settings.tsApiKey;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  if (!tsEnabled || tsApiKey.isEmpty()) {
    return;
  }

  unsigned long nowMs = millis();
  unsigned long intervalMs = tsIntervalSeconds * 1000UL;
  if (!isIntervalElapsed(nowMs, intervalMs)) {
    return;
  }
  markSent(nowMs);

  client.stop();
  DBG("\n[ThingSpeak] connecting (HTTPS)...");
  if (client.connect(CloudConfig::THINGSPEAK_SERVER, CloudConfig::THINGSPEAK_PORT)) {
    String url = "/update?api_key=" + tsApiKey +
                 "&field1=" + String(payload.voltage, 3) +
                 "&field2=" + String(payload.pressurePsi, 2) +
                 "&field3=" + String(payload.pressureBar, 2);

    if (useTempSensor) {
      url += "&field4=" + String(payload.temperatureC, 2);
    }

    String request;
    request.reserve(220);
    request += "GET " + url + " HTTP/1.1\r\n";
    request += "Host: " + String(CloudConfig::THINGSPEAK_SERVER) + "\r\n";
    request += "Connection: close\r\n\r\n";

    client.print(request);
    DBG("[ThingSpeak] HTTPS request sent");
  } else {
    DBG("[ThingSpeak] HTTPS connection failed");
  }
}
