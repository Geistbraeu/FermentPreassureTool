#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_adc_cal.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX31865.h>
#include <ESPmDNS.h>
#include "config.h"
#include "ConfigPortal.h"
#include "web_server.h"
#include "CloudManager.h"
#include "Settings.h"
#include "RuntimeState.h"

// Настройки OLED
Adafruit_SSD1306 display(DisplayConfig::SCREEN_WIDTH, DisplayConfig::SCREEN_HEIGHT, &Wire, DisplayConfig::OLED_RESET_PIN);
bool isOledConnected = false;

// Настройки MAX31865 (SPI)
Adafruit_MAX31865 tempSensor = Adafruit_MAX31865(
    HardwareConfig::TEMP_SENSOR_CS_PIN,
    HardwareConfig::TEMP_SENSOR_DI_PIN,
    HardwareConfig::TEMP_SENSOR_DO_PIN,
    HardwareConfig::TEMP_SENSOR_CLK_PIN
);

// Железо
const int sensorPin = HardwareConfig::ADC_PRESSURE_PIN;

// Глобальные переменные для обмена данными между ядрами
RuntimeState runtimeState;

// Прототипы функций
void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp);
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);
void processSampledData();

void setup() {
  Serial.begin(NetworkConfig::SERIAL_BAUD_RATE);   
  while (!Serial); 

  // Настройка АЦП и чтение калибровки из eFuse
  analogSetAttenuation(ADC_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, SensorConfig::ADC_VREF, &runtimeState.adc_chars);

  // Инициализация OLED
  if(display.begin(SSD1306_SWITCHCAPVCC, DisplayConfig::OLED_I2C_ADDRESS)) {
    isOledConnected = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();
  } else {
    Serial.println(F("SSD1306 allocation failed (no OLED connected)"));
  }

  // Инициализация Wi-Fi через ConfigPortal
  ConfigPortal::begin();

  // Инициализация клапана
  pinMode(HardwareConfig::SOLENOID_PIN, OUTPUT);
  digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);

  // Инициализация mDNS
  if (!MDNS.begin(NetworkConfig::HOSTNAME)) {
    Serial.println("mDNS ERROR");
  } else {
    Serial.println("mDNS responder started: " + String(NetworkConfig::HOSTNAME) + ".local");
    MDNS.addService("http", "tcp", NetworkConfig::WEBSERVER_PORT);
  }
 
  // Чтение настроек
  settings.load();

  // Инициализация MAX31865
  if (settings.useTempSensor) {
    tempSensor.begin(MAX31865_2WIRE);
  }

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

  Serial.println("\n--- Система запущена на двух ядрах ---");
}

void loop() {
  // Пустой цикл, всё работает в задачах FreeRTOS
  vTaskDelete(NULL); 
}

// --- Логика сенсора температуры ---
float readTemperature(bool isEnabled) {
  if (!isEnabled) {
    runtimeState.isTempSensorConnected = false;
    return 0.0;
  }

  uint16_t fault = tempSensor.readFault();
  if (fault) {
    Serial.print("MAX31865 Fault: "); Serial.println(fault);
    tempSensor.clearFault();
    runtimeState.isTempSensorConnected = false;
    return 0.0;
  }

  float t = tempSensor.temperature(SensorConfig::RNOMINAL, SensorConfig::RREF);
  if (t < SensorConfig::TEMP_FAULT_THRESHOLD) {
    runtimeState.isTempSensorConnected = false;
    return 0.0;
  }

  float tempOffset = 0.0f;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    tempOffset = settings.tempOffset;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  t = t - tempOffset;
  runtimeState.isTempSensorConnected = true;
  return t;
}

void processSampledData() {
  bool useTempSensor = false;
  float maxPressureThreshold = 0.0f;
  float hysteresis = 0.0f;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    useTempSensor = settings.useTempSensor;
    maxPressureThreshold = settings.maxPressureThreshold;
    hysteresis = settings.hysteresis;
    xSemaphoreGive(runtimeState.settingsMutex);
  }

  float t = 0.0;
  if (useTempSensor) {
    t = readTemperature(true);
  } else {
    runtimeState.isTempSensorConnected = false;
  }

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
          digitalWrite(HardwareConfig::SOLENOID_PIN, runtimeState.manualOn ? HIGH : LOW);
      } else {
            if (runtimeState.currentPressure > maxPressureThreshold) {
              digitalWrite(HardwareConfig::SOLENOID_PIN, HIGH);
            } else if (runtimeState.currentPressure < (maxPressureThreshold - hysteresis)) {
              digitalWrite(HardwareConfig::SOLENOID_PIN, LOW);
          }
      }
    }
    xSemaphoreGive(runtimeState.dataMutex);
  }
}

