#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "CloudManager.h"

#include "Settings.h"

static unsigned long lastThingSpeakTime = 0;
static unsigned long lastBrewfatherTime = 0;

// SECURE CLIENT AND SERVER SETTINGS
WiFiClientSecure client;

const char* tsServer = "api.thingspeak.com";
const char* bfServer = "log.brewfather.net";

void initCloud() {
  client.setInsecure();
  lastThingSpeakTime = millis() - (settings.tsIntervalSeconds * 1000);
  lastBrewfatherTime = millis() - (settings.bfIntervalMinutes * 60000);
}

void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp) {
  if (!settings.tsEnabled) return;
  unsigned long currentMillis = millis();
  if (currentMillis - lastThingSpeakTime < (settings.tsIntervalSeconds * 1000)) {
    return;
  }
  lastThingSpeakTime = currentMillis;

  client.stop();
  Serial.println("\n[ThingSpeak] Подключение к серверу (HTTPS)...");
  if (client.connect(tsServer, 443)) {
    Serial.print("[ThingSpeak] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P(PSI): "); Serial.print(pressure);
    Serial.print(", P(Bar): "); Serial.print(pressureBar); 
    if (settings.useTempSensor) {
      Serial.print(" Bar, T: "); Serial.print(temp); Serial.println(" C");
    } else {
      Serial.println(" Bar");
    }

    String url = "/update?api_key=" + settings.tsApiKey + 
                 "&field1=" + String(voltage, 3) + 
                 "&field2=" + String(pressure, 2) +
                 "&field3=" + String(pressureBar, 2);
    
    if (settings.useTempSensor) {
        url += "&field4=" + String(temp, 2);
    }

    String httpRequest;
    httpRequest.reserve(200);
    httpRequest += "GET " + url + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(tsServer) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";

    client.print(httpRequest);
    Serial.println("[ThingSpeak] HTTPS запрос успешно отправлен.");
  } else {
    Serial.println("[ThingSpeak] Ошибка HTTPS подключения (порт 443).");
  }
}

void sendDataToBrewfather(float voltage, float pressure, float temp) {
  if (!settings.bfEnabled) return;
  unsigned long currentMillis = millis();
  if (currentMillis - lastBrewfatherTime < (settings.bfIntervalMinutes * 60000)) {
    return;
  }
  lastBrewfatherTime = currentMillis;

  client.stop();
  
  Serial.println("\n[Brewfather] Подключение к серверу для POST...");

  if (client.connect(bfServer, 443)) {
    Serial.print("[Brewfather] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P: "); Serial.print(pressure); 
    if (settings.useTempSensor) {
        Serial.print(" PSI, T: "); Serial.print(temp); Serial.println(" C");
    } else {
        Serial.println(" PSI");
    }

    Serial.println("[Brewfather] HTTPS Подключено! Формирование JSON пакета...");

    // 1. Формируем тело JSON (с резервированием памяти во избежание фрагментации кучи)
    String jsonBody;
    jsonBody.reserve(200);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + settings.bfDeviceName + "\",";
    jsonBody += "\"pressure\":" + String(pressure, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    if (settings.useTempSensor) {
        jsonBody += "\"temp\":" + String(temp, 2) + ",";
        jsonBody += "\"temp_unit\":\"C\",";
    }
    jsonBody += "\"battery\":" + String(voltage, 2) + ",";
    jsonBody += "\"comment\":\"Voltage: " + String(voltage, 2) + "V";
    if (settings.useTempSensor) {
        jsonBody += ", Temp: " + String(temp, 2) + "C";
    }
    jsonBody += "\"";
    jsonBody += "}";
    
    Serial.println("[Brewfather] Sending JSON: " + jsonBody);
    
    int contentLength = jsonBody.length();

    String httpRequest;
    httpRequest.reserve(300);
    httpRequest += "POST /stream?id=" + settings.bfStreamId + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(bfServer) + "\r\n";
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
