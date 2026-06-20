#ifndef THINGSPEAK_PROVIDER_H
#define THINGSPEAK_PROVIDER_H

#include <WiFiClientSecure.h>
#include "cloud/CloudProvider.h"

class ThingSpeakProvider : public CloudProvider {
public:
  void init() override;
  void send(const CloudPayload& payload) override;

private:
  WiFiClientSecure client;
};

#endif // THINGSPEAK_PROVIDER_H
