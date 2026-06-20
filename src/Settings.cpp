#include "Settings.h"
#include "config.h"
#include <Preferences.h>

Settings settings;
WifiSettings wifiSettings;

bool Settings::saveFloat(const String& key, float value) {
    Preferences prefs;
    if (!prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, false)) {
        return false;
    }
    size_t written = prefs.putFloat(key.c_str(), value);
    prefs.end();
    return written > 0;
}

bool Settings::saveInt(const String& key, int value) {
    Preferences prefs;
    if (!prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, false)) {
        return false;
    }
    size_t written = prefs.putInt(key.c_str(), value);
    prefs.end();
    return written > 0;
}

bool Settings::saveULong(const String& key, unsigned long value) {
    Preferences prefs;
    if (!prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, false)) {
        return false;
    }
    size_t written = prefs.putULong(key.c_str(), value);
    prefs.end();
    return written > 0;
}

bool Settings::saveBool(const String& key, bool value) {
    Preferences prefs;
    if (!prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, false)) {
        return false;
    }
    size_t written = prefs.putBool(key.c_str(), value);
    prefs.end();
    return written > 0;
}

bool Settings::saveString(const String& key, const String& value) {
    Preferences prefs;
    if (!prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, false)) {
        return false;
    }
    size_t written = prefs.putString(key.c_str(), value.c_str());
    prefs.end();
    return written > 0;
}

void Settings::load() {
    Preferences prefs;
    prefs.begin(PreferencesConfig::NVS_NAMESPACE_CONFIG, true);
    maxPressureThreshold = prefs.getFloat("maxPressure", 14.0);
    pressureUnit = prefs.getInt("pUnit", 0);
    hysteresis = prefs.getFloat("hysteresis", 0.5);
    updateIntervalMs = prefs.getULong("updateInterval", ControlConfig::DEFAULT_UPDATE_INTERVAL_MS);
    medianSampleCount = prefs.getULong("medianCount", ControlConfig::DEFAULT_MEDIAN_SAMPLE_COUNT);
    medianSampleDelayMs = prefs.getULong("medianDelay", ControlConfig::DEFAULT_MEDIAN_SAMPLE_DELAY_MS);
    tsIntervalSeconds = prefs.getULong("tsInterval", CloudConfig::THINGSPEAK_DEFAULT_INTERVAL_SEC);
    bfIntervalMinutes = prefs.getULong("bfInterval", CloudConfig::BREWFATHER_DEFAULT_INTERVAL_MIN);
    offsetVoltage = prefs.getFloat("offsetVoltage", SensorConfig::PRESSURE_OFFSET_DEFAULT);
    tempOffset = prefs.getFloat("tempOffset", 0.5);
    useTempSensor = prefs.getBool("useTemp", true);
    // ⚠️ NO HARDCODED API KEYS - must be configured via web UI
    tsApiKey = prefs.getString("tsApiKey", "");
    bfStreamId = prefs.getString("bfStreamId", "");
    bfDeviceName = prefs.getString("bfDevName", "Pressure_Sensor");
    tsEnabled = prefs.getBool("tsEnabled", true);
    bfEnabled = prefs.getBool("bfEnabled", true);
    httpEnabled = prefs.getBool("httpEnabled", false);
    httpServer = prefs.getString("httpServer", "");
    httpPath = prefs.getString("httpPath", "/");
    httpBodyTemplate = prefs.getString("httpBody", "");
    httpIntervalSeconds = prefs.getULong("httpInterval", CloudConfig::CUSTOM_HTTP_DEFAULT_INTERVAL_SEC);
    prefs.end();
}

bool Settings::isValid() const {
    // Check if credentials are present when services are enabled
    if (tsEnabled && tsApiKey.isEmpty()) {
        return false;
    }
    if (bfEnabled && bfStreamId.isEmpty()) {
        return false;
    }
    if (maxPressureThreshold <= 0) {
        return false;
    }
    return true;
}

bool Settings::setMaxPressureThreshold(float val) {
    if (val < 0.5 || val > 25.0) return false;
    if (maxPressureThreshold == val) return true;  // No change
    maxPressureThreshold = val;
    return saveFloat("maxPressure", val);
}

bool Settings::setPressureUnit(int val) {
    if (val < 0 || val > 1) return false;  // Only 0 (PSI) or 1 (Bar)
    if (pressureUnit == val) return true;
    pressureUnit = val;
    return saveInt("pUnit", val);
}

bool Settings::setHysteresis(float val) {
    if (val <= 0 || val >= 2.0) return false;
    if (hysteresis == val) return true;
    hysteresis = val;
    return saveFloat("hysteresis", val);
}

bool Settings::setUpdateIntervalMs(unsigned long val) {
    if (val < 50 || val > 5000) return false;
    if (updateIntervalMs == val) return true;
    updateIntervalMs = val;
    return saveULong("updateInterval", val);
}

