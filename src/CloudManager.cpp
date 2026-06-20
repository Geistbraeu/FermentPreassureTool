#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "CloudManager.h"
#include "Settings.h"
#include "RuntimeState.h"

extern RuntimeState runtimeState;

static unsigned long lastThingSpeakTime = 0;
static unsigned long lastBrewfatherTime = 0;
static unsigned long lastCustomHTTPTime = 0;

// SECURE CLIENT AND SERVER SETTINGS
WiFiClientSecure client;
WiFiClient httpClient;

void initCloud() {
  client.setInsecure();
  unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
  unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
  unsigned long httpIntervalSeconds = CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    tsIntervalSeconds = settings.tsIntervalSeconds;
    bfIntervalMinutes = settings.bfIntervalMinutes;
    httpIntervalSeconds = settings.httpIntervalSeconds;
    xSemaphoreGive(runtimeState.settingsMutex);
  }
  lastThingSpeakTime = millis() - (tsIntervalSeconds * 1000);
  lastBrewfatherTime = millis() - (bfIntervalMinutes * 60000);
  lastCustomHTTPTime = millis() - (httpIntervalSeconds * 1000);
}

void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp) {
  bool tsEnabled = false;
  unsigned long tsIntervalSeconds = CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC;
  bool useTempSensor = true;
  String tsApiKey;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    tsEnabled = settings.tsEnabled;
    tsIntervalSeconds = settings.tsIntervalSeconds;
    useTempSensor = settings.useTempSensor;
    tsApiKey = settings.tsApiKey;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  if (!tsEnabled) return;
  unsigned long currentMillis = millis();
  if (currentMillis - lastThingSpeakTime < (tsIntervalSeconds * 1000)) {
    return;
  }
  lastThingSpeakTime = currentMillis;

  client.stop();
  Serial.println("\n[ThingSpeak] Подключение к серверу (HTTPS)...");
  if (client.connect(CloudConfig::THINGSPEAK_SERVER, CloudConfig::THINGSPEAK_PORT)) {
    Serial.print("[ThingSpeak] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P(PSI): "); Serial.print(pressure);
    Serial.print(", P(Bar): "); Serial.print(pressureBar); 
    if (useTempSensor) {
      Serial.print(" Bar, T: "); Serial.print(temp); Serial.println(" C");
    } else {
      Serial.println(" Bar");
    }

    String url = "/update?api_key=" + tsApiKey + 
                 "&field1=" + String(voltage, 3) + 
                 "&field2=" + String(pressure, 2) +
                 "&field3=" + String(pressureBar, 2);
    
    if (useTempSensor) {
        url += "&field4=" + String(temp, 2);
    }

    String httpRequest;
    httpRequest.reserve(200);
    httpRequest += "GET " + url + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(CloudConfig::THINGSPEAK_SERVER) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";

    client.print(httpRequest);
    Serial.println("[ThingSpeak] HTTPS запрос успешно отправлен.");
  } else {
    Serial.println("[ThingSpeak] Ошибка HTTPS подключения (порт 443).");
  }
}

