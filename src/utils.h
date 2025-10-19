#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <M5Unified.h>
#include <M5GFX.h>

// External references
extern bool useCelsius;
extern M5Canvas canvas;

// Structure to hold weather data
struct WeatherData {
    float temperature;
    float apparentTemperature;
    float humidity;
    float windSpeed;
    float windDir;
    float precipitation;
    int weatherCode;

    String sunriseTime;
    String sunsetTime;

    struct {
        float temp;
        float precip;
        float humidity;
        float pressure;
        float uvIndex;
        int weatherCode;
    } hourly[8];  // MAX_HOURLY

    float forecastMaxTemp[7];  // MAX_FORECAST
    float forecastMinTemp[7];
    float forecastRain[7];
    float forecastHumidity[7];
    float forecastPressure[7];

    float todayMinTemp;
    float todayMaxTemp;
};

extern WeatherData currentWeather;

// Temperature conversion and formatting
float convertTemp(float tempCelsius);
String formatTemp(float temp);

// Icon and weather condition helpers
const uint8_t* getWeatherIcon(int weatherCode, bool isDaytime);
String getWeatherConditionText(int weatherCode);
bool isDaytime(int hour);

// Time and astronomical calculations
float getMoonPhase();
bool isNightTime();
unsigned long getRefreshInterval();

// Sensor reading
float readInternalTemperature();
float readInternalHumidity();

// URL encoding
String urlEncode(String str);

// Drawing helpers (to reduce duplication)
void drawDegreeSymbol(int x, int y, int radius);
int getRSSIQuality(int rssi);

#endif // UTILS_H