bool Settings::setMedianSampleCount(unsigned int val) {
    if (val < ControlConfig::MIN_MEDIAN_SAMPLES || val > ControlConfig::MAX_MEDIAN_SAMPLES) return false;
    if ((val % 2) == 0) val++;  // Ensure odd number
    if (val > ControlConfig::MAX_MEDIAN_SAMPLES) return false;
    if (medianSampleCount == val) return true;
    medianSampleCount = val;
    return saveULong("medianCount", val);
}

bool Settings::setMedianSampleDelayMs(unsigned long val) {
    if (val < 1 || val > 1000) return false;
    if (medianSampleDelayMs == val) return true;
    medianSampleDelayMs = val;
    return saveULong("medianDelay", val);
}

bool Settings::setTsIntervalSeconds(unsigned long val) {
    if (val < 15 || val > 3600) return false;
    if (tsIntervalSeconds == val) return true;
    tsIntervalSeconds = val;
    return saveULong("tsInterval", val);
}

bool Settings::setBfIntervalMinutes(unsigned long val) {
    if (val < 5 || val > 1440) return false;  // 5 min to 24 hours
    if (bfIntervalMinutes == val) return true;
    bfIntervalMinutes = val;
    return saveULong("bfInterval", val);
}

bool Settings::setOffsetVoltage(float val) {
    if (val < 0.0 || val > 4.5) return false;
    if (offsetVoltage == val) return true;
    offsetVoltage = val;
    return saveFloat("offsetVoltage", val);
}

bool Settings::setTempOffset(float val) {
    if (val < -50.0 || val > 50.0) return false;
    if (tempOffset == val) return true;
    tempOffset = val;
    return saveFloat("tempOffset", val);
}

bool Settings::setUseTempSensor(bool val) {
    if (useTempSensor == val) return true;
    useTempSensor = val;
    return saveBool("useTemp", val);
}

bool Settings::setTsApiKey(const String& val) {
    if (val.length() > CloudConfig::CUSTOM_HTTP_MAX_KEY_LEN) return false;
    if (tsApiKey == val) return true;
    tsApiKey = val;
    return saveString("tsApiKey", val);
}

bool Settings::setBfStreamId(const String& val) {
    if (val.length() > CloudConfig::CUSTOM_HTTP_MAX_KEY_LEN) return false;
    if (bfStreamId == val) return true;
    bfStreamId = val;
    return saveString("bfStreamId", val);
}

bool Settings::setBfDeviceName(const String& val) {
    if (val.isEmpty() || val.length() > 64) return false;
    if (bfDeviceName == val) return true;
    bfDeviceName = val;
    return saveString("bfDevName", val);
}

bool Settings::setTsEnabled(bool val) {
    if (tsEnabled == val) return true;
    tsEnabled = val;
    return saveBool("tsEnabled", val);
}

bool Settings::setBfEnabled(bool val) {
    if (bfEnabled == val) return true;
    bfEnabled = val;
    return saveBool("bfEnabled", val);
}

bool Settings::setHttpEnabled(bool val) {
    if (httpEnabled == val) return true;
    httpEnabled = val;
    return saveBool("httpEnabled", val);
}

bool Settings::setHttpServer(const String& val) {
    if (val.length() > CloudConfig::CUSTOM_HTTP_MAX_SERVER_LEN) return false;
    if (httpServer == val) return true;
    httpServer = val;
    return saveString("httpServer", val);
}

bool Settings::setHttpPath(const String& val) {
    if (val.isEmpty() || val.length() > CloudConfig::CUSTOM_HTTP_MAX_PATH_LEN) return false;
    if (httpPath == val) return true;
    httpPath = val;
    return saveString("httpPath", val);
}

bool Settings::setHttpBodyTemplate(const String& val) {
    if (val.length() > CloudConfig::CUSTOM_HTTP_MAX_BODY_LEN) return false;
    if (httpBodyTemplate == val) return true;
    httpBodyTemplate = val;
    return saveString("httpBody", val);
}

bool Settings::setHttpIntervalSeconds(unsigned long val) {
    if (val < 15 || val > 3600) return false;
    if (httpIntervalSeconds == val) return true;
    httpIntervalSeconds = val;
    return saveULong("httpInterval", val);
}

void WifiSettings::load() {
    Preferences prefs;
    prefs.begin(PreferencesConfig::NVS_NAMESPACE_WIFI, true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    devName = prefs.getString("devName", NetworkConfig::HOSTNAME);
    prefs.end();
}

void WifiSettings::save(const String& newSsid, const String& newPass, const String& newDevName) {
    ssid = newSsid;
    pass = newPass;
    devName = newDevName;
    Preferences prefs;
    prefs.begin(PreferencesConfig::NVS_NAMESPACE_WIFI, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("devName", devName);
    prefs.end();
}
