/*
   Adapted from Bastelschlumpf/M5PaperWeather for M5Paper S3
   Modified to use M5Unified instead of M5EPD
   Uses Open-Meteo API instead of OpenWeatherMap

   Version 1.12 - Refactored for better modularity and performance
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>

// Include all our new modular headers
#include "constants.h"
#include "utils.h"
#include "weather_api.h"
#include "config.h"
#include "display.h"

// Global objects
Preferences preferences;
M5Canvas canvas(&M5.Display);
WeatherData currentWeather;

// Configuration state
bool useCelsius = false;
bool nightModeSleep = true;
String cityName = DEFAULT_CITY;

// Runtime state
unsigned long lastRefreshTime = 0;
int refreshCounter = 0;

void enterDeepSleep(unsigned long sleepTimeMs) {
    Serial.printf("Entering deep sleep for %lu ms (%lu minutes)\n",
                  sleepTimeMs, sleepTimeMs / 60000);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Configure wakeup timer
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000);

    // Flush serial before sleep
    Serial.flush();

    // Enter deep sleep
    esp_deep_sleep_start();
}

void setup() {
    M5.begin();
    M5.Display.begin();
    Serial.begin(115200);

    Serial.println("\n=================================");
    Serial.println("PaperS3Weather " + String(VERSION));
    Serial.println("Based on Bastelschlumpf design");
    Serial.println("=================================");

    // Configure display
    M5.Display.setRotation(1);
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("PaperS3Weather " + String(VERSION));
    M5.Display.setCursor(20, 50);
    M5.Display.println("Initializing...");
    M5.Display.endWrite();
    M5.Display.display();

    Serial.println("Splash screen displayed");
    delay(2000);

    setupWiFi();

    // Configure time via NTP if connected
    if (WiFi.status() == WL_CONNECTED) {
        configTime(TIMEZONE_OFFSET_HOURS * 3600, 0, NTP_SERVER_1, NTP_SERVER_2);
        Serial.println("Time configured via NTP");
    }

    // Load preferences and fetch weather
    if (WiFi.status() == WL_CONNECTED) {
        float latitude, longitude;
        loadPreferences(latitude, longitude, cityName);

        Serial.printf("Fetching weather for: %.4f, %.4f (%s)\n",
                     latitude, longitude, cityName.c_str());

        bool fetchSuccess = false;
        for (int retry = 0; retry < HTTP_RETRY_ATTEMPTS; retry++) {
            if (retry > 0) {
                Serial.printf("Weather fetch retry %d/%d...\n", retry + 1, HTTP_RETRY_ATTEMPTS);
                delay(HTTP_RETRY_DELAY_MS);
            }

            if (fetchWeatherData(latitude, longitude)) {
                Serial.println("Weather fetch successful!");
                displayWeather();
                lastRefreshTime = millis();
                fetchSuccess = true;
                break;
            }
        }

        if (!fetchSuccess) {
            Serial.println("All weather fetch attempts failed!");
            M5.Display.fillScreen(TFT_WHITE);
            M5.Display.setTextColor(TFT_BLACK);
            M5.Display.setCursor(20, 20);
            M5.Display.println("Failed to fetch weather");
            M5.Display.println("Will retry in 1 minute");
            M5.Display.display();
            // Retry sooner on failure
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
    static bool hasWaited = false;

    // Check WHY we woke up - only show interaction window on manual reset
    if (!hasWaited) {
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

        // ESP_SLEEP_WAKEUP_UNDEFINED means: power on or reset button pressed
        // ESP_SLEEP_WAKEUP_TIMER means: automatic wake from deep sleep
        if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
            // Manual reset/power on - give user time to interact
            Serial.println("\n*** Manual wake detected (reset button or power on) ***");
            Serial.println("*** Waiting 30 seconds for user interaction ***");
            Serial.println("*** Tap bottom-right corner of screen for CONFIG ***");

            unsigned long startWait = millis();
            const unsigned long waitDuration = USER_INTERACTION_TIMEOUT_MS;
            unsigned long lastSerialUpdate = 0;

            while (millis() - startWait < waitDuration) {
                M5.update();  // Update touch state

                // Check for touch in bottom-right corner (CFG button area)
                auto touch = M5.Touch.getDetail();
                if (touch.wasPressed()) {
                    int touchX = touch.x;
                    int touchY = touch.y;

                    // Check if touch is in CFG area (bottom-right corner)
                    if (touchX > (SCREEN_WIDTH - CFG_BUTTON_TOUCH_WIDTH) &&
                        touchY > (SCREEN_HEIGHT - CFG_BUTTON_TOUCH_HEIGHT)) {
                        Serial.println("\n*** CONFIG button pressed! ***");
                        M5.Display.startWrite();
                        M5.Display.fillScreen(TFT_WHITE);
                        M5.Display.setTextSize(2);
                        M5.Display.setCursor(20, 20);
                        M5.Display.println("Opening Configuration...");
                        M5.Display.println("\nConnect to:");
                        M5.Display.println("  M5Paper-Weather");
                        M5.Display.println("Password: configure");
                        M5.Display.println("URL: 192.168.4.1");
                        M5.Display.endWrite();
                        M5.Display.display();

                        // Disconnect from WiFi and start config portal
                        WiFi.disconnect();
                        delay(500);
                        startConfigPortal();

                        // After config, restart
                        ESP.restart();
                    }
                }

                // Update serial countdown every second
                unsigned long remaining = (waitDuration - (millis() - startWait)) / 1000;
                if (millis() - lastSerialUpdate >= 1000) {
                    Serial.printf("Waiting for config tap... %lu seconds remaining\n", remaining);
                    lastSerialUpdate = millis();
                }

                delay(100);  // Small delay to reduce CPU usage
            }

            Serial.println("*** Wait period ended, entering sleep mode ***\n");
        } else {
            // Automatic wake from timer - skip interaction window
            Serial.println("\n*** Automatic wake from timer - skipping interaction window ***");
        }

        hasWaited = true;
    }

    unsigned long sleepTime = getRefreshInterval();

    // Get current settings for display
    preferences.begin("weather", true);
    int nightStart = preferences.getInt("night_start", 22);
    int nightEnd = preferences.getInt("night_end", 5);
    preferences.end();

    Serial.println("=================================");
    Serial.printf("Night mode: %s\n", nightModeSleep ? "ENABLED" : "DISABLED");
    if (nightModeSleep) {
        Serial.printf("Night hours: %d:00 - %d:00\n", nightStart, nightEnd);
    }
    Serial.printf("Current time is: %s\n", isNightTime() ? "NIGHT" : "DAY");
    Serial.printf("Refresh interval: %lu minutes\n", sleepTime / 60000);
    Serial.printf("Refresh counter: %d/6\n", refreshCounter % 6);
    Serial.println("=================================");

    enterDeepSleep(sleepTime);
}
