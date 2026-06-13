#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_adc_cal.h>

// Настройки Wi-Fi
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
const unsigned long tsInterval = 15000; // 15 секунд для ThingSpeak

unsigned long lastBrewfatherTime = 0;
const unsigned long bfInterval = 900000; // 15 минут (900 000 мс) для Brewfather

// Железо
const int sensorPin = 34;        // ADC1_CH6 (GPIO 34) - хороший выбор для ESP32
WiFiClientSecure client;

// Глобальные переменные для обмена данными между ядрами
float currentVoltage = 0.0;
float currentPressure = 0.0;
bool isDataReady = false;
SemaphoreHandle_t dataMutex; 

esp_adc_cal_characteristics_t adc_chars;

// Прототипы функций
void setupWiFi();
void sendDataToThingSpeak(float voltage, float pressure);
void sendDataToBrewfather(float voltage, float pressure);
void sensorTask(void *pvParameters);
void networkTask(void *pvParameters);

void setup() {
  Serial.begin(115200);   
  while (!Serial); 

  // Настройка АЦП и чтение калибровки из eFuse
  analogSetAttenuation(ADC_11db); // Используем ADC_11db, так как ADC_12db еще не объявлена в HAL
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  // Создаем мьютекс для защиты общих данных
  dataMutex = xSemaphoreCreateMutex();

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

    // 3. Расчет значений
    // Используем калиброванное преобразование raw -> mV
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(medianRaw, &adc_chars);
    float vMeasured = voltage_mv / 1000.0; // Напряжение на пине в Вольтах
    
    // Коэффициент для делителя 10кОм и 22кОм: (10+22)/22 = 1.4545
    float vSensor = vMeasured * 1.4545;          // Реальное напряжение на выходе датчика

    float p = 0.0;
    if (vSensor > 0.5) {
      // Формула для стандартного датчика 0.5V - 4.5V (100 PSI)
      p = (vSensor - 0.5) * 100.0 / (4.5 - 0.5); 
    }

    // Применение адаптивного фильтра EMA
    if (filteredVoltage < 0) {
      // Инициализация при первом проходе
      filteredVoltage = vSensor;
      filteredPressure = p;
    } else {
      // Вычисляем абсолютное изменение давления
      float diffPsi = abs(p - filteredPressure);

      if (diffPsi > stepThresholdPsi) {
        // Резкое изменение давления! Сбрасываем фильтр на новые мгновенные значения
        filteredVoltage = vSensor;
        filteredPressure = p;
        Serial.printf("[Сенсор] Резкий скачок давления! Разница: %.2f PSI. Фильтр сброшен.\n", diffPsi);
      } else {
        // Медленное изменение (шум АЦП). Плавно фильтруем
        filteredVoltage = alpha * vSensor + (1.0 - alpha) * filteredVoltage;
        filteredPressure = alpha * p + (1.0 - alpha) * filteredPressure;
      }
    }

    // 4. Сохранение в общие переменные с блокировкой мьютекса
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      currentVoltage = filteredVoltage;
      currentPressure = filteredPressure;
      isDataReady = true; // Данные считаны хотя бы один раз
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(200)); // Частота опроса ~5 Гц
  }
}

// --- ЛОГИКА СЕТИ (Core 0) ---
void networkTask(void *pvParameters) {
  setupWiFi();
  client.setInsecure();

  lastThingSpeakTime = millis() - tsInterval;
  lastBrewfatherTime = millis() - bfInterval;

  for (;;) {
    unsigned long currentMillis = millis();
    float vLocal = 0.0, pLocal = 0.0;
    bool ready = false;

    // Проверка подключения
    if (WiFi.status() != WL_CONNECTED) {
      setupWiFi();
    }

    // Получаем копию данных и статус готовности
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      vLocal = currentVoltage;
      pLocal = currentPressure;
      ready = isDataReady;
      xSemaphoreGive(dataMutex);
    }

    // Выполняем отправку только если данные уже были считаны сенсором
    if (ready) {
        // Отправка в ThingSpeak
        if (currentMillis - lastThingSpeakTime >= tsInterval) {
            lastThingSpeakTime = currentMillis;
            sendDataToThingSpeak(vLocal, pLocal);
        }

        // Отправка в Brewfather
        if (currentMillis - lastBrewfatherTime >= bfInterval) {
            lastBrewfatherTime = currentMillis;
            sendDataToBrewfather(vLocal, pLocal);
        }
    } else {
        Serial.println("[Сетевая задача] Ожидание первых данных от сенсора...");
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Проверка таймеров раз в секунду
  }
}

void setupWiFi() {
  Serial.print("Подключение к Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("[Wi-Fi] Подключено успешно!");
}

void sendDataToThingSpeak(float voltage, float pressure) {
  client.stop();
  Serial.println("\n[ThingSpeak] Подключение к серверу (HTTPS)...");
  if (client.connect(tsServer, 443)) {
    Serial.print("[ThingSpeak] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P: "); Serial.print(pressure); Serial.println(" PSI");

    String url = "/update?api_key=" + tsApiKey + 
                 "&field1=" + String(voltage, 3) + 
                 "&field2=" + String(pressure, 2);

    String httpRequest;
    httpRequest.reserve(160);
    httpRequest += "GET " + url + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(tsServer) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";

    client.print(httpRequest);
    Serial.println("[ThingSpeak] HTTPS запрос успешно отправлен.");
  } else {
    Serial.println("[ThingSpeak] Ошибка HTTPS подключения (порт 443).");
  }
}

void sendDataToBrewfather(float voltage, float pressure) {
  client.stop();
  
  Serial.println("\n[Brewfather] Подключение к серверу для POST...");

  if (client.connect(bfServer, 443)) {
    Serial.print("[Brewfather] Отправка -> V: "); Serial.print(voltage);
    Serial.print("V, P: "); Serial.print(pressure); Serial.println(" PSI");

    Serial.println("[Brewfather] HTTPS Подключено! Формирование JSON пакета...");

    // 1. Формируем тело JSON (с резервированием памяти во избежание фрагментации кучи)
    String jsonBody;
    jsonBody.reserve(120);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + deviceName + "\",";
    jsonBody += "\"pressure\":" + String(pressure, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    jsonBody += "\"battery\":" + String(voltage, 2);
    jsonBody += "}";
    
    int contentLength = jsonBody.length();

    String httpRequest;
    httpRequest.reserve(280);
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