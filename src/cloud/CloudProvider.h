#ifndef CLOUD_PROVIDER_H
#define CLOUD_PROVIDER_H

#include <Arduino.h>

struct CloudPayload {
  float voltage = 0.0f;
  float pressurePsi = 0.0f;
  float pressureBar = 0.0f;
  float temperatureC = 0.0f;
};

class CloudProvider {
public:
  virtual ~CloudProvider() = default;
  virtual void init() = 0;
  virtual void send(const CloudPayload& payload) = 0;

protected:
  unsigned long lastSendMs = 0;

  bool isIntervalElapsed(unsigned long nowMs, unsigned long intervalMs) const {
    return lastSendMs == 0 || (nowMs - lastSendMs >= intervalMs);
  }

  void markSent(unsigned long nowMs) {
    lastSendMs = nowMs;
  }
};

#endif // CLOUD_PROVIDER_H
