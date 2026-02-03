#include "mqtt_manager.h"
#include "constants.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

extern bool useCelsius;
extern WeatherData currentWeather;

// Flag to indicate data reception
volatile bool mqttDataReceived = false;

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] (");
  Serial.print(length);
  Serial.println(" bytes)");

  // Allocate the JSON document
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Parse the data
  // The user provided schema: "outTemp_F", "outHumidity", "windSpeed_mph",
  // "windDir", "rainRate_inch_per_hour"

  // Temperature (always in F in JSON, convert if needed)
  float tempF = doc["outTemp_F"].as<float>();
  float appTempF = doc["appTemp_F"].as<float>();

  // Humidity
  float humidity = doc["outHumidity"].as<float>();

  // Wind
  float windSpeedMph = doc["windSpeed_mph"].as<float>();
  float windDir = doc["windDir"].as<float>();

  // Rain (using rate for "current" rain indication)
  float rainRateIn = doc["rainRate_inch_per_hour"].as<float>();
  float dayRainIn = doc["dayRain_in"].as<float>();

  // Update global weather data
  // Note: Constants.h/Utils.h implies we need to store in standard units or
  // convert based on display preference. However, OpenMeteo code stores raw
  // values and conversion happens? Wait, OpenMeteo code in weather_api.cpp
  // requests units dynamically: url += useCelsius ?
  // "&temperature_unit=celsius..." : "&temperature_unit=fahrenheit...";

  // So `currentWeather` structs are expected to hold values in the *current
  // display unit*.

  if (useCelsius) {
    currentWeather.temperature = (tempF - 32.0) * 5.0 / 9.0;
    currentWeather.apparentTemperature = (appTempF - 32.0) * 5.0 / 9.0;
    currentWeather.windSpeed = windSpeedMph * 1.60934; // mph to km/h
    currentWeather.precipitation = rainRateIn * 25.4;  // inch to mm
  } else {
    currentWeather.temperature = tempF;
    currentWeather.apparentTemperature = appTempF;
    currentWeather.windSpeed = windSpeedMph;
    currentWeather.precipitation = rainRateIn;
  }

  currentWeather.humidity = humidity;
  currentWeather.windDir = windDir;
  currentWeather.source = "MQTT";

  // Map rain to precipitation if it is raining right now
  // If rainRate is 0, we can check if it rained today?
  // OpenMeteo "precipitation" is usually hourly or current intensity.
  // We will stick to rate for now, but maybe show dayRain if we had a dedicated
  // field.

  Serial.println("\n=== MQTT Weather Data Updated ===");
  Serial.printf("Temp: %.1f, Humidity: %.0f%%, Wind: %.1f\n",
                currentWeather.temperature, currentWeather.humidity,
                currentWeather.windSpeed);

  mqttDataReceived = true;
}

bool fetchMQTTWeatherData(String server, int port) {
  if (server.length() == 0)
    return false;

  WiFiClient espClient;
  PubSubClient client(espClient);

  client.setServer(server.c_str(), port);
  client.setCallback(mqttCallback);

  Serial.printf("Connecting to MQTT: %s:%d...\n", server.c_str(), port);

  // Random Client ID
  String clientId = "PaperS3-Weather-";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("MQTT Connected");
    client.subscribe("weather/loop");

    // Wait for message
    // weather/loop often publishes every 2-5 seconds
    unsigned long start = millis();
    mqttDataReceived = false;

    while (!mqttDataReceived && millis() - start < 20000) { // 20 second timeout
      client.loop();
      delay(10);
    }

    client.disconnect();

    if (mqttDataReceived) {
      Serial.println("MQTT Fetch Successful");
      return true;
    } else {
      Serial.println("MQTT Timeout: No message received in 10s");
      return false;
    }
  } else {
    Serial.print("MQTT Connection Failed, state=");
    Serial.println(client.state());
    return false;
  }
}
