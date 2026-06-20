#ifndef CONFIG_H
#define CONFIG_H

/**
 * FermentPreassureTool - Centralized Configuration
 * All hardware, sensor, and system constants in one place
 */

// ==================== HARDWARE PINS ====================
namespace HardwareConfig {
  // Solenoid relay control
  constexpr int SOLENOID_PIN = 27;

  // Pressure sensor (ADC)
  constexpr int ADC_PRESSURE_PIN = 34;  // ADC1_CH6

  // Temperature sensor (MAX31865 SPI)
  constexpr int TEMP_SENSOR_CS_PIN = 5;
  constexpr int TEMP_SENSOR_DI_PIN = 23;   // MOSI
  constexpr int TEMP_SENSOR_DO_PIN = 19;   // MISO
  constexpr int TEMP_SENSOR_CLK_PIN = 18;  // SCK
}

// ==================== DISPLAY (OLED SSD1306) ====================
namespace DisplayConfig {
  constexpr int SCREEN_WIDTH = 128;
  constexpr int SCREEN_HEIGHT = 64;
  constexpr int OLED_RESET_PIN = -1;
  constexpr uint8_t OLED_I2C_ADDRESS = 0x3C;

  // Display layout coordinates (Y positions)
  constexpr int LAYOUT_Y_HOSTNAME = 0;
  constexpr int LAYOUT_Y_STATUS = 15;
  constexpr int LAYOUT_Y_IP = 20;
  constexpr int LAYOUT_Y_PRESSURE = 28;
  constexpr int LAYOUT_Y_TEMP = 35;
  constexpr int LAYOUT_Y_VOLTAGE = 42;
  constexpr int LAYOUT_Y_MANUAL = 52;

  // Display layout coordinates (X positions)
  constexpr int LAYOUT_X_LEFT = 0;
  constexpr int LAYOUT_X_CENTER = 85;
}

// ==================== SENSOR CALIBRATION ====================
namespace SensorConfig {
  // MAX31865 PT100 calibration
  constexpr float RREF = 430.0;           // Reference resistor for PT100
  constexpr float RNOMINAL = 100.0;       // Nominal resistance at 0°C
  
  // Temperature sensor fault detection
  constexpr float TEMP_FAULT_THRESHOLD = -100.0;  // Celsius

  // Pressure sensor calibration
  constexpr float PRESSURE_OFFSET_DEFAULT = 0.515;  // Volts
  constexpr float PRESSURE_VOLTAGE_RANGE = 4.0;      // Volts (4.5V - 0.5V)
  constexpr float PRESSURE_PSI_RANGE = 100.0;        // PSI

  // Voltage divider
  constexpr float ADC_VOLTAGE_DIVIDER = 1.4545;

  // Unit conversions
  constexpr float PSI_TO_BAR = 0.0689476;

  // ADC settings
  // Note: ADC types don't support constexpr, use direct enum values
  // ADC_ATTEN_DB_12 and ADC_WIDTH_BIT_12 are defined in ESP32 Arduino framework
  constexpr uint32_t ADC_VREF = 1100;  // mV
}

// ==================== SAMPLING & CONTROL ====================
namespace ControlConfig {
  // Median filter
  constexpr unsigned int MAX_MEDIAN_SAMPLES = 31;
  constexpr unsigned int MIN_MEDIAN_SAMPLES = 3;

  // Manual solenoid override timeout
  constexpr unsigned long MANUAL_OVERRIDE_TIMEOUT_MS = 10000;  // 10 seconds

  // Default sampling intervals (configurable via web UI)
  constexpr unsigned long DEFAULT_UPDATE_INTERVAL_MS = 200;
  constexpr unsigned int DEFAULT_MEDIAN_SAMPLE_COUNT = 5;
  constexpr unsigned long DEFAULT_MEDIAN_SAMPLE_DELAY_MS = 10;
}

// ==================== NETWORK & mDNS ====================
namespace NetworkConfig {
  constexpr const char* HOSTNAME = "ferment01";
    // HOSTNAME will be used as: WiFi.setHostname(HOSTNAME) and MDNS.begin(HOSTNAME)
  constexpr int WEBSERVER_PORT = 80;
  constexpr int MDNS_PORT = 53;

  // Serial communication
  constexpr unsigned long SERIAL_BAUD_RATE = 115200;
}

// ==================== CLOUD INTEGRATION ====================
namespace CloudConfig {
  // ThingSpeak
  constexpr const char* THINGSPEAK_SERVER = "api.thingspeak.com";
  constexpr int THINGSPEAK_PORT = 443;  // HTTPS
  constexpr unsigned long THINGSPEAK_DEFAULT_INTERVAL_SEC = 120;

  // Brewfather
  constexpr const char* BREWFATHER_SERVER = "log.brewfather.net";
  constexpr int BREWFATHER_PORT = 443;  // HTTPS
  constexpr unsigned long BREWFATHER_DEFAULT_INTERVAL_MIN = 15;

  // Custom HTTP
  constexpr int CUSTOM_HTTP_PORT_HTTP = 80;
  constexpr int CUSTOM_HTTP_PORT_HTTPS = 443;
  constexpr unsigned long CUSTOM_HTTP_DEFAULT_INTERVAL_SEC = 60;
  constexpr size_t CUSTOM_HTTP_MAX_SERVER_LEN = 64;
  constexpr size_t CUSTOM_HTTP_MAX_PATH_LEN = 128;
  constexpr size_t CUSTOM_HTTP_MAX_BODY_LEN = 512;
  constexpr size_t CUSTOM_HTTP_MAX_KEY_LEN = 128;
}

// ==================== FREERTOS TASK CONFIGURATION ====================
namespace TaskConfig {
  // Sensor task (Core 1)
  constexpr int SENSOR_TASK_STACK_SIZE = 4096;
  constexpr int SENSOR_TASK_PRIORITY = 1;
  constexpr int SENSOR_TASK_CORE = 1;

  // Network task (Core 0)
  constexpr int NETWORK_TASK_STACK_SIZE = 8192;
  constexpr int NETWORK_TASK_PRIORITY = 1;
  constexpr int NETWORK_TASK_CORE = 0;

  // Mutex timeout
  constexpr TickType_t MUTEX_TIMEOUT_TICKS = pdMS_TO_TICKS(100);
}

// ==================== PREFERENCES (NVS) ====================
namespace PreferencesConfig {
  constexpr const char* NVS_NAMESPACE_CONFIG = "config";
  constexpr const char* NVS_NAMESPACE_WIFI = "wifi";
  constexpr size_t NVS_MAX_KEY_LENGTH = 15;
}

#endif // CONFIG_H
