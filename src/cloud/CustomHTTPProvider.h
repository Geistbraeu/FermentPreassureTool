#ifndef CUSTOM_HTTP_PROVIDER_H
#define CUSTOM_HTTP_PROVIDER_H

#include <WiFiClient.h>
#include "cloud/CloudProvider.h"

class CustomHTTPProvider : public CloudProvider {
public:
  void init() override;
  void send(const CloudPayload& payload) override;

private:
  WiFiClient httpClient;

  String replacePlaceholders(String text, const CloudPayload& payload) const;
};

#endif // CUSTOM_HTTP_PROVIDER_H
