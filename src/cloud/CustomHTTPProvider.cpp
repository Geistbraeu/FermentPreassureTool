#include "cloud/CustomHTTPProvider.h"

#include <WiFiClientSecure.h>
#include "Settings.h"
#include "RuntimeState.h"
#include "config.h"
#include "debug.h"

extern RuntimeState runtimeState;

void CustomHTTPProvider::init() {
  lastSendMs = 0;
}

String CustomHTTPProvider::replacePlaceholders(String text, const CloudPayload& payload) const {
  text.replace("{volt}", String(payload.voltage, 3));
  text.replace("{psi}", String(payload.pressurePsi, 3));
  text.replace("{bar}", String(payload.pressureBar, 3));
  text.replace("{temp}", String(payload.temperatureC, 3));
  return text;
}

void CustomHTTPProvider::send(const CloudPayload& payload) {
  bool httpEnabled = false;
  String httpServer;
  String httpPath;
  String httpBodyTemplate;
  unsigned long httpIntervalSeconds = CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC;

  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    httpEnabled = settings.httpEnabled;
    httpServer = settings.httpServer;
    httpPath = settings.httpPath;
    httpBodyTemplate = settings.httpBodyTemplate;
    httpIntervalSeconds = settings.httpIntervalSeconds;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  if (!httpEnabled || httpServer.isEmpty()) {
    return;
  }

  unsigned long nowMs = millis();
  unsigned long intervalMs = httpIntervalSeconds * 1000UL;
  if (!isIntervalElapsed(nowMs, intervalMs)) {
    return;
  }
  markSent(nowMs);

  String host = httpServer;
  int port = CloudConfig::CUSTOM_HTTP_PORT_HTTP;

  if (host.startsWith("http://")) {
    host.remove(0, 7);
  } else if (host.startsWith("https://")) {
    host.remove(0, 8);
    port = CloudConfig::CUSTOM_HTTP_PORT_HTTPS;
  }

  int slashIndex = host.indexOf('/');
  if (slashIndex >= 0) {
    host = host.substring(0, slashIndex);
  }

  int colonIndex = host.indexOf(':');
  if (colonIndex >= 0) {
    String portStr = host.substring(colonIndex + 1);
    port = portStr.toInt();
    host = host.substring(0, colonIndex);
  }

  String path = httpPath.length() > 0 ? httpPath : "/";
  String finalPath = replacePlaceholders(path, payload);
  String finalBody = replacePlaceholders(httpBodyTemplate, payload);

  if (port == CloudConfig::CUSTOM_HTTP_PORT_HTTPS) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    if (!httpsClient.connect(host.c_str(), port)) {
      DBG("[HTTP] HTTPS connection failed");
      return;
    }

    String request = "POST " + finalPath + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    if (!finalBody.isEmpty()) {
      request += "Content-Type: application/json\r\n";
      request += "Content-Length: " + String(finalBody.length()) + "\r\n";
    }
    request += "Connection: close\r\n\r\n";
    if (!finalBody.isEmpty()) {
      request += finalBody;
    }

    DBG("[HTTP] HTTPS request:\n" + request);
    if (!finalBody.isEmpty()) {
      DBG("[HTTP] HTTPS body:\n" + finalBody);
    }
    httpsClient.print(request);
    httpsClient.flush();
    DBG("[HTTP] HTTPS POST sent");
    return;
  }

  httpClient.stop();
  if (!httpClient.connect(host.c_str(), port)) {
    DBG("[HTTP] HTTP connection failed");
    return;
  }

  String request = "POST " + finalPath + " HTTP/1.1\r\n";
  request += "Host: " + host + "\r\n";
  if (!finalBody.isEmpty()) {
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + String(finalBody.length()) + "\r\n";
  }
  request += "Connection: close\r\n\r\n";
  if (!finalBody.isEmpty()) {
    request += finalBody;
  }

  DBG("[HTTP] HTTP request:\n" + request);
  if (!finalBody.isEmpty()) {
    DBG("[HTTP] HTTP body:\n" + finalBody);
  }
  httpClient.print(request);
  httpClient.flush();
  DBG("[HTTP] HTTP POST sent");
}