// --- ЛОГИКА СЕНСОРА (Core 1) ---
void sensorTask(void *pvParameters) {
  int samples[ControlConfig::MAX_MEDIAN_SAMPLES];
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

    if (sampleCount < ControlConfig::MIN_MEDIAN_SAMPLES) sampleCount = ControlConfig::MIN_MEDIAN_SAMPLES;
    if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
    if ((sampleCount % 2) == 0) {
      sampleCount++;
      if (sampleCount > ControlConfig::MAX_MEDIAN_SAMPLES) sampleCount = ControlConfig::MAX_MEDIAN_SAMPLES;
    }

    // 1. Сбор данных для медианного фильтра
    for (unsigned int i = 0; i < sampleCount; i++) {
      samples[i] = analogRead(sensorPin);
      vTaskDelay(pdMS_TO_TICKS(medianSampleDelayMs));
    }

    // 2. Простая сортировка (пузырек) для нахождения медианы
    for (unsigned int i = 0; i < sampleCount - 1; i++) {
      for (unsigned int j = 0; j < sampleCount - i - 1; j++) {
        if (samples[j] > samples[j + 1]) {
          int temp = samples[j];
          samples[j] = samples[j + 1];
          samples[j + 1] = temp;
        }
      }
    }
    int medianRaw = samples[sampleCount / 2];

    // 3. Расчет значений давления
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(medianRaw, &runtimeState.adc_chars);
    float vMeasured = voltage_mv / 1000.0;
    float vSensor = vMeasured * SensorConfig::ADC_VOLTAGE_DIVIDER;

    float p = 0.0;
    if (vSensor > offsetVoltage) {
      p = (vSensor - offsetVoltage) * SensorConfig::PRESSURE_PSI_RANGE / SensorConfig::PRESSURE_VOLTAGE_RANGE;
    }

    // 4. Непрерывно публикуем фильтрованные значения в RuntimeState
    if (xSemaphoreTake(runtimeState.dataMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      runtimeState.sampledVoltage = vSensor;
      runtimeState.sampledPressure = p;
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

    // Обновление дисплея
    String ipStr = "Connecting...";
    if (WiFi.status() == WL_CONNECTED) {
      ipStr = WiFi.localIP().toString();
    }
    float pBar = pLocal * SensorConfig::PSI_TO_BAR;
    updateDisplay(ipStr, vLocal, pBar, tLocal);

    // Выполняем отправку только если данные уже были считаны сенсором
    if (ready) {
        sendDataToThingSpeak(vLocal, pLocal, pBar, tLocal);
        sendDataToBrewfather(vLocal, pLocal, tLocal);
        sendDataViaCustomHTTP(vLocal, pLocal, pBar, tLocal);
    } else {
        Serial.println("[Сетевая задача] Ожидание первых данных от сенсора...");
    }
    unsigned long updateIntervalMs = ControlConfig::DEFAULT_UPDATE_INTERVAL_MS;
    if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
      updateIntervalMs = settings.updateIntervalMs;
      xSemaphoreGive(runtimeState.settingsMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(updateIntervalMs));
  }
}

void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp) {
  if (!isOledConnected) return;

  int pressureUnit = 0;
  float maxPressureThreshold = 0.0f;
  if (xSemaphoreTake(runtimeState.settingsMutex, TaskConfig::MUTEX_TIMEOUT_TICKS) == pdTRUE) {
    pressureUnit = settings.pressureUnit;
    maxPressureThreshold = settings.maxPressureThreshold;
    xSemaphoreGive(runtimeState.settingsMutex);
  }
  
  display.clearDisplay();

  // 1. Верхняя зона (y=0-15) - Имя и Max Pressure
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(DisplayConfig::LAYOUT_X_LEFT, DisplayConfig::LAYOUT_Y_HOSTNAME);
  display.print(NetworkConfig::HOSTNAME);
  
  float pDisplay = (pressureUnit == 0) ? (pressureBar / SensorConfig::PSI_TO_BAR) : pressureBar;
  String unitStr = (pressureUnit == 0) ? "PSI" : "Bar";
  
  float maxPVal = (pressureUnit == 0) ? maxPressureThreshold : (maxPressureThreshold * SensorConfig::PSI_TO_BAR);
  String maxPStr = String(maxPVal, 1) + " " + unitStr;
  
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(maxPStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(128 - w, 0);
  display.print(maxPStr);
  
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // 2. Синяя зона (нижние 48 пикселей)
  // Средние 2/4 (y=16-48) - Давление
  display.setTextSize(3);
  display.setCursor(5, 20);
  
  if (pDisplay < 10.0) {
    display.print(pDisplay, 2);
  } else if (pDisplay < 100.0) {
    display.print(pDisplay, 1);
  } else {
    display.print((int)pDisplay);
  }

  display.setTextSize(2);
  display.setCursor(85, 28);
  display.print(unitStr);

  // Нижняя зона (y=49-63) - IP и Вольтаж
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(ipStatus);
  
  String voltStr = String(voltage, 2) + " V";
  display.getTextBounds(voltStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(128 - w, 52);
  display.print(voltStr);

  display.display();
}

