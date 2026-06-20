#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_adc_cal.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX31865.h>
#include <ESPmDNS.h>
#include "ConfigPortal.h"
#include "web_server.h"
#include "CloudManager.h"
#include "Settings.h"
#include "RuntimeState.h"

// Настройки OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SOLENOID_PIN  27
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool isOledConnected = false;

// Настройки MAX31865 (SPI)
#define MAX31865_CS_PIN 5
// Использование программного SPI (для примера, можно использовать аппаратный)
// Pins: DI=23, DO=19, CLK=18, CS=5
Adafruit_MAX31865 tempSensor = Adafruit_MAX31865(MAX31865_CS_PIN, 23, 19, 18);
#define RREF      430.0
#define RNOMINAL  100.0




// Железо
const int sensorPin = 34;        // ADC1_CH6 (GPIO 34) - хороший выбор для ESP32

// Глобальные переменные для обмена данными между ядрами
RuntimeState runtimeState;

// Прототипы функций
void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp);
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);

void setup() {
  Serial.begin(115200);   
  while (!Serial); 

  // Настройка АЦП и чтение калибровки из eFuse
  analogSetAttenuation(ADC_11db); // Используем ADC_11db, так как ADC_12db еще не объявлена в HAL
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &runtimeState.adc_chars);

  // Инициализация OLED
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
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
  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW);

  // Инициализация mDNS
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("mDNS ERROR");
  } else {
    Serial.println("mDNS responder started: " + String(HOSTNAME) + ".local");
    MDNS.addService("http", "tcp", 80);
  }
 
 

  // Чтение настроек
  settings.load();

  // Инициализация MAX31865
  if (settings.useTempSensor) {
    tempSensor.begin(MAX31865_2WIRE);
  }


  // Создаем мьютекс для защиты общих данных
  runtimeState.dataMutex = xSemaphoreCreateMutex();

  // Инициализация веб-сервера
  initWebServer();

  // Задача для чтения сенсора (Core 1)
  xTaskCreatePinnedToCore(
    sensorTask,   // Функция задачи
    "SensorTask", // Имя
    4096,         // Стек
    NULL,         // Параметры
    1,            // Приоритет
    NULL,         // Хендл
    1             // Ядро (Core 1)
  );

  // Задача для Wi-Fi и облаков (Core 0)
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,         // Больше стека для SSL
    NULL,
    1,
    NULL,
    0             // Ядро (Core 0)
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

  float t = tempSensor.temperature(RNOMINAL, RREF);
  if (t < -100.0) {
    runtimeState.isTempSensorConnected = false;
    return 0.0;
  }

  t = t - settings.tempOffset;
  runtimeState.isTempSensorConnected = true;
  return t;
}

// --- ЛОГИКА СЕНСОРА (Core 1) ---
void sensorTask(void *pvParameters) {
  const int numSamples = 5;
  int samples[numSamples];

  for (;;) {
    // 1. Сбор данных для медианного фильтра
    for (int i = 0; i < numSamples; i++) {
      samples[i] = analogRead(sensorPin);
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 2. Простая сортировка (пузырек) для нахождения медианы
    for (int i = 0; i < numSamples - 1; i++) {
      for (int j = 0; j < numSamples - i - 1; j++) {
        if (samples[j] > samples[j + 1]) {
          int temp = samples[j];
          samples[j] = samples[j + 1];
          samples[j + 1] = temp;
        }
      }
    }
    int medianRaw = samples[numSamples / 2];

    // 3. Расчет значений давления
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(medianRaw, &runtimeState.adc_chars);
    float vMeasured = voltage_mv / 1000.0;
    float vSensor = vMeasured * 1.4545;

    float p = 0.0;
    if (vSensor > settings.offsetVoltage) {
      p = (vSensor - settings.offsetVoltage) * 100.0 / (4.5 - 0.5); 
    }

    // 4. Чтение температуры
    float t = 0.0;
    if (settings.useTempSensor) {
      t = readTemperature(true);
    } else {
      runtimeState.isTempSensorConnected = false;
    }

    // 5. Сохранение в общие переменные с блокировкой мьютекса
    if (xSemaphoreTake(runtimeState.dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      runtimeState.currentVoltage = vSensor;
      runtimeState.currentPressure = p;
      runtimeState.currentTemp = t; // Сохраняем температуру
      runtimeState.isDataReady = true;

      // Логика управления клапаном
      if (runtimeState.manualOverride && (millis() - runtimeState.manualStartTime > 10000)) {
          runtimeState.manualOverride = false;
      }
      
      if (runtimeState.manualOverride) {
          digitalWrite(SOLENOID_PIN, runtimeState.manualOn ? HIGH : LOW);
      } else {
          if (runtimeState.currentPressure > settings.maxPressureThreshold) {
              digitalWrite(SOLENOID_PIN, HIGH);
          } else if (runtimeState.currentPressure < (settings.maxPressureThreshold - settings.hysteresis)) {
              digitalWrite(SOLENOID_PIN, LOW);
          }
      }
      xSemaphoreGive(runtimeState.dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(settings.sensorInterval));
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
    if (xSemaphoreTake(runtimeState.dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
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
    float pBar = pLocal * 0.0689476;
    updateDisplay(ipStr, vLocal, pBar, tLocal);

    // Выполняем отправку только если данные уже были считаны сенсором
    if (ready) {
        sendDataToThingSpeak(vLocal, pLocal, pBar, tLocal);
        sendDataToBrewfather(vLocal, pLocal, tLocal);
        sendDataViaCustomHTTP(vLocal, pLocal, pBar, tLocal);
    } else {
        Serial.println("[Сетевая задача] Ожидание первых данных от сенсора...");
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Проверка таймеров раз в секунду
  }
}

void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp) {
  if (!isOledConnected) return;
  
  display.clearDisplay();

  // 1. Верхняя зона (y=0-15) - Имя и Max Pressure
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(HOSTNAME);
  
  float pDisplay = (settings.pressureUnit == 0) ? (pressureBar / 0.0689476) : pressureBar;
  String unitStr = (settings.pressureUnit == 0) ? "PSI" : "Bar";
  
  float maxPVal = (settings.pressureUnit == 0) ? settings.maxPressureThreshold : (settings.maxPressureThreshold * 0.0689476);
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

