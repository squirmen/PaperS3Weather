# Changelog - M5PaperS3 Weather Dashboard

## Version 1.13 - Battery Life Improvements (April 2025)

### Power Optimization
- **Improved deep sleep power consumption** - IMU now put to sleep before power-off, display properly shut down with `sleep()` and `waitDisplay()` calls
- **RTC alarm-based power-off** - Switched from `esp_deep_sleep_start()` to `M5.Power.powerOff()` with RTC alarm for significantly lower standby current
- **RTC-based time on boot** - System time is now set from the RTC if it holds a valid date, skipping the NTP call entirely on most wakes. NTP is only used on first boot (or after battery reset) and the result is saved to RTC for subsequent wakes
- **Faster boot cycle** - Eliminating NTP on timer wakes reduces WiFi-on time and overall wake duration

### Technical Details
- Deep sleep now calls `M5.Imu.sleep()`, `M5.Display.sleep()`, `M5.Display.waitDisplay()`, `M5.Rtc.setAlarmIRQ()`, and `M5.Power.powerOff()` for comprehensive shutdown
- `setupTime()` function checks RTC year > 2023 to determine if RTC has been previously set
- First boot still uses NTP and writes the result to RTC for future use

---

## Version 1.12 - Modular Refactor & Smart Wake (January 2025)

### 🏗️ Architecture Overhaul

#### Modular Code Organization
- **Split monolithic main.cpp into 6 modular files**:
  - `constants.h` - All configuration constants (100+ magic numbers extracted)
  - `utils.h/cpp` - Helper functions (temperature, time, weather conditions)
  - `weather_api.h/cpp` - API communication with retry logic
  - `config.h/cpp` - WiFi setup and web configuration portal
  - `display.h/cpp` - All display rendering functions
- **Reduced code duplication by ~70%**
- **Improved maintainability** with clear separation of concerns
- **Easier to extend** with new features

### ✨ New Features

#### Smart Wake Detection
- **30-second config window ONLY on manual reset**
  - Press reset button → 30 seconds to tap [CFG] for configuration
  - Automatic timer wakes → immediate refresh (no waiting)
  - Saves ~99% battery vs waiting every refresh cycle
- **Silent operation** - no on-screen countdown (avoids e-ink ghosting)
- **Touch detection** active during wait period for config access

#### User-Configurable Settings
- **Customizable refresh intervals** via web interface:
  - Day time: 5-120 minutes (default: 10 min)
  - Night time: 15-240 minutes (default: 60 min)
- **Customizable night mode hours**:
  - Start hour: 0-23 (default: 22:00 / 10pm)
  - End hour: 0-23 (default: 05:00 / 5am)
- **Modern, responsive web interface**:
  - Card-based design
  - Mobile-friendly
  - Input validation (client & server side)
  - Current settings displayed
  - Helpful tooltips and examples

### 🔧 Technical Improvements

#### Enhanced Reliability
- **HTTP timeout handling** (10 second timeout)
- **Retry logic** for weather API calls (3 attempts with 2s backoff)
- **WiFi retry logic** (3 attempts with 2s backoff)
- **Proper coordinate validation** using COORD_NOT_SET instead of 0.0
- **JSON parsing improvements** - switched to getString() for reliability

#### Better Power Management
- **WiFi disconnect before deep sleep** for reduced power consumption
- **Dynamic refresh intervals** loaded from user preferences
- **Intelligent wake reason detection** using ESP32 sleep API
- **No unnecessary display updates** during config wait

#### Bug Fixes
- Fixed coordinate validation bug (0.0, 0.0 is valid - Gulf of Guinea)
- Fixed JSON parsing "InvalidInput" errors with proper buffer handling
- Fixed convertTemp() function (API handles conversion, simplified code)
- Removed e-ink ghosting from partial display updates

### 📊 Memory Usage

- **RAM**: 15.2% (49,968 / 327,680 bytes)
- **Flash**: 19.8% (1,295,149 / 6,553,600 bytes)
- Plenty of headroom for future features

### 🔄 Migration from v1.11

**User Action Required:**
- Access config portal after update to set custom refresh intervals
- Default intervals match v1.11 behavior (10 min day, 60 min night)
- Night mode hours default to 22:00-05:00 (same as v1.11)

**Breaking Changes:**
- None - all preferences migrate automatically

### 📝 Configuration Access

**How to access configuration:**
1. Press the reset button on your M5Paper S3
2. Device wakes and displays weather
3. You have 30 seconds to tap the [CFG] button (bottom-right corner)
4. Configuration portal opens automatically
5. Connect to WiFi network: `M5Paper-Weather` (password: `configure`)
6. Navigate to: `http://192.168.4.1`

**Note:** Automatic wakes (every 10-60 min) skip the wait period and immediately refresh weather data, saving battery.

---

## Version 1.11 - Display Layout Improvements

### Features
- Reconfigured "Current" panel for better spacing
- Moved "Feels like" temperature for improved readability
- Enhanced visual hierarchy

---

## Version 1.1 - Initial M5Paper S3 Port

### Features
- Migration from M5Paper (EPD) to M5Paper S3
- Switched from M5EPD to M5Unified library
- Replaced OpenWeatherMap with Open-Meteo API
- City-based geocoding with automatic coordinate lookup
- Celsius/Fahrenheit support
- WiFi configuration portal
- Night mode power saving
- Deep sleep implementation
- Real-time weather display
- 8-hour forecast
- Multiple data graphs (temperature, precipitation, humidity, pressure)
- Sun/moon information
- Wind compass visualization
