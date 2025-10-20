#include "config.h"
#include "constants.h"
#include "weather_api.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>

extern Preferences preferences;
extern bool useCelsius;
extern bool nightModeSleep;
extern String cityName;

void setupWiFi() {
    preferences.begin("weather", false);
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    preferences.end();

    if (ssid.length() > 0) {
        WiFi.mode(WIFI_STA);

        for (int attempt = 0; attempt < WIFI_RETRY_ATTEMPTS; attempt++) {
            if (attempt > 0) {
                Serial.printf("WiFi retry %d/%d...\n", attempt + 1, WIFI_RETRY_ATTEMPTS);
                delay(WIFI_RETRY_DELAY_MS);
            }

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

            Serial.println("\nWiFi connection failed");
            WiFi.disconnect();
        }

        Serial.println("All WiFi connection attempts failed");
    }

    // No saved credentials or all attempts failed
    startConfigPortal();
}

void startConfigPortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASSWORD);

    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(20, 20);
    M5.Display.println("Config Mode");
    M5.Display.println("Connect to: " + String(CONFIG_AP_SSID));
    M5.Display.println("Password: " + String(CONFIG_AP_PASSWORD));
    M5.Display.println("Open: " + String(CONFIG_AP_IP));
    M5.Display.endWrite();
    M5.Display.display();

    WebServer server(80);

    server.on("/", HTTP_GET, [&server]() {
        // Load current settings including WiFi credentials
        preferences.begin("weather", true);
        String currentSSID = preferences.getString("ssid", "");
        String currentPassword = preferences.getString("password", "");
        String currentCity = preferences.getString("city", DEFAULT_CITY);
        String currentLat = preferences.getString("latitude", "");
        String currentLon = preferences.getString("longitude", "");
        String currentUnit = preferences.getString("tempunit", "F");
        bool currentNightMode = preferences.getBool("nightmode", true);
        int currentDayInterval = preferences.getInt("day_interval", 10);
        int currentNightInterval = preferences.getInt("night_interval", 60);
        int currentNightStart = preferences.getInt("night_start", 22);
        int currentNightEnd = preferences.getInt("night_end", 5);
        preferences.end();

        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<style>";
        html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;margin:0;padding:20px;background:#f5f5f5;max-width:600px;margin:0 auto}";
        html += "h1{color:#333;margin-bottom:10px}";
        html += "h3{color:#555;margin:20px 0 10px;font-size:18px;border-bottom:2px solid #4CAF50;padding-bottom:5px}";
        html += ".card{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px}";
        html += "label{display:block;margin:15px 0 5px;color:#555;font-weight:500}";
        html += "input,select{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;font-size:14px}";
        html += "input:focus,select:focus{outline:none;border-color:#4CAF50}";
        html += "input[type='checkbox']{width:auto;margin-right:8px}";
        html += "input[type='number']{width:80px;display:inline-block}";
        html += ".inline-group{display:flex;gap:10px;align-items:center}";
        html += ".inline-group label{margin:0}";
        html += "button{width:100%;padding:15px;background:#4CAF50;color:white;border:none;border-radius:4px;font-size:16px;font-weight:600;cursor:pointer;margin-top:20px}";
        html += "button:hover{background:#45a049}";
        html += ".note{font-size:12px;color:#666;margin:5px 0;line-height:1.4}";
        html += ".help{font-size:13px;color:#888;margin:5px 0;padding:8px;background:#f9f9f9;border-left:3px solid #4CAF50;border-radius:2px}";
        html += ".current{font-size:13px;color:#0066cc;padding:10px;background:#e3f2fd;border-radius:4px;margin-bottom:15px}";
        html += ".section{margin:20px 0}";
        html += "hr{border:none;border-top:1px solid #eee;margin:25px 0}";
        html += "</style>";

        html += "<script>";
        html += "function validateForm(){";
        html += "  var ssid=document.forms['config']['ssid'].value;";
        html += "  var city=document.forms['config']['city'].value;";
        html += "  var dayInt=parseInt(document.forms['config']['day_interval'].value);";
        html += "  var nightInt=parseInt(document.forms['config']['night_interval'].value);";
        html += "  var nightStart=parseInt(document.forms['config']['night_start'].value);";
        html += "  var nightEnd=parseInt(document.forms['config']['night_end'].value);";
        html += "  if(ssid==''){alert('WiFi SSID is required');return false;}";
        html += "  if(city==''){alert('City name is required');return false;}";
        html += "  if(dayInt<5||dayInt>120){alert('Day refresh must be 5-120 minutes');return false;}";
        html += "  if(nightInt<15||nightInt>240){alert('Night refresh must be 15-240 minutes');return false;}";
        html += "  if(nightStart<0||nightStart>23){alert('Night start hour must be 0-23');return false;}";
        html += "  if(nightEnd<0||nightEnd>23){alert('Night end hour must be 0-23');return false;}";
        html += "  return true;";
        html += "}";
        html += "</script>";

        html += "</head><body>";
        html += "<div class='card'>";
        html += "<h1>Weather Dashboard Setup</h1>";

        // Show current settings
        if (currentCity.length() > 0) {
            html += "<div class='current'><strong>Current Settings:</strong><br>";
            html += "Location: " + currentCity;
            if (currentLat.length() > 0) {
                html += " (" + currentLat + ", " + currentLon + ")";
            }
            html += "<br>Temperature: " + String(currentUnit == "C" ? "Celsius" : "Fahrenheit");
            html += "<br>Updates: " + String(currentDayInterval) + " min (day), " + String(currentNightInterval) + " min (night)";
            html += "<br>Night Mode: " + String(currentNightMode ? "ON" : "OFF");
            if (currentNightMode) {
                html += " (" + String(currentNightStart) + ":00 - " + String(currentNightEnd) + ":00)";
            }
            html += "</div>";
        }

        html += "<form name='config' action='/save' method='POST' onsubmit='return validateForm()'>";

        // WiFi Section
        html += "<div class='section'>";
        html += "<h3>WiFi Connection</h3>";
        html += "<label>Network Name (SSID):</label>";
        html += "<input name='ssid' value='" + currentSSID + "' required maxlength='32' placeholder='Your WiFi network'><br>";
        html += "<label>Password:</label>";
        html += "<input type='password' name='password' value='" + currentPassword + "' maxlength='64' placeholder='WiFi password'>";
        html += "<div class='note'>Note: Must be 2.4GHz WiFi (ESP32 doesn't support 5GHz)</div>";
        if (currentSSID.length() > 0) {
            html += "<div class='help'>Currently saved: " + currentSSID + "</div>";
        }
        html += "</div>";

        // Location Section
        html += "<div class='section'>";
        html += "<h3>Location</h3>";
        html += "<label>City Name:</label>";
        html += "<input name='city' value='" + currentCity + "' required placeholder='e.g., Auckland, London, New York'>";
        html += "<div class='help'>The app will automatically find your city's coordinates</div>";
        html += "<label>OR Manual Coordinates (optional):</label>";
        html += "<input name='lat' placeholder='Latitude (e.g., -36.8485)' pattern='^-?\\d+\\.?\\d*$' value='" + currentLat + "'>";
        html += "<input name='lon' placeholder='Longitude (e.g., 174.7633)' pattern='^-?\\d+\\.?\\d*$' value='" + currentLon + "'>";
        html += "<div class='note'>Leave blank to auto-lookup from city name</div>";
        html += "</div>";

        // Display Preferences
        html += "<div class='section'>";
        html += "<h3>Display Preferences</h3>";
        html += "<label>Temperature Unit:</label>";
        html += "<select name='tempunit'>";
        html += "<option value='F'" + String(currentUnit == "F" ? " selected" : "") + ">Fahrenheit</option>";
        html += "<option value='C'" + String(currentUnit == "C" ? " selected" : "") + ">Celsius</option>";
        html += "</select>";
        html += "</div>";

        // Update Schedule
        html += "<div class='section'>";
        html += "<h3>Update Schedule</h3>";
        html += "<label>Day Time Refresh (minutes):</label>";
        html += "<input type='number' name='day_interval' value='" + String(currentDayInterval) + "' min='5' max='120' required>";
        html += "<div class='help'>How often to update during the day (5-120 minutes)<br>Lower = more updates, more battery use</div>";
        html += "<label>Night Time Refresh (minutes):</label>";
        html += "<input type='number' name='night_interval' value='" + String(currentNightInterval) + "' min='15' max='240' required>";
        html += "<div class='help'>How often to update at night (15-240 minutes)<br>Longer interval saves battery while you sleep</div>";
        html += "</div>";

        // Night Mode
        html += "<div class='section'>";
        html += "<h3>Night Mode</h3>";
        html += "<label><input type='checkbox' name='nightmode' value='1'" + String(currentNightMode ? " checked" : "") + "> Enable Night Mode</label>";
        html += "<div class='help'>Reduces update frequency during night hours to save battery</div>";
        html += "<div class='inline-group'>";
        html += "<label>Start:</label><input type='number' name='night_start' value='" + String(currentNightStart) + "' min='0' max='23' required>";
        html += "<label>End:</label><input type='number' name='night_end' value='" + String(currentNightEnd) + "' min='0' max='23' required>";
        html += "<span class='note'>(24-hour format)</span>";
        html += "</div>";
        html += "<div class='note'>Example: 22 = 10 PM, 5 = 5 AM</div>";
        html += "</div>";

        html += "<button type='submit'>Save & Restart</button>";
        html += "</form>";
        html += "</div>";
        html += "</body></html>";

        server.send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, [&server]() {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        String city = server.arg("city");
        String lat = server.arg("lat");
        String lon = server.arg("lon");
        String tempUnit = server.arg("tempunit");
        bool nightMode = server.arg("nightmode") == "1";
        int dayInterval = server.arg("day_interval").toInt();
        int nightInterval = server.arg("night_interval").toInt();
        int nightStart = server.arg("night_start").toInt();
        int nightEnd = server.arg("night_end").toInt();

        if (ssid.length() == 0 || city.length() == 0) {
            server.send(400, "text/html",
                "<html><body><h1>Error</h1><p>SSID and City are required!</p>"
                "<a href='/'>Go Back</a></body></html>");
            return;
        }

        // Validate refresh intervals
        if (dayInterval < 5 || dayInterval > 120) {
            server.send(400, "text/html",
                "<html><body><h1>Error</h1><p>Day refresh interval must be 5-120 minutes!</p>"
                "<a href='/'>Go Back</a></body></html>");
            return;
        }

        if (nightInterval < 15 || nightInterval > 240) {
            server.send(400, "text/html",
                "<html><body><h1>Error</h1><p>Night refresh interval must be 15-240 minutes!</p>"
                "<a href='/'>Go Back</a></body></html>");
            return;
        }

        // Validate night mode hours
        if (nightStart < 0 || nightStart > 23 || nightEnd < 0 || nightEnd > 23) {
            server.send(400, "text/html",
                "<html><body><h1>Error</h1><p>Night hours must be 0-23!</p>"
                "<a href='/'>Go Back</a></body></html>");
            return;
        }

        if (lat.length() > 0 && lon.length() > 0) {
            float latVal = lat.toFloat();
            float lonVal = lon.toFloat();
            if (latVal < -90 || latVal > 90 || lonVal < -180 || lonVal > 180) {
                server.send(400, "text/html",
                    "<html><body><h1>Error</h1><p>Invalid coordinates! "
                    "Latitude must be -90 to 90, Longitude must be -180 to 180.</p>"
                    "<a href='/'>Go Back</a></body></html>");
                return;
            }
        }

        // Save preferences
        preferences.begin("weather", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.putString("city", city);
        preferences.putString("tempunit", tempUnit);
        preferences.putBool("nightmode", nightMode);
        preferences.putInt("day_interval", dayInterval);
        preferences.putInt("night_interval", nightInterval);
        preferences.putInt("night_start", nightStart);
        preferences.putInt("night_end", nightEnd);

        if (lat.length() > 0 && lon.length() > 0) {
            preferences.putString("latitude", lat);
            preferences.putString("longitude", lon);
        } else {
            // Clear coordinates so they'll be geocoded
            preferences.putString("latitude", String(COORD_NOT_SET));
            preferences.putString("longitude", String(COORD_NOT_SET));
        }

        preferences.end();

        server.send(200, "text/html",
            "<html><head><meta http-equiv='refresh' content='3;url=/restart'></head>"
            "<body><h1>Saved!</h1><p>Restarting in 3 seconds...</p>"
            "<p>The device will connect to your WiFi and fetch weather data.</p></body></html>");

        delay(3000);
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

void loadPreferences(float &latitude, float &longitude, String &cityName) {
    preferences.begin("weather", true);
    String latStr = preferences.getString("latitude", String(COORD_NOT_SET));
    String lonStr = preferences.getString("longitude", String(COORD_NOT_SET));
    cityName = preferences.getString("city", DEFAULT_CITY);
    String tempUnit = preferences.getString("tempunit", "F");
    useCelsius = (tempUnit == "C");
    nightModeSleep = preferences.getBool("nightmode", true);
    preferences.end();

    latitude = latStr.toFloat();
    longitude = lonStr.toFloat();

    if (latitude == COORD_NOT_SET || longitude == COORD_NOT_SET) {
        Serial.println("No coordinates found, geocoding city: " + cityName);
        if (geocodeCity(cityName, latitude, longitude)) {
            preferences.begin("weather", false);
            preferences.putString("latitude", String(latitude, 4));
            preferences.putString("longitude", String(longitude, 4));
            preferences.end();
            Serial.printf("Geocoded %s to %.4f, %.4f\n", cityName.c_str(), latitude, longitude);
        } else {
            Serial.println("Geocoding failed, using defaults");
            latitude = DEFAULT_LATITUDE;
            longitude = DEFAULT_LONGITUDE;
        }
    }

    Serial.printf("Using coordinates: %.4f, %.4f (%s)\n", latitude, longitude, cityName.c_str());
    Serial.printf("Temperature unit: %s\n", useCelsius ? "Celsius" : "Fahrenheit");
}
