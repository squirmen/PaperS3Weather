#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5Unified.h>
#include <M5GFX.h>

// Main display function
void displayWeather();

// Panel drawing functions
void drawCurrentConditions(int x, int y, int dx, int dy);
void drawSunInfo(int x, int y, int dx, int dy);
void drawWindInfo(int x, int y, int dx, int dy);
void drawM5PaperInfo(int x, int y, int dx, int dy);
void drawHourlyForecast(int x, int y, int dx, int dy, int index);

// Graph drawing functions
void drawGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float values[]);
void drawTempGraph(int x, int y, int dx, int dy, String title, int xMin, int xMax, float yMin, float yMax, float highValues[], float lowValues[]);

// Component drawing functions
void drawIcon(int x, int y, const uint8_t *icon, int dx = 64, int dy = 64, bool highContrast = false);
void drawRSSI(int x, int y, int rssi);
void drawBattery(int x, int y, int batteryPercent);
void drawArrow(int x, int y, int asize, float aangle, int pwidth, int plength);
void drawWindCompass(int x, int y, float angle, float windspeed, int radius);

#endif // DISPLAY_H
