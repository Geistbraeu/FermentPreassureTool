#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_adc_cal.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MAX31865.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "web_server.h"

// Настройки OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool isOledConnected = false;

// Настройки MAX31865 (SPI)
#define MAX31865_CS_PIN 5
// Использование программного SPI (для примера, можно использовать аппаратный)
// Pins: DI=23, DO=19, CLK=18, CS=5
Adafruit_MAX31865 tempSensor = Adafruit_MAX31865(MAX31865_CS_PIN, 23, 19, 18);
#define RREF      430.0
#define RNOMINAL  100.0
const char* ssid = "Ghost";
const char* pass = "ghostdemonde";

// Настройки Облаков
const char* tsServer = "api.thingspeak.com";
const String tsApiKey = "DEXTOPQCD39G16GW";

const char* bfServer = "log.brewfather.net";
const String bfStreamId = "b8wwXJ3xdW3B8h"; 
const String deviceName = "Pressure_Sensor"; // Имя вашего датчика в Brewfather

// Таймеры (миллисекунды)
unsigned long lastThingSpeakTime = 0;
const unsigned long tsInterval = 120000; // 120 секунд для ThingSpeak

unsigned long lastBrewfatherTime = 0;
const unsigned long bfInterval = 900000; // 15 минут (900 000 мс) для Brewfather

// Железо
const int sensorPin = 34;        // ADC1_CH6 (GPIO 34) - хороший выбор для ESP32
WiFiClientSecure client;

// Глобальные переменные для обмена данными между ядрами
float currentVoltage = 0.0;
float currentPressure = 0.0;
float currentTemp = 0.0;
float maxPressureThreshold = 14.0; // Порог давления
float offsetVoltage = 0.515; // Для калибровки нуля, если нужно
float tempOffset = 0.5;      // Смещение температуры для компенсации сопротивления проводов (в °C)
bool isDataReady = false;
bool isTempSensorConnected = false;
SemaphoreHandle_t dataMutex; 
esp_adc_cal_characteristics_t adc_chars;

// Прототипы функций
void setupWiFi();
void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp);
void sendDataToBrewfather(float voltage, float pressure, float temp);
void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp);
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);

void setup() {
  Serial.begin(115200);   
  while (!Serial); 

  // Настройка АЦП и чтение калибровки из eFuse
  analogSetAttenuation(ADC_11db); // Используем ADC_11db, так как ADC_12db еще не объявлена в HAL
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  // Инициализация OLED
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    isOledConnected = true;
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Connecting...");
    display.display();
  } else {
    Serial.println(F("SSD1306 allocation failed (no OLED connected)"));
  }

  // Инициализация Wi-Fi
  setupWiFi();

  // Инициализация mDNS
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("mDNS ERROR");
  } else {
    Serial.println("mDNS responder started: " + String(HOSTNAME) + ".local");
    MDNS.addService("http", "tcp", 80);
  }
 
 

  // Инициализация MAX31865
  // tempSensor.begin(MAX31865_2WIRE);  // Настройка для 3-проводного датчика, измените если 2 или 4

  // Чтение настроек
  Preferences prefs;
  prefs.begin("config", true);
  maxPressureThreshold = prefs.getFloat("maxPressure", 100.0);
  offsetVoltage = prefs.getFloat("offsetVoltage", 0.515);
  prefs.end();

  // Создаем мьютекс для защиты общих данных
  dataMutex = xSemaphoreCreateMutex();

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

