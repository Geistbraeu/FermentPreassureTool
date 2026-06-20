#ifndef BREWFATHER_PROVIDER_H
#define BREWFATHER_PROVIDER_H

#include <WiFiClientSecure.h>
#include "cloud/CloudProvider.h"

class BrewfatherProvider : public CloudProvider {
public:
  void init() override;
  void send(const CloudPayload& payload) override;

private:
  WiFiClientSecure client;
};

#endif // BREWFATHER_PROVIDER_H
