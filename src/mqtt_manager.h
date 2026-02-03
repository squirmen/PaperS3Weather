#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "utils.h"
#include <Arduino.h>

// Fetch weather data from MQTT
// Returns true if data was successfully fetched and parsed
bool fetchMQTTWeatherData(String server, int port);

#endif // MQTT_MANAGER_H
