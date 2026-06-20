#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

#if defined(APP_DEBUG_SERIAL) && (APP_DEBUG_SERIAL == 1)
#define DBG(msg) do { Serial.println(msg); } while (0)
#define DBGF(...) do { Serial.printf(__VA_ARGS__); } while (0)
#else
#define DBG(msg) do { } while (0)
#define DBGF(...) do { } while (0)
#endif

#endif // DEBUG_H
