#ifndef WEATHER_API_H
#define WEATHER_API_H

#include <Arduino.h>
#include "utils.h"

// Fetch weather data from Open-Meteo API with retry logic
bool fetchWeatherData(float latitude, float longitude);

// Geocode city name to coordinates
bool geocodeCity(String cityName, float &latitude, float &longitude);

#endif // WEATHER_API_H
