/*
   Adapted from Bastelschlumpf/M5PaperWeather for M5Paper S3
   Modified to use M5Unified instead of M5EPD
   Uses Open-Meteo API instead of OpenWeatherMap
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>
#include "Icons.h"

// Configuration
#define WIFI_TIMEOUT_MS 20000
#define REFRESH_INTERVAL_DAY_MS 600000
#define REFRESH_INTERVAL_NIGHT_MS 3600000
#define NIGHT_START_HOUR 22
#define NIGHT_END_HOUR 5
#define VERSION "Version 1.11"

// Preferences
Preferences preferences;
bool useCelsius = false;
bool nightModeSleep = true;
String cityName = "Auckland";

// Runtime state
unsigned long lastRefreshTime = 0;
int refreshCounter = 0;

// Weather data structure
#define MAX_HOURLY 8
#define MAX_FORECAST 7

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
    } hourly[MAX_HOURLY];

    float forecastMaxTemp[MAX_FORECAST];
    float forecastMinTemp[MAX_FORECAST];
    float forecastRain[MAX_FORECAST];
    float forecastHumidity[MAX_FORECAST];
    float forecastPressure[MAX_FORECAST];

    float todayMinTemp;
    float todayMaxTemp;
};

WeatherData currentWeather;

M5Canvas canvas(&M5.Display);

const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;

#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif

// Function declarations
void setupWiFi();
void startConfigPortal();
bool fetchWeatherData(float latitude, float longitude);
bool geocodeCity(String cityName, float &latitude, float &longitude);
void displayWeather();
bool isNightTime();
unsigned long getRefreshInterval();
void enterDeepSleep(unsigned long sleepTimeMs);
void drawIcon(int x, int y, const uint8_t *icon, int dx = 64, int dy = 64, bool highContrast = false);
void drawRSSI(int x, int y, int rssi);
void drawBattery(int x, int y, int batteryPercent);
void drawArrow(int x, int y, int asize, float aangle, int pwidth, int plength);
void drawCurrentConditions(int x, int y, int dx, int dy);
void drawSunInfo(int x, int y, int dx, int dy);
void drawWindInfo(int x, int y, int dx, int dy);
void drawM5PaperInfo(int x, int y, int dx, int dy);
void drawWindCompass(int x, int y, float angle, float windspeed, int radius);
void drawGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float values[]);
void drawTempGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float highValues[], float lowValues[]);
void drawHourlyForecast(int x, int y, int dx, int dy, int index);
const uint8_t* getWeatherIcon(int weatherCode, bool isDaytime);
bool isDaytime(int hour);
String formatTemp(float temp);
float convertTemp(float temp);
String getWeatherConditionText(int weatherCode);
float getMoonPhase();
float readSHT30Temperature();
float readSHT30Humidity();

void setup() {
    M5.begin();
    M5.Display.begin();
    Serial.begin(115200);

    Serial.println("\n=================================");
    Serial.println("PaperS3Weather");
    Serial.println("Based on Bastelschlumpf design");
    Serial.println("=================================");

    M5.Display.setRotation(1);
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("PaperS3Weather");
    M5.Display.setCursor(20, 50);
    M5.Display.println("Initializing...");
    M5.Display.endWrite();
    M5.Display.display();

    Serial.println("Splash screen displayed");
    delay(2000);

    setupWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        configTime(13 * 3600, 0, "pool.ntp.org", "time.nist.gov");
        Serial.println("Time configured via NTP");
    }

    if (WiFi.status() == WL_CONNECTED) {
        preferences.begin("weather", true);
        String latStr = preferences.getString("latitude", "-36.8485");
        String lonStr = preferences.getString("longitude", "174.7633");
        cityName = preferences.getString("city", "Auckland");
        String tempUnit = preferences.getString("tempunit", "F");
        useCelsius = (tempUnit == "C");
        nightModeSleep = preferences.getBool("nightmode", true);
        preferences.end();

        float latitude = latStr.toFloat();
        float longitude = lonStr.toFloat();

        if (latStr.length() == 0 || lonStr.length() == 0 || (latitude == 0.0 && longitude == 0.0)) {
            Serial.println("No coordinates found, geocoding city: " + cityName);
            if (geocodeCity(cityName, latitude, longitude)) {
                preferences.begin("weather", false);
                preferences.putString("latitude", String(latitude, 4));
                preferences.putString("longitude", String(longitude, 4));
                preferences.end();
                Serial.printf("Geocoded %s to %.4f, %.4f\n", cityName.c_str(), latitude, longitude);
            } else {
                Serial.println("Geocoding failed, using defaults");
                latitude = -36.8485;
                longitude = 174.7633;
            }
        }

        Serial.printf("Fetching weather for: %.4f, %.4f (%s)\n", latitude, longitude, cityName.c_str());
        Serial.printf("Temperature unit: %s\n", useCelsius ? "Celsius" : "Fahrenheit");

        if (fetchWeatherData(latitude, longitude)) {
            Serial.println("Weather fetch successful!");
            displayWeather();
            lastRefreshTime = millis();
        } else {
            Serial.println("Weather fetch failed!");
            M5.Display.fillScreen(TFT_WHITE);
            M5.Display.setTextColor(TFT_BLACK);
            M5.Display.setCursor(20, 20);
            M5.Display.println("Failed to fetch weather");
            M5.Display.display();
            lastRefreshTime = millis() - REFRESH_INTERVAL_DAY_MS + 60000;
        }
    } else {
        M5.Display.fillScreen(TFT_WHITE);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(20, 20);
        M5.Display.println("No WiFi - Touch to configure");
        M5.Display.display();
        lastRefreshTime = millis();
    }

    Serial.println("Setup complete!");
}

void loop() {
    static bool hasDelayed = false;

    if (!hasDelayed) {
        delay(3000);
        hasDelayed = true;
    }

    unsigned long sleepTime = getRefreshInterval();

    Serial.println("=================================");
    Serial.printf("Night mode: %s\n", nightModeSleep ? "ENABLED" : "DISABLED");
    Serial.printf("Current time is: %s\n", isNightTime() ? "NIGHT (10pm-5am)" : "DAY (5am-10pm)");
    Serial.printf("Refresh interval: %lu minutes\n", sleepTime / 60000);
    Serial.printf("Refresh counter: %d/6\n", refreshCounter % 6);
    Serial.println("=================================");

    enterDeepSleep(sleepTime);
}

void setupWiFi() {
    preferences.begin("weather", false);
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    preferences.end();

    if (ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.println(WiFi.localIP());
            return;
        }
    }

    startConfigPortal();
}

void startConfigPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("M5Paper-Weather", "configure");

    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(20, 20);
    M5.Display.println("Config Mode");
    M5.Display.println("Connect to: M5Paper-Weather");
    M5.Display.println("Password: configure");
    M5.Display.println("Open: 192.168.4.1");
    M5.Display.endWrite();
    M5.Display.display();

    WebServer server(80);

    server.on("/", HTTP_GET, [&server]() {
        String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'>";
        html += "<style>body{font-family:Arial;margin:20px;max-width:500px}";
        html += "input,select{width:100%;padding:10px;margin:5px 0;box-sizing:border-box}";
        html += "button{width:100%;padding:15px;background:#4CAF50;color:white;border:none;margin-top:10px}";
        html += ".note{font-size:12px;color:#666;margin:5px 0}</style>";
        html += "</head><body><h1>PaperS3Weather Config</h1>";
        html += "<form action='/save' method='POST'>";
        html += "<label>WiFi SSID:</label><input name='ssid' required><br>";
        html += "<label>WiFi Password:</label><input type='password' name='password' required><br>";
        html += "<hr><h3>Location</h3>";
        html += "<label>City Name:</label><input name='city' value='Auckland' required>";
        html += "<div class='note'>City coordinates will be looked up automatically</div><br>";
        html += "<label>OR Manual Coordinates (optional):</label>";
        html += "<input name='lat' placeholder='Latitude (e.g., -36.8485)'>";
        html += "<input name='lon' placeholder='Longitude (e.g., 174.7633)'>";
        html += "<div class='note'>Leave blank to use city name</div><br>";
        html += "<hr><h3>Preferences</h3>";
        html += "<label>Temperature Unit:</label><select name='tempunit'>";
        html += "<option value='F'>Fahrenheit (F)</option>";
        html += "<option value='C'>Celsius (C)</option>";
        html += "</select><br>";
        html += "<label><input type='checkbox' name='nightmode' value='1' checked> Night Mode</label>";
        html += "<div class='note'>Reduces refresh rate 10pm-5am for battery life</div><br>";
        html += "<button type='submit'>Save & Restart</button></form></body></html>";
        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, [&server]() {
        String city = server.arg("city");
        String lat = server.arg("lat");
        String lon = server.arg("lon");

        preferences.begin("weather", false);
        preferences.putString("ssid", server.arg("ssid"));
        preferences.putString("password", server.arg("password"));
        preferences.putString("latitude", lat);
        preferences.putString("longitude", lon);
        preferences.putString("city", city);
        preferences.putString("tempunit", server.arg("tempunit"));
        preferences.putBool("nightmode", server.arg("nightmode") == "1");
        preferences.end();

        server.send(200, "text/html", "<html><body><h1>Saved! Restarting...</h1><p>Connecting to WiFi and looking up location...</p></body></html>");
        delay(2000);
        ESP.restart();
    });

    server.begin();

    while (true) {
        server.handleClient();
        M5.update();
        if (M5.BtnA.wasPressed()) break;
        delay(10);
    }

    server.stop();
    WiFi.mode(WIFI_STA);
}

bool fetchWeatherData(float latitude, float longitude) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?";
    url += "latitude=" + String(latitude, 4);
    url += "&longitude=" + String(longitude, 4);
    url += "&current=temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,wind_speed_10m,wind_direction_10m,weather_code";
    url += "&hourly=temperature_2m,precipitation_probability,relative_humidity_2m,pressure_msl,uv_index,weather_code";
    url += "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,relative_humidity_2m_mean,pressure_msl_mean,sunrise,sunset";
    url += useCelsius ? "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm" :
                        "&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch";
    url += "&timezone=auto&forecast_days=7";

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            // Extract current conditions (timestamp-specific real-time data)
            currentWeather.temperature = doc["current"]["temperature_2m"];
            currentWeather.apparentTemperature = doc["current"]["apparent_temperature"];
            currentWeather.humidity = doc["current"]["relative_humidity_2m"];
            currentWeather.windSpeed = doc["current"]["wind_speed_10m"];
            currentWeather.windDir = doc["current"]["wind_direction_10m"];
            currentWeather.precipitation = doc["current"]["precipitation"];
            currentWeather.weatherCode = doc["current"]["weather_code"];

            Serial.println("\n=== Current Conditions from API ===");
            Serial.printf("Temperature: %.1f\n", currentWeather.temperature);
            Serial.printf("Feels Like: %.1f\n", currentWeather.apparentTemperature);
            Serial.printf("Humidity: %.0f%%\n", currentWeather.humidity);
            Serial.printf("Wind: %.1f @ %.0f°\n", currentWeather.windSpeed, currentWeather.windDir);
            Serial.printf("Weather Code: %d\n", currentWeather.weatherCode);

            String sunriseStr = doc["daily"]["sunrise"][0].as<String>();
            String sunsetStr = doc["daily"]["sunset"][0].as<String>();
            if (sunriseStr.length() > 10) {
                currentWeather.sunriseTime = sunriseStr.substring(11, 16);
            }
            if (sunsetStr.length() > 10) {
                currentWeather.sunsetTime = sunsetStr.substring(11, 16);
            }

            // Get current hour to use as offset into API hourly arrays
            // Open-Meteo returns hourly data starting from 00:00 of current day
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo)) {
                timeinfo.tm_hour = 0;
            }
            int currentHourOffset = timeinfo.tm_hour;

            JsonArray hourlyTemp = doc["hourly"]["temperature_2m"];
            JsonArray hourlyPrecip = doc["hourly"]["precipitation_probability"];
            JsonArray hourlyHumidity = doc["hourly"]["relative_humidity_2m"];
            JsonArray hourlyPressure = doc["hourly"]["pressure_msl"];
            JsonArray hourlyUV = doc["hourly"]["uv_index"];
            JsonArray hourlyWeatherCode = doc["hourly"]["weather_code"];

            Serial.printf("Current hour: %d, using as offset into API arrays\n", currentHourOffset);

            for (int i = 0; i < MAX_HOURLY && (currentHourOffset + i) < hourlyTemp.size(); i++) {
                int apiIndex = currentHourOffset + i;
                currentWeather.hourly[i].temp = hourlyTemp[apiIndex];
                currentWeather.hourly[i].precip = hourlyPrecip[apiIndex];
                currentWeather.hourly[i].humidity = hourlyHumidity[apiIndex];
                currentWeather.hourly[i].pressure = hourlyPressure[apiIndex];
                currentWeather.hourly[i].uvIndex = (apiIndex < hourlyUV.size()) ? hourlyUV[apiIndex].as<float>() : 0.0f;
                currentWeather.hourly[i].weatherCode = hourlyWeatherCode[apiIndex];
                Serial.printf("Hour %d (API index %d) UV: %.1f\n", i, apiIndex, currentWeather.hourly[i].uvIndex);
            }

            JsonArray dailyMax = doc["daily"]["temperature_2m_max"];
            JsonArray dailyMin = doc["daily"]["temperature_2m_min"];
            JsonArray dailyRain = doc["daily"]["precipitation_sum"];
            JsonArray dailyHumid = doc["daily"]["relative_humidity_2m_mean"];
            JsonArray dailyPressure = doc["daily"]["pressure_msl_mean"];

            if (dailyMax.size() > 0 && dailyMin.size() > 0) {
                currentWeather.todayMinTemp = dailyMin[0];
                currentWeather.todayMaxTemp = dailyMax[0];
            }

            for (int i = 0; i < MAX_FORECAST && i < dailyMax.size(); i++) {
                currentWeather.forecastMaxTemp[i] = dailyMax[i];
                currentWeather.forecastMinTemp[i] = dailyMin[i];
                currentWeather.forecastRain[i] = dailyRain[i];
                currentWeather.forecastHumidity[i] = dailyHumid[i];
                if (i < dailyPressure.size()) {
                    currentWeather.forecastPressure[i] = dailyPressure[i];
                }
            }

            http.end();
            return true;
        }
    }

    http.end();
    return false;
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

bool geocodeCity(String cityName, float &latitude, float &longitude) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = "https://geocoding-api.open-meteo.com/v1/search?name=";
    url += urlEncode(cityName);
    url += "&count=1&language=en&format=json";

    Serial.println("Geocoding city: " + cityName);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc["results"].size() > 0) {
            latitude = doc["results"][0]["latitude"];
            longitude = doc["results"][0]["longitude"];
            String foundCity = doc["results"][0]["name"].as<String>();
            String country = doc["results"][0]["country"].as<String>();

            Serial.printf("Found: %s, %s at %.4f, %.4f\n",
                         foundCity.c_str(), country.c_str(), latitude, longitude);

            http.end();
            return true;
        }
    }

    Serial.println("Geocoding failed");
    http.end();
    return false;
}

void displayWeather() {
    M5.Display.startWrite();
    canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas.fillSprite(TFT_WHITE);
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(2);

    canvas.setTextSize(2);
    canvas.drawString(VERSION, 20, 10);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(cityName, SCREEN_WIDTH / 2, 10);
    canvas.setTextDatum(TL_DATUM);

    int rssi = WiFi.RSSI();
    int quality = 2 * (rssi + 100);
    if (quality > 100) quality = 100;
    if (quality < 0) quality = 0;
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(quality) + "%", SCREEN_WIDTH - 153, 10);
    canvas.setTextDatum(TL_DATUM);
    drawRSSI(SCREEN_WIDTH - 147, 23, rssi);

    int batteryPercent = M5.Power.getBatteryLevel();
    if (batteryPercent < 0) batteryPercent = 0;
    if (batteryPercent > 100) batteryPercent = 100;

    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(String(batteryPercent) + "%", SCREEN_WIDTH - 71, 10);
    canvas.setTextDatum(TL_DATUM);
    drawBattery(SCREEN_WIDTH - 60, 10, batteryPercent);

    canvas.setTextSize(1);
    canvas.drawString("[CFG]", SCREEN_WIDTH - 50, SCREEN_HEIGHT - 20);

    canvas.drawRect(14, 34, SCREEN_WIDTH - 28, SCREEN_HEIGHT - 43, TFT_BLACK);

    canvas.drawRect(15, 35, SCREEN_WIDTH - 30, 251, TFT_BLACK);
    canvas.drawLine(232, 35, 232, 286, TFT_BLACK);
    canvas.drawLine(465, 35, 465, 286, TFT_BLACK);
    canvas.drawLine(697, 35, 697, 286, TFT_BLACK);

    drawCurrentConditions(15, 35, 217, 251);
    drawWindInfo(232, 35, 233, 251);
    drawSunInfo(465, 35, 232, 251);
    drawM5PaperInfo(697, 35, 245, 251);

    canvas.drawRect(15, 286, SCREEN_WIDTH - 30, 122, TFT_BLACK);
    for (int i = 0; i < MAX_HOURLY; i++) {
        int x = 15 + i * 116;
        canvas.drawLine(x, 286, x, 408, TFT_BLACK);
        drawHourlyForecast(x, 286, 116, 122, i);
    }

    canvas.drawRect(15, 408, SCREEN_WIDTH - 30, 122, TFT_BLACK);

    float hourlyUVArray[MAX_HOURLY];
    float hourlyPrecipArray[MAX_HOURLY];
    float hourlyHumidityArray[MAX_HOURLY];
    float hourlyPressureArray[MAX_HOURLY];

    for (int i = 0; i < MAX_HOURLY; i++) {
        hourlyUVArray[i] = currentWeather.hourly[i].uvIndex;
        hourlyPrecipArray[i] = currentWeather.hourly[i].precip;
        hourlyHumidityArray[i] = currentWeather.hourly[i].humidity;
        hourlyPressureArray[i] = currentWeather.hourly[i].pressure;
    }

    drawGraph(15, 408, 232, 122, "UV Index", 0, 7, 0, 12, hourlyUVArray);
    drawGraph(247, 408, 232, 122, "Precip (%)", 0, 7, 0, 100, hourlyPrecipArray);
    drawGraph(479, 408, 232, 122, "Humidity (%)", 0, 7, 0, 100, hourlyHumidityArray);
    drawGraph(711, 408, 232, 122, "Pressure (hPa)", 0, 7, 980, 1040, hourlyPressureArray);

    canvas.pushSprite(0, 0);
    canvas.deleteSprite();

    M5.Display.endWrite();
    M5.Display.display();
}

void drawWindCompass(int x, int y, float angle, float windspeed, int radius) {
    int dxo, dyo, dxi, dyi;

    canvas.setTextSize(2);
    canvas.drawCircle(x, y, radius, TFT_BLACK);
    canvas.drawCircle(x, y, radius + 1, TFT_BLACK);
    canvas.drawCircle(x, y, radius * 0.7, TFT_BLACK);

    for (float a = 0; a < 360; a += 22.5) {
        dxo = radius * cos((a - 90) * PI / 180);
        dyo = radius * sin((a - 90) * PI / 180);

        dxi = dxo * 0.9;
        dyi = dyo * 0.9;
        canvas.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, TFT_BLACK);

        dxo = dxo * 0.7;
        dyo = dyo * 0.7;
        dxi = dxo * 0.9;
        dyi = dyo * 0.9;
        canvas.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, TFT_BLACK);
    }

    int labelOffset = radius + 18;
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("N", x, y - labelOffset);
    canvas.drawString("S", x, y + labelOffset - 8);

    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("W", x - labelOffset, y);
    canvas.drawString("E", x + labelOffset, y);

    int diagOffset = (int)(labelOffset * 0.707);
    canvas.setTextDatum(BR_DATUM);
    canvas.drawString("NE", x + diagOffset + 10, y - diagOffset);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString("SE", x + diagOffset + 10, y + diagOffset);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawString("SW", x - diagOffset - 10, y + diagOffset);
    canvas.setTextDatum(BL_DATUM);
    canvas.drawString("NW", x - diagOffset - 10, y - diagOffset);

    String speedUnit = useCelsius ? "km/h" : "mph";
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(String(windspeed, 1), x, y - 20);
    canvas.drawString(speedUnit, x, y);
    canvas.setTextDatum(TL_DATUM);

    drawArrow(x, y, radius - 17, angle, 15, 27);
}

void drawHourlyForecast(int x, int y, int dx, int dy, int index) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        timeinfo.tm_hour = 0;
    }
    int forecastHour = (timeinfo.tm_hour + index + 1) % 24;

    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    char hourStr[6];
    sprintf(hourStr, "%02d:00", forecastHour);
    canvas.drawString(hourStr, x + dx / 2, y + 10);
    canvas.drawString(formatTemp(currentWeather.hourly[index].temp), x + dx / 2, y + 30);
    canvas.setTextDatum(TL_DATUM);

    bool isDay = isDaytime(forecastHour);
    int iconX = x + dx / 2 - 32;
    int iconY = y + 50;
    const uint8_t* weatherIcon = getWeatherIcon(currentWeather.hourly[index].weatherCode, isDay);
    drawIcon(iconX, iconY, weatherIcon, 64, 64, true);
}

void drawGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float values[]) {
    String yMinString = String((int)yMin);
    String yMaxString = String((int)yMax);
    int textWidth = 5 + max(yMinString.length() * 3.5, yMaxString.length() * 3.5);

    int graphX = x + 5 + textWidth + 5;
    int graphY = y + 35;
    int graphDX = dx - textWidth - 20;
    int graphDY = dy - 35 - 20;
    float xStep = graphDX / (float)(xMax - xMin);
    float yStep = graphDY / (yMax - yMin);

    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(title, x + dx / 2, y + 10);
    canvas.setTextDatum(TL_DATUM);

    canvas.setTextSize(1);
    canvas.drawString(yMaxString, x + 5, graphY - 5);
    canvas.drawString(yMinString, x + 5, graphY + graphDY - 3);

    for (int i = 0; i <= (xMax - xMin); i++) {
        canvas.drawString(String(i), graphX + i * xStep, graphY + graphDY + 5);
    }

    canvas.drawRect(graphX, graphY, graphDX, graphDY, TFT_BLACK);

    if (yMin < 0 && yMax > 0) {
        float yValueDX = (float)graphDY / (yMax - yMin);
        int yPos = graphY + graphDY - (0.0 - yMin) * yValueDX;
        if (yPos > graphY && yPos < graphY + graphDY) {
            canvas.drawString("0", graphX - 20, yPos);
            for (int xDash = graphX; xDash < graphX + graphDX - 10; xDash += 10) {
                canvas.drawLine(xDash, yPos, xDash + 5, yPos, TFT_BLACK);
            }
        }
    }

    int lastX = -1, lastY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = values[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, 2, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastX, lastY, xPos, yPos, TFT_BLACK);
        }
        lastX = xPos;
        lastY = yPos;
    }
}

void drawTempGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float highValues[], float lowValues[]) {
    String yMinString = String((int)yMin);
    String yMaxString = String((int)yMax);
    int textWidth = 5 + max(yMinString.length() * 3.5, yMaxString.length() * 3.5);

    int graphX = x + 5 + textWidth + 5;
    int graphY = y + 35;
    int graphDX = dx - textWidth - 20;
    int graphDY = dy - 35 - 20;
    float xStep = graphDX / (float)(xMax - xMin);
    float yStep = graphDY / (yMax - yMin);

    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(title, x + dx / 2, y + 10);
    canvas.setTextDatum(TL_DATUM);

    canvas.setTextSize(1);
    canvas.drawString(yMaxString, x + 5, graphY - 5);
    canvas.drawString(yMinString, x + 5, graphY + graphDY - 3);

    for (int i = 0; i <= (xMax - xMin); i++) {
        canvas.drawString(String(i), graphX + i * xStep, graphY + graphDY + 5);
    }

    canvas.drawRect(graphX, graphY, graphDX, graphDY, TFT_BLACK);

    int lastHighX = -1, lastHighY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = highValues[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, 2, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastHighX, lastHighY, xPos, yPos, TFT_BLACK);
        }
        lastHighX = xPos;
        lastHighY = yPos;
    }

    int lastLowX = -1, lastLowY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float yValue = lowValues[i - xMin];
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (yValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, 2, TFT_BLACK);
        if (i > xMin) {
            canvas.drawLine(lastLowX, lastLowY, xPos, yPos, TFT_BLACK);
        }
        lastLowX = xPos;
        lastLowY = yPos;
    }

    int lastAvgX = -1, lastAvgY = -1;
    for (int i = xMin; i <= xMax; i++) {
        float avgValue = (highValues[i - xMin] + lowValues[i - xMin]) / 2.0;
        float yValueDY = (float)graphDY / (yMax - yMin);
        int xPos = graphX + graphDX / (xMax - xMin) * i;
        int yPos = graphY + graphDY - (avgValue - yMin) * yValueDY;

        if (yPos > graphY + graphDY) yPos = graphY + graphDY;
        if (yPos < graphY) yPos = graphY;

        canvas.fillCircle(xPos, yPos, 1, TFT_BLACK);
        if (i > xMin) {
            int dx = xPos - lastAvgX;
            int dy = yPos - lastAvgY;
            float len = sqrt(dx*dx + dy*dy);
            for (float t = 0; t < len; t += 5) {
                int px = lastAvgX + (dx * t / len);
                int py = lastAvgY + (dy * t / len);
                canvas.drawPixel(px, py, TFT_BLACK);
            }
        }
        lastAvgX = xPos;
        lastAvgY = yPos;
    }
}

float convertTemp(float temp) {
    return temp;
}

String formatTemp(float temp) {
    return String((int)temp) + (useCelsius ? "C" : "F");
}

void drawIcon(int x, int y, const uint8_t *icon, int dx, int dy, bool highContrast) {
    const uint16_t *icon16 = (const uint16_t *)icon;
    for (int yi = 0; yi < dy; yi++) {
        for (int xi = 0; xi < dx; xi++) {
            uint16_t pixel = icon16[yi * dx + xi];
            int grayscale = 15 - (pixel / 4096);

            if (highContrast) {
                if (grayscale > 0) {
                    canvas.drawPixel(x + xi, y + yi, TFT_BLACK);
                }
            } else {
                uint16_t color = 0xFFFF - (grayscale * 0x1111);
                canvas.drawPixel(x + xi, y + yi, color);
            }
        }
    }
}

void drawRSSI(int x, int y, int rssi) {
    int quality = 2 * (rssi + 100);
    if (quality > 100) quality = 100;
    if (quality < 0) quality = 0;

    auto drawArc = [&](int cx, int cy, int r, int fromDeg, int toDeg) {
        for (int i = fromDeg; i < toDeg; i++) {
            double rad = i * PI / 180;
            int px = cx + r * cos(rad);
            int py = cy + r * sin(rad);
            canvas.drawPixel(px, py, TFT_BLACK);
        }
    };

    if (quality >= 80) drawArc(x + 12, y, 16, 225, 315);
    if (quality >= 40) drawArc(x + 12, y, 12, 225, 315);
    if (quality >= 20) drawArc(x + 12, y, 8, 225, 315);
    if (quality >= 10) drawArc(x + 12, y, 4, 225, 315);
    drawArc(x + 12, y, 2, 225, 315);
}

void drawBattery(int x, int y, int batteryPercent) {
    canvas.drawRect(x, y, 40, 16, TFT_BLACK);
    canvas.drawRect(x + 40, y + 3, 4, 10, TFT_BLACK);

    for (int i = x; i < x + 40; i++) {
        canvas.drawLine(i, y, i, y + 15, TFT_BLACK);
        if ((i - x) * 100.0 / 40.0 > batteryPercent) {
            break;
        }
    }
}

void drawArrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
    float dx = (asize + 21) * cos((aangle - 90) * PI / 180) + x;
    float dy = (asize + 21) * sin((aangle - 90) * PI / 180) + y;
    float x1 = 0;           float y1 = plength;
    float x2 = pwidth / 2;  float y2 = pwidth / 2;
    float x3 = -pwidth / 2; float y3 = pwidth / 2;
    float angle = aangle * PI / 180;
    float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
    float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
    float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
    float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
    float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
    float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
    canvas.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, TFT_BLACK);
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
    int sunriseHour = 6, sunsetHour = 18;
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

    double daysSinceNew = jd - 2451550.26;
    double newMoons = daysSinceNew / 29.53058867;
    double phase = newMoons - floor(newMoons);

    return phase;
}

float readSHT30Temperature() {
    float temp = -999.0;
    M5.Imu.update();
    if (M5.Imu.getTemp(&temp)) {
        return temp;
    }
    return -999.0;
}

float readSHT30Humidity() {
    return -999.0;
}

void drawCurrentConditions(int x, int y, int dx, int dy) {
    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("Current", x + dx / 2, y + 7);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawLine(x, y + 35, x + dx, y + 35, TFT_BLACK);

    int spacing = dx / 4;

    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);

    String tempNum = String((int)currentWeather.temperature);
    int mainTempX = x + spacing;
    int mainTempY = y + 50;
    canvas.drawString(tempNum, mainTempX, mainTempY);

    int tempWidth = canvas.textWidth(tempNum) * 2;
    int degreeX = mainTempX + tempWidth / 2 - 42;
    int degreeY = mainTempY + 13;
    canvas.drawCircle(degreeX, degreeY, 8, TFT_BLACK);
    canvas.drawCircle(degreeX, degreeY, 7, TFT_BLACK);

    canvas.setFont(nullptr);
    canvas.setTextFont(1);
    canvas.setTextSize(1);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        timeinfo.tm_hour = 12;
    }
    bool isDay = isDaytime(timeinfo.tm_hour);

    int iconX = x + dx - spacing - 25;
    int iconY = mainTempY - 2;
    const uint8_t* weatherIcon = getWeatherIcon(currentWeather.weatherCode, isDay);
    drawIcon(iconX, iconY, weatherIcon, 64, 64, true);

    String condition = getWeatherConditionText(currentWeather.weatherCode);
    canvas.setTextDatum(TC_DATUM);

    int conditionY = mainTempY + 90;
    int availableWidth = dx - 20;
    int textWidth = canvas.textWidth(condition) * 3;

    if (textWidth > availableWidth) {
        canvas.setTextSize(2);
    } else {
        canvas.setTextSize(3);
    }
    canvas.drawString(condition, x + dx / 2, conditionY);

    int feelsLikeY = y + 185;

    // Draw "Feels Like:" label
    canvas.setTextSize(2);
    canvas.setTextDatum(TL_DATUM);
    String feelsLikeLabel = "Feels Like:";
    int labelX = x + spacing - 35;
    canvas.drawString(feelsLikeLabel, labelX, feelsLikeY);

    // Calculate label width and position temperature
    int labelWidth = canvas.textWidth(feelsLikeLabel) * 2;
    int feelsLikeTempX = labelX + labelWidth - 130;

    // Draw temperature at size 4 with same Y alignment as label
    canvas.setTextSize(4);
    canvas.setTextDatum(TL_DATUM);
    String feelsTemp = String((int)currentWeather.apparentTemperature);
    canvas.drawString(feelsTemp, feelsLikeTempX, feelsLikeY - 8);

    // Position degree symbol right next to the temperature number
    // textWidth() at size 4 already accounts for the scaling
    int tempNumWidth = canvas.textWidth(feelsTemp);
    int feelsLikeDegX = feelsLikeTempX + tempNumWidth + 3;  // Small gap after temp
    int feelsLikeDegY = feelsLikeY - 3;
    canvas.drawCircle(feelsLikeDegX, feelsLikeDegY, 5, TFT_BLACK);
    canvas.drawCircle(feelsLikeDegX, feelsLikeDegY, 4, TFT_BLACK);

    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);

    String lowTemp = "L:" + String((int)currentWeather.todayMinTemp);
    String highTemp = "H:" + String((int)currentWeather.todayMaxTemp);

    int tempTextY = y + 220;

    canvas.drawString(lowTemp, x + spacing, tempTextY);
    canvas.drawString(highTemp, x + dx - spacing, tempTextY);

    int lowDegX = x + spacing + canvas.textWidth(lowTemp) / 2 + 5;
    int highDegX = x + dx - spacing + canvas.textWidth(highTemp) / 2 + 5;
    int degY = tempTextY + 1;

    canvas.drawCircle(lowDegX, degY, 3, TFT_BLACK);
    canvas.drawCircle(lowDegX, degY, 2, TFT_BLACK);
    canvas.drawCircle(highDegX, degY, 3, TFT_BLACK);
    canvas.drawCircle(highDegX, degY, 2, TFT_BLACK);

    canvas.setTextDatum(TL_DATUM);
}

void drawSunInfo(int x, int y, int dx, int dy) {
    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("Sun & Moon", x + dx / 2, y + 7);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawLine(x, y + 35, x + dx, y + 35, TFT_BLACK);

    canvas.setTextSize(3);
    drawIcon(x + 25, y + 50, SUNRISE64x64, 64, 64, false);
    if (currentWeather.sunriseTime.length() > 0) {
        canvas.drawString(currentWeather.sunriseTime, x + 100, y + 75);
    }

    drawIcon(x + 25, y + 125, SUNSET64x64, 64, 64, false);
    if (currentWeather.sunsetTime.length() > 0) {
        canvas.drawString(currentWeather.sunsetTime, x + 100, y + 150);
    }

    float moonPhase = getMoonPhase();
    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    String phaseText = "";
    if (moonPhase < 0.05 || moonPhase > 0.95) phaseText = "New";
    else if (moonPhase < 0.20) phaseText = "Waxing Cres";
    else if (moonPhase < 0.30) phaseText = "First Qtr";
    else if (moonPhase < 0.45) phaseText = "Waxing Gib";
    else if (moonPhase < 0.55) phaseText = "Full";
    else if (moonPhase < 0.70) phaseText = "Waning Gib";
    else if (moonPhase < 0.80) phaseText = "Last Qtr";
    else phaseText = "Waning Cres";

    canvas.drawString("Moon: " + phaseText, x + dx / 2, y + 210);
    canvas.setTextDatum(TL_DATUM);
}

void drawMoonInfo(int x, int y, int dx, int dy) {
    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("Moon", x + dx / 2, y + 7);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawLine(x, y + 35, x + dx, y + 35, TFT_BLACK);

    canvas.setTextSize(2);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("Phase", x + dx / 2, y + 80);
    canvas.setTextDatum(TL_DATUM);

    int moonX = x + dx / 2;
    int moonY = y + 150;
    canvas.drawCircle(moonX, moonY, 30, TFT_BLACK);
    canvas.drawCircle(moonX, moonY, 29, TFT_BLACK);
    canvas.fillCircle(moonX, moonY, 28, TFT_BLACK);
}

void drawWindInfo(int x, int y, int dx, int dy) {
    canvas.setTextSize(3);
    canvas.drawString("Wind", x + dx / 2 - 40, y + 7);
    canvas.drawLine(x, y + 35, x + dx, y + 35, TFT_BLACK);

    drawWindCompass(x + dx / 2, y + dy / 2 + 20, currentWeather.windDir, currentWeather.windSpeed, 75);
}

void drawM5PaperInfo(int x, int y, int dx, int dy) {
    canvas.setTextSize(3);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString("M5Paper S3", x + dx / 2, y + 7);
    canvas.setTextDatum(TL_DATUM);
    canvas.drawLine(x, y + 35, x + dx, y + 35, TFT_BLACK);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char dateStr[16], timeStr[16];
        sprintf(dateStr, "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        canvas.setTextSize(3);
        canvas.setTextDatum(TC_DATUM);
        canvas.drawString(dateStr, x + dx / 2, y + 55);
        canvas.drawString(timeStr, x + dx / 2, y + 95);
        canvas.setTextDatum(TL_DATUM);

        canvas.setTextSize(2);
        canvas.setTextDatum(TC_DATUM);
        canvas.drawString("updated", x + dx / 2, y + 120);
        canvas.setTextDatum(TL_DATUM);
    }

    float sensorTemp = readSHT30Temperature();
    float sensorHumid = readSHT30Humidity();

    float displayTemp = (sensorTemp > -999) ? sensorTemp : currentWeather.temperature;
    float displayHumid = (sensorHumid > -999) ? sensorHumid : currentWeather.humidity;

    canvas.setTextSize(3);
    drawIcon(x + 35, y + 140, TEMPERATURE64x64, 64, 64, false);
    canvas.drawString(formatTemp(displayTemp), x + 35, y + 210);

    drawIcon(x + 145, y + 140, HUMIDITY64x64, 64, 64, false);
    canvas.drawString(String((int)displayHumid) + "%", x + 150, y + 210);
}

bool isNightTime() {
    if (!nightModeSleep) return false;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return false;

    int currentHour = timeinfo.tm_hour;

    if (NIGHT_START_HOUR > NIGHT_END_HOUR) {
        return (currentHour >= NIGHT_START_HOUR || currentHour < NIGHT_END_HOUR);
    } else {
        return (currentHour >= NIGHT_START_HOUR && currentHour < NIGHT_END_HOUR);
    }
}

unsigned long getRefreshInterval() {
    if (isNightTime()) {
        return REFRESH_INTERVAL_NIGHT_MS;
    } else {
        return REFRESH_INTERVAL_DAY_MS;
    }
}

void enterDeepSleep(unsigned long sleepTimeMs) {
    Serial.printf("Entering deep sleep for %lu ms (%lu minutes)\n",
                  sleepTimeMs, sleepTimeMs / 60000);

    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);

    Serial.flush();

    esp_deep_sleep_start();
}
