#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi setup with retry logic
void setupWiFi();

// Configuration portal
void startConfigPortal();

// Load preferences
void loadPreferences(float &latitude, float &longitude, String &cityName);

#endif // CONFIG_H
