#include <Arduino.h>
#include <WiFi.h>
#include <esp_adc_cal.h>
#include <ESPmDNS.h>
#include "config.h"
#include "ConfigPortal.h"
#include "web_server.h"
#include "CloudManager.h"
#include "Settings.h"
#include "RuntimeState.h"
#include "debug.h"
#include "device/DisplayManager.h"
#include "device/SensorManager.h"
#include "device/SolenoidController.h"

// Глобальные переменные для обмена данными между ядрами
RuntimeState runtimeState;

// Прототипы функций
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);
void processSampledData();

void setup() {
  Serial.begin(NetworkConfig::SERIAL_BAUD_RATE);   
  while (!Serial); 

  // Настройка АЦП и чтение калибровки из eFuse
  sensorManager.initAdc(&runtimeState.adc_chars);

  // Инициализация OLED
  displayManager.init();

  // Инициализация Wi-Fi через ConfigPortal
  ConfigPortal::begin();

  // Инициализация клапана
  solenoidController.init();

  // Инициализация mDNS
  if (!MDNS.begin(NetworkConfig::HOSTNAME)) {
    DBG("mDNS ERROR");
  } else {
    DBG("mDNS responder started: " + String(NetworkConfig::HOSTNAME) + ".local");
    MDNS.addService("http", "tcp", NetworkConfig::WEBSERVER_PORT);
  }
 
  // Чтение настроек
  settings.load();

  // Инициализация MAX31865
  sensorManager.initTempSensor(settings.useTempSensor);

  // Создаем мьютекс для защиты общих данных
  runtimeState.dataMutex = xSemaphoreCreateMutex();
  runtimeState.settingsMutex = xSemaphoreCreateMutex();
  if (runtimeState.dataMutex == NULL || runtimeState.settingsMutex == NULL) {
    Serial.println("FATAL: mutex creation failed");
    for (;;) {
      delay(1000);
    }
  }

  // Инициализация веб-сервера
  initWebServer();

  // Задача для чтения сенсора (Core 1)
  xTaskCreatePinnedToCore(
    sensorTask,
    "SensorTask",
    TaskConfig::SENSOR_TASK_STACK_SIZE,
    NULL,
    TaskConfig::SENSOR_TASK_PRIORITY,
    NULL,
    TaskConfig::SENSOR_TASK_CORE
  );

  // Задача для Wi-Fi и облаков (Core 0)
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    TaskConfig::NETWORK_TASK_STACK_SIZE,
    NULL,
    TaskConfig::NETWORK_TASK_PRIORITY,
    NULL,
    TaskConfig::NETWORK_TASK_CORE
  );

  DBG("\n--- Система запущена на двух ядрах ---");
}

void loop() {
  // Пустой цикл, всё работает в задачах FreeRTOS
  vTaskDelete(NULL); 
}

void processSampledData() {
  bool useTempSensor = false;
  float maxPressureThreshold = 0.0f;
  float hysteresis = 0.0f;
  float tempOffset = 0.0f;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    useTempSensor = settings.useTempSensor;
    maxPressureThreshold = settings.maxPressureThreshold;
    hysteresis = settings.hysteresis;
    tempOffset = settings.tempOffset;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  bool isTempConnected = false;
  float t = sensorManager.readTemperature(useTempSensor, tempOffset, &isTempConnected);
  runtimeState.isTempSensorConnected = isTempConnected;

  if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    if (runtimeState.hasSampledData) {
      runtimeState.currentVoltage = runtimeState.sampledVoltage;
      runtimeState.currentPressure = runtimeState.sampledPressure;
      runtimeState.currentTemp = t;
      runtimeState.isDataReady = true;

        if (runtimeState.manualOverride &&
          (unsigned long)(millis() - runtimeState.manualStartTime) > ControlConfig::MANUAL_OVERRIDE_TIMEOUT_MS) {
          runtimeState.manualOverride = false;
      }

      if (runtimeState.manualOverride) {
          solenoidController.applyState(true, runtimeState.manualOn, runtimeState.currentPressure, maxPressureThreshold, hysteresis);
      } else {
          solenoidController.applyState(false, false, runtimeState.currentPressure, maxPressureThreshold, hysteresis);
      }
    }
    xSemaphoreGive(runtimeState.dataMutex);
  }
}

// --- ЛОГИКА СЕНСОРА (Core 1) ---
void sensorTask(void *pvParameters) {
  unsigned long lastProcessAt = 0;

  for (;;) {
    unsigned int sampleCount = ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT;
    unsigned long medianSampleDelayMs = ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS;
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    float offsetVoltage = SensorConfig::PRESSURE_OFFSET_DEFAULT;
    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      sampleCount = settings.medianSampleCount;
      medianSampleDelayMs = settings.medianSampleDelayMs;
      updateIntervalMs = settings.updateIntervalMs;
      offsetVoltage = settings.offsetVoltage;
      xSemaphoreGive(runtimeState.settingsMutex);
    }

    SensorReading reading = sensorManager.readFilteredPressure(sampleCount, medianSampleDelayMs, offsetVoltage, &runtimeState.adc_chars);

    // 4. Непрерывно публикуем фильтрованные значения в RuntimeState
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      runtimeState.sampledVoltage = reading.voltage;
      runtimeState.sampledPressure = reading.pressure;
      runtimeState.hasSampledData = true;
      xSemaphoreGive(runtimeState.dataMutex);
    }

    // 5. Периодическая обработка sampled данных по updateIntervalMs
    unsigned long now = millis();
    if (lastProcessAt == 0 || (now - lastProcessAt) >= updateIntervalMs) {
      processSampledData();
      lastProcessAt = now;
    }
  }
}

// --- ЛОГИКА СЕТИ (Core 0) ---
void networkTask(void *pvParameters) {
  initCloud();

  for (;;) {
    float vLocal = 0.0, pLocal = 0.0, tLocal = 0.0;
    bool ready = false;
    int pressureUnit = 0;
    float maxPressureThreshold = 0.0f;

    // Проверка подключения
    if (WiFi.status() != WL_CONNECTED) {
      ConfigPortal::connect();
    }

    // Получаем копию данных и статус готовности
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      vLocal = runtimeState.currentVoltage;
      pLocal = runtimeState.currentPressure;
      tLocal = runtimeState.currentTemp;
      ready = runtimeState.isDataReady;
      xSemaphoreGive(runtimeState.dataMutex);
    }

    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      pressureUnit = settings.pressureUnit;
      maxPressureThreshold = settings.maxPressureThreshold;
      xSemaphoreGive(runtimeState.settingsMutex);
    }

    // Обновление дисплея
    String ipStr = "Connecting...";
    if (WiFi.status() == WL_CONNECTED) {
      ipStr = WiFi.localIP().toString();
    }
    float pBar = pLocal * SensorConfig::PSI_TO_BAR;
    displayManager.update(ipStr, vLocal, pBar, pressureUnit, maxPressureThreshold);

    // Выполняем отправку только если данные уже были считаны сенсором
    if (ready) {
        sendDataToThingSpeak(vLocal, pLocal, pBar, tLocal);
        sendDataToBrewfather(vLocal, pLocal, tLocal);
        sendDataViaCustomHTTP(vLocal, pLocal, pBar, tLocal);
    }
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      updateIntervalMs = settings.updateIntervalMs;
      xSemaphoreGive(runtimeState.settingsMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(updateIntervalMs));
  }
}

