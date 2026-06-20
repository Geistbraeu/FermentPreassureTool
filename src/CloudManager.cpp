#include "CloudManager.h"

static CloudManager cloudManager;

void CloudManager::init() {
  thingSpeakProvider.init();
  brewfatherProvider.init();
  customHttpProvider.init();
}

void CloudManager::sendThingSpeak(const CloudPayload& payload) {
  thingSpeakProvider.send(payload);
}

void CloudManager::sendBrewfather(const CloudPayload& payload) {
  brewfatherProvider.send(payload);
}

void CloudManager::sendCustomHttp(const CloudPayload& payload) {
  customHttpProvider.send(payload);
}

void initCloud() {
  cloudManager.init();
}

void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressureBar;
  payload.temperatureC = temp;
  cloudManager.sendThingSpeak(payload);
}

void sendDataToBrewfather(float voltage, float pressure, float temp) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressure * 0.0689476f;
  payload.temperatureC = temp;
  cloudManager.sendBrewfather(payload);
}

void sendDataViaCustomHTTP(float voltage, float pressure, float pressureBar, float temp) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressureBar;
  payload.temperatureC = temp;
  cloudManager.sendCustomHttp(payload);
}
