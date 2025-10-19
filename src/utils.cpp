#include "utils.h"
#include "constants.h"
#include "Icons.h"
#include <Preferences.h>

extern Preferences preferences;
extern bool nightModeSleep;

float convertTemp(float temp) {
    return temp;
}

String formatTemp(float temp) {
    return String((int)temp) + (useCelsius ? "C" : "F");
}

const uint8_t* getWeatherIcon(int weatherCode, bool isDaytime) {
    if (weatherCode == 0) {
        return isDaytime ? image_data_01d : image_data_01n;
    }
    if (weatherCode <= 3) {
        return isDaytime ? image_data_02d : image_data_02n;
    }
    if (weatherCode >= 45 && weatherCode <= 48) {
        return isDaytime ? image_data_50d : image_data_50n;
    }
    if (weatherCode >= 51 && weatherCode <= 67) {
        return isDaytime ? image_data_10d : image_data_10n;
    }
    if (weatherCode >= 71 && weatherCode <= 86) {
        return isDaytime ? image_data_13d : image_data_13n;
    }
    if (weatherCode >= 95) {
        return isDaytime ? image_data_11d : image_data_11n;
    }
    return image_data_unknown;
}

String getWeatherConditionText(int weatherCode) {
    if (weatherCode == 0) return "Clear";
    if (weatherCode == 1) return "Mainly Clear";
    if (weatherCode == 2) return "Partly Cloudy";
    if (weatherCode == 3) return "Overcast";
    if (weatherCode >= 45 && weatherCode <= 48) return "Foggy";
    if (weatherCode >= 51 && weatherCode <= 55) return "Drizzle";
    if (weatherCode >= 56 && weatherCode <= 57) return "Freezing Drizzle";
    if (weatherCode >= 61 && weatherCode <= 65) return "Rain";
    if (weatherCode >= 66 && weatherCode <= 67) return "Freezing Rain";
    if (weatherCode >= 71 && weatherCode <= 75) return "Snow";
    if (weatherCode >= 77 && weatherCode <= 77) return "Snow Grains";
    if (weatherCode >= 80 && weatherCode <= 82) return "Rain Showers";
    if (weatherCode >= 85 && weatherCode <= 86) return "Snow Showers";
    if (weatherCode >= 95 && weatherCode <= 96) return "Thunderstorm";
    if (weatherCode >= 99) return "Thunderstorm Hail";
    return "Unknown";
}

bool isDaytime(int hour) {
    int sunriseHour = DEFAULT_SUNRISE_HOUR;
    int sunsetHour = DEFAULT_SUNSET_HOUR;

    if (currentWeather.sunriseTime.length() >= 2) {
        sunriseHour = currentWeather.sunriseTime.substring(0, 2).toInt();
    }
    if (currentWeather.sunsetTime.length() >= 2) {
        sunsetHour = currentWeather.sunsetTime.substring(0, 2).toInt();
    }
    return (hour >= sunriseHour && hour < sunsetHour);
}

float getMoonPhase() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return 0.0;
    }

    int year = timeinfo.tm_year + 1900;
    int month = timeinfo.tm_mon + 1;
    int day = timeinfo.tm_mday;

    if (month <= 2) {
        year -= 1;
        month += 12;
    }

    int a = year / 100;
    int b = a / 4;
    int c = 2 - a + b;
    double e = floor(365.25 * (year + 4716));
    double f = floor(30.6001 * (month + 1));
    double jd = c + day + e + f - 1524.5;

    double daysSinceNew = jd - JULIAN_REF_DATE;
    double newMoons = daysSinceNew / LUNAR_CYCLE_DAYS;
    double phase = newMoons - floor(newMoons);

    return phase;
}

bool isNightTime() {
    if (!nightModeSleep) return false;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return false;

    int currentHour = timeinfo.tm_hour;

    // Load night mode hours from preferences
    preferences.begin("weather", true);
    int nightStart = preferences.getInt("night_start", NIGHT_START_HOUR);
    int nightEnd = preferences.getInt("night_end", NIGHT_END_HOUR);
    preferences.end();

    if (nightStart > nightEnd) {
        return (currentHour >= nightStart || currentHour < nightEnd);
    } else {
        return (currentHour >= nightStart && currentHour < nightEnd);
    }
}

unsigned long getRefreshInterval() {
    // Load refresh intervals from preferences
    preferences.begin("weather", true);
    int dayInterval = preferences.getInt("day_interval", 10);
    int nightInterval = preferences.getInt("night_interval", 60);
    preferences.end();

    if (isNightTime()) {
        return nightInterval * 60000;  // Convert minutes to milliseconds
    } else {
        return dayInterval * 60000;  // Convert minutes to milliseconds
    }
}

float readInternalTemperature() {
    float temp = SENSOR_ERROR_VALUE;
    M5.Imu.update();
    if (M5.Imu.getTemp(&temp)) {
        return temp;
    }
    return SENSOR_ERROR_VALUE;
}

float readInternalHumidity() {
    // M5Paper S3 doesn't have humidity sensor in IMU
    return SENSOR_ERROR_VALUE;
}

String urlEncode(String str) {
    String encoded = "";
    char c;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            encoded += '%';
            char hex[3];
            sprintf(hex, "%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}

// Drawing helper to reduce code duplication
void drawDegreeSymbol(int x, int y, int radius) {
    for (int i = 0; i < 2; i++) {
        canvas.drawCircle(x, y, radius - i, TFT_BLACK);
    }
}

// RSSI quality calculation (reduces duplication)
int getRSSIQuality(int rssi) {
    int quality = RSSI_QUALITY_MULTIPLIER * (rssi + RSSI_QUALITY_OFFSET);
    if (quality > 100) quality = 100;
    if (quality < 0) quality = 0;
    return quality;
}
