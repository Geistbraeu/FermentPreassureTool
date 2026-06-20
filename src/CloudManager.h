#ifndef CLOUD_MANAGER_H
#define CLOUD_MANAGER_H

#include "cloud/BrewfatherProvider.h"
#include "cloud/CustomHTTPProvider.h"
#include "cloud/ThingSpeakProvider.h"

class CloudManager {
public:
	void init();
	void sendThingSpeak(const CloudPayload& payload);
	void sendBrewfather(const CloudPayload& payload);
	void sendCustomHttp(const CloudPayload& payload);

private:
	ThingSpeakProvider thingSpeakProvider;
	BrewfatherProvider brewfatherProvider;
	CustomHTTPProvider customHttpProvider;
};

void initCloud();
void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp);
void sendDataToBrewfather(float voltage, float pressure, float temp);
void sendDataViaCustomHTTP(float voltage, float pressure, float pressureBar, float temp);

#endif // CLOUD_MANAGER_H
