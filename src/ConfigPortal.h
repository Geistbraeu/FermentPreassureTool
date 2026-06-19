#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>

class ConfigPortal {
public:
    static bool begin(); // Tries to connect, returns true if connected
    static bool connect(); // Connects using stored settings
    static void startSetupMode(); // Starts AP, Captive Portal, blocks for 5 mins
};

#endif
