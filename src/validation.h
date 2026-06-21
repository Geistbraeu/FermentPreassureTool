#ifndef VALIDATION_H
#define VALIDATION_H

#include <Arduino.h>

/**
 * FermentPreassureTool - Input Validation Helpers
 * Centralized validation functions for web form inputs
 */

namespace Validation {
  /**
   * Trim whitespace from string
   */
  inline String trim(const String& val) {
    int start = 0;
    int end = val.length() - 1;
    while (start <= end && isspace(val[start])) start++;
    while (end >= start && isspace(val[end])) end--;
    return val.substring(start, end + 1);
  }

  /**
   * Pressure threshold validation (PSI)
   * Valid range: 0.5 - 34.0 PSI
   */
  inline bool isValidPressure(float val) {
    return val >= 0.5f && val <= 34.0f;
  }

  /**
   * Hysteresis validation (PSI)
   * Valid range: 0.1 - 2.0 PSI
   */
  inline bool isValidHysteresis(float val) {
    return val > 0.0f && val < 2.0f;
  }

  /**
   * Pressure unit validation
   * Valid: 0 (PSI) or 1 (Bar)
   */
  inline bool isValidPressureUnit(int val) {
    return val == 0 || val == 1;
  }

  /**
   * Update interval validation (milliseconds)
   * Valid range: 50 - 5000 ms
   */
  inline bool isValidUpdateInterval(unsigned long val) {
    return val >= 50 && val <= 5000;
  }

  /**
   * Median sample count validation
   * Valid range: 3-31, must be odd number
   */
  inline bool isValidMedianSampleCount(unsigned int val) {
    return val >= 3 && val <= 31 && (val % 2 == 1);
  }

  /**
   * Median sample delay validation (milliseconds)
   * Valid range: 1 - 1000 ms
   */
  inline bool isValidMedianSampleDelay(unsigned long val) {
    return val >= 1 && val <= 1000;
  }

  /**
   * ThingSpeak interval validation (seconds)
   * Valid range: 15 - 3600 seconds (15 sec - 1 hour)
   */
  inline bool isValidTsInterval(unsigned long val) {
    return val >= 15 && val <= 3600;
  }

  /**
   * Brewfather interval validation (minutes)
   * Valid range: 15 - 1440 minutes (15 min - 24 hours)
   */
  inline bool isValidBfInterval(unsigned long val) {
    return val >= 15 && val <= 1440;
  }

  /**
   * Offset voltage validation (Volts)
   * Valid range: 0.0 - 4.5 V
   */
  inline bool isValidOffsetVoltage(float val) {
    return val >= 0.0f && val <= 4.5f;
  }

  /**
   * Temperature offset validation (Celsius)
   * Valid range: -50 to +50°C
   */
  inline bool isValidTempOffset(float val) {
    return val >= -50.0f && val <= 50.0f;
  }

  /**
   * API Key validation (general string)
   * Valid: non-empty, max length check
   */
  inline bool isValidApiKey(const String& val, size_t maxLen = 128) {
    return !val.isEmpty() && val.length() <= maxLen;
  }

  /**
   * Device name validation
   * Valid: non-empty, 1-64 characters
   */
  inline bool isValidDeviceName(const String& val) {
    return !val.isEmpty() && val.length() <= 64;
  }

  /**
   * HTTP server validation
   * Valid: non-empty, max 64 characters
   */
  inline bool isValidHttpServer(const String& val) {
    return !val.isEmpty() && val.length() <= 64;
  }

  /**
   * HTTP path validation
   * Valid: starts with /, max 128 characters
   */
  inline bool isValidHttpPath(const String& val) {
    return !val.isEmpty() && val.length() <= 128 && val[0] == '/';
  }

  /**
   * HTTP body template validation
   * Valid: max 512 characters (can be empty for GET)
   */
  inline bool isValidHttpBodyTemplate(const String& val) {
    return val.length() <= 512;
  }

  /**
   * Custom HTTP interval validation (seconds)
   * Valid range: 15 - 3600 seconds
   */
  inline bool isValidCustomHttpInterval(unsigned long val) {
    return val >= 15 && val <= 3600;
  }

  /**
   * Generic string length validation
   * Helper for other string parameters
   */
  inline bool isValidStringLength(const String& val, size_t minLen = 0, size_t maxLen = 256) {
    return val.length() >= minLen && val.length() <= maxLen;
  }

  /**
   * Hostname/device name validation
   * Valid: alphanumeric, underscore, hyphen only
   */
  inline bool isValidHostname(const String& val) {
    if (val.isEmpty() || val.length() > 32) return false;
    for (unsigned int i = 0; i < val.length(); i++) {
      char c = val[i];
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
        return false;
      }
    }
    return true;
  }
}

#endif // VALIDATION_H