// --- ЛОГИКА СЕНСОРА (Core 1) ---
void sensorTask(void *pvParameters) {
  const int numSamples = 5;
  int samples[numSamples];

  // Коэффициенты адаптивного фильтра (EMA)
  const float alpha = 0.03;            // Степень сглаживания при медленных изменениях
  const float stepThresholdPsi = 5.0;  // Порог резкого изменения давления (в PSI)
  static float filteredVoltage = -1.0; // Хранилище отфильтрованного вольтажа
  static float filteredPressure = -1.0;// Хранилище отфильтрованного давления

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
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(medianRaw, &adc_chars);
    float vMeasured = voltage_mv / 1000.0;
    float vSensor = vMeasured * 1.4545;

    float p = 0.0;
    if (vSensor > offsetVoltage) {
      p = (vSensor - offsetVoltage) * 100.0 / (4.5 - 0.5); 
    }

    // 4. Чтение температуры (отключено)
    // uint16_t fault = tempSensor.readFault();
    // float t = 0.0;
    // if (fault) {
    //   Serial.print("MAX31865 Fault: "); Serial.println(fault);
    //   tempSensor.clearFault();
    //   isTempSensorConnected = false;
    // } else {
    //   t = tempSensor.temperature(RNOMINAL, RREF);
    //   if (t < -100.0) {
    //     isTempSensorConnected = false;
    //   } else {
    //     t = t - tempOffset;
    //     isTempSensorConnected = true;
    //   }
    // }
    float t = 0.0;
    isTempSensorConnected = false;

    // Применение адаптивного фильтра (для давления)
    if (filteredVoltage < 0) {
      filteredVoltage = vSensor;
      filteredPressure = p;
    } else {
      float diffPsi = abs(p - filteredPressure);
      if (diffPsi > stepThresholdPsi) {
        filteredVoltage = vSensor;
        filteredPressure = p;
        Serial.printf("[Сенсор] Резкий скачок давления! Разница: %.2f PSI. Фильтр сброшен.\n", diffPsi);
      } else {
        filteredVoltage = alpha * vSensor + (1.0 - alpha) * filteredVoltage;
        filteredPressure = alpha * p + (1.0 - alpha) * filteredPressure;
      }
    }

    // 5. Сохранение в общие переменные с блокировкой мьютекса
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      currentVoltage = filteredVoltage;
      currentPressure = filteredPressure;
      currentTemp = t; // Сохраняем температуру
      isDataReady = true;
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// --- ЛОГИКА СЕТИ (Core 0) ---
void networkTask(void *pvParameters) {
  client.setInsecure();

  lastThingSpeakTime = millis() - tsInterval;
  lastBrewfatherTime = millis() - bfInterval;

  for (;;) {
    unsigned long currentMillis = millis();
    float vLocal = 0.0, pLocal = 0.0, tLocal = 0.0;
    bool ready = false;

    // Проверка подключения
    if (WiFi.status() != WL_CONNECTED) {
      setupWiFi();
    }

    // Получаем копию данных и статус готовности
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      vLocal = currentVoltage;
      pLocal = currentPressure;
      tLocal = currentTemp;
      ready = isDataReady;
      xSemaphoreGive(dataMutex);
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
        // Отправка в ThingSpeak
        if (currentMillis - lastThingSpeakTime >= tsInterval) {
            lastThingSpeakTime = currentMillis;
            sendDataToThingSpeak(vLocal, pLocal, pBar, 0.0);
        }

        // Отправка в Brewfather
        if (currentMillis - lastBrewfatherTime >= bfInterval) {
            lastBrewfatherTime = currentMillis;
            sendDataToBrewfather(vLocal, pLocal, 0.0);
        }
    } else {
        Serial.println("[Сетевая задача] Ожидание первых данных от сенсора...");
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Проверка таймеров раз в секунду
  }
}

void updateDisplay(String ipStatus, float voltage, float pressureBar, float temp) {
  if (!isOledConnected) return;
  
  display.clearDisplay();

  // 1. Желтая зона (верхние 16 пикселей)
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (ipStatus == "Connecting...") {
      display.setCursor(0, 0);
      display.print(ipStatus);
  } else {
      display.setCursor(0, 0);
      display.print(ipStatus);
      
      String voltStr = String(voltage, 2) + " V";
      int16_t x1, y1;
      uint16_t w, h;
      display.getTextBounds(voltStr, 0, 0, &x1, &y1, &w, &h);
      display.setCursor(128 - w, 0);
      display.print(voltStr);
  }
  display.drawLine(0, 15, 127, 15, SSD1306_WHITE);

  // 2. Синяя зона (нижние 48 пикселей)
  // Средние 2/4 (y=16-48) - Давление
  display.setTextSize(3);
  display.setCursor(5, 20);
  if (pressureBar < 10.0) {
    display.print(pressureBar, 2);
  } else if (pressureBar < 100.0) {
    display.print(pressureBar, 1);
  } else {
    display.print((int)pressureBar);
  }

  display.setTextSize(2);
  display.setCursor(85, 28);
  display.print("Bar");

  // Нижняя 1/4 (y=49-63) - Температура
  // display.setTextSize(1);
  // display.setCursor(5, 52);
  // display.print("Temp: ");
  // if (isTempSensorConnected) {
  //   display.print(temp, 1);
  //   display.print(" C");
  // } else {
  //   display.print("Error");
  // }

  display.display();
}

void setupWiFi() {
  Serial.print("Подключение к Wi-Fi: ");
  Serial.println(ssid);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.getHostname());
  Serial.println("[Wi-Fi] Подключено успешно!");
}

void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp) {
  client.stop();
  Serial.println("\n[ThingSpeak] Подключение к серверу (HTTPS)...");
  if (client.connect(tsServer, 443)) {
    Serial.print("[ThingSpeak] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P(PSI): "); Serial.print(pressure);
    Serial.print(", P(Bar): "); Serial.print(pressureBar); 
    Serial.print(" Bar, T: "); Serial.print(temp); Serial.println(" C");

    String url = "/update?api_key=" + tsApiKey + 
                 "&field1=" + String(voltage, 3) + 
                 "&field2=" + String(pressure, 2) +
                 "&field3=" + String(pressureBar, 2) +
                 "&field4=" + String(temp, 2);

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
  client.stop();
  
  Serial.println("\n[Brewfather] Подключение к серверу для POST...");

  if (client.connect(bfServer, 443)) {
    Serial.print("[Brewfather] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P: "); Serial.print(pressure); 
    Serial.print(" PSI, T: "); Serial.print(temp); Serial.println(" C");

    Serial.println("[Brewfather] HTTPS Подключено! Формирование JSON пакета...");

    // 1. Формируем тело JSON (с резервированием памяти во избежание фрагментации кучи)
    String jsonBody;
    jsonBody.reserve(150);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + deviceName + "\",";
    jsonBody += "\"pressure\":" + String(pressure, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    jsonBody += "\"temp\":" + String(temp, 2) + ",";
    jsonBody += "\"temp_unit\":\"C\",";
    jsonBody += "\"battery\":" + String(voltage, 2) + ",";
    jsonBody += "\"comment\":\"Voltage: " + String(voltage, 2) + "V, Temp: " + String(temp, 2) + "C\"";
    jsonBody += "}";
    
    Serial.println("[Brewfather] Sending JSON: " + jsonBody);
    
    int contentLength = jsonBody.length();

    String httpRequest;
    httpRequest.reserve(300);
    httpRequest += "POST /stream?id=" + bfStreamId + " HTTP/1.1\r\n";
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