void sendDataToBrewfather(float voltage, float pressure, float temp) {
  bool bfEnabled = false;
  unsigned long bfIntervalMinutes = CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN;
  bool useTempSensor = true;
  String bfDeviceName;
  String bfStreamId;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    bfEnabled = settings.bfEnabled;
    bfIntervalMinutes = settings.bfIntervalMinutes;
    useTempSensor = settings.useTempSensor;
    bfDeviceName = settings.bfDeviceName;
    bfStreamId = settings.bfStreamId;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  if (!bfEnabled) return;
  unsigned long currentMillis = millis();
  if (currentMillis - lastBrewfatherTime < (bfIntervalMinutes * 60000)) {
    return;
  }
  lastBrewfatherTime = currentMillis;

  client.stop();
  
  Serial.println("\n[Brewfather] Подключение к серверу для POST...");

  if (client.connect(CloudConfig::BREWFATHER_SERVER, CloudConfig::BREWFATHER_PORT)) {
    Serial.print("[Brewfather] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P: "); Serial.print(pressure); 
    if (useTempSensor) {
        Serial.print(" PSI, T: "); Serial.print(temp); Serial.println(" C");
    } else {
        Serial.println(" PSI");
    }

    Serial.println("[Brewfather] HTTPS Подключено! Формирование JSON пакета...");

    // 1. Формируем тело JSON (с резервированием памяти во избежание фрагментации кучи)
    String jsonBody;
    jsonBody.reserve(200);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + bfDeviceName + "\",";
    jsonBody += "\"pressure\":" + String(pressure, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    if (useTempSensor) {
        jsonBody += "\"temp\":" + String(temp, 2) + ",";
        jsonBody += "\"temp_unit\":\"C\",";
    }
    jsonBody += "\"battery\":" + String(voltage, 2) + ",";
    jsonBody += "\"comment\":\"Voltage: " + String(voltage, 2) + "V";
    if (useTempSensor) {
        jsonBody += ", Temp: " + String(temp, 2) + "C";
    }
    jsonBody += "\"";
    jsonBody += "}";
    
    Serial.println("[Brewfather] Sending JSON: " + jsonBody);
    
    int contentLength = jsonBody.length();

    String httpRequest;
    httpRequest.reserve(300);
    httpRequest += "POST /stream?id=" + bfStreamId + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(CloudConfig::BREWFATHER_SERVER) + "\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += "Content-Length: " + String(contentLength) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";
    httpRequest += jsonBody + "\r\n";

    client.print(httpRequest);
    client.flush(); // Гарантируем отправку буфера из памяти ESP32

    Serial.println("[Brewfather] JSON POST запрос успешно отправлен.");
  } else {
    Serial.println("[Brewfather] Ошибка HTTPS подключения (порт 443).");
  }
}

String replacePlaceholders(String text, float voltage, float pressure, float pressureBar, float temp) {
  String result = text;
  result.replace("{volt}", String(voltage, 3));
  result.replace("{psi}", String(pressure, 3));
  result.replace("{bar}", String(pressureBar, 3));
  result.replace("{temp}", String(temp, 3));
  return result;
}

void sendDataViaCustomHTTP(float voltage, float pressure, float pressureBar, float temp) {
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

  unsigned long currentMillis = millis();
  if (currentMillis - lastCustomHTTPTime < (httpIntervalSeconds * 1000)) {
    return;
  }
  lastCustomHTTPTime = currentMillis;

  String serverInput = httpServer;
  String pathInput = httpPath;
  if (pathInput.length() == 0) {
    pathInput = "/";
  }

  String host = serverInput;
  int port = 80;
  if (host.startsWith("http://")) {
    host.remove(0, 7);
  } else if (host.startsWith("https://")) {
    host.remove(0, 8);
    port = 443;
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

  String finalPath = replacePlaceholders(pathInput, voltage, pressure, pressureBar, temp);
  String finalBody = replacePlaceholders(httpBodyTemplate, voltage, pressure, pressureBar, temp);

  httpClient.stop();
  Serial.println("\n[HTTP] Подключение к серверу...");

  if (port == 443) {
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    if (httpsClient.connect(host.c_str(), port)) {
      Serial.print("[HTTP] Отправка POST -> ");
      Serial.println(finalPath);
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
      httpsClient.print(request);
      httpsClient.flush();
      Serial.println("[HTTP] POST запрос успешно отправлен.");
    } else {
      Serial.println("[HTTP] Ошибка подключения к HTTPS серверу.");
    }
  } else {
    if (httpClient.connect(host.c_str(), port)) {
      Serial.print("[HTTP] Отправка POST -> ");
      Serial.println(finalPath);
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
      httpClient.print(request);
      httpClient.flush();
      Serial.println("[HTTP] POST запрос успешно отправлен.");
    } else {
      Serial.println("[HTTP] Ошибка подключения к HTTP серверу.");
    }
  }
}
