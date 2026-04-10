# PaperS3Weather v1.12 - M5Burner Installation Guide

## Package Contents

The M5Burner package includes:

```
PaperS3Weather_v1.12/
├── bootloader.bin (15 KB)    - ESP32-S3 bootloader
├── partitions.bin (3 KB)     - Partition table for 16MB flash
├── firmware.bin (1.2 MB)     - Main application firmware
├── manifest.json (2 KB)      - M5Burner metadata
├── screenshot.jpg (1.8 MB)   - Device display preview
└── README.txt (4 KB)         - Quick reference guide
```

**Total Package Size**: 3.1 MB (compressed: 2.5 MB)

---

## Installation Methods

### Method 1: M5Burner (Recommended for Beginners)

#### Step 1: Download M5Burner
1. Visit: https://m5stack.com/pages/download
2. Download M5Burner for your operating system (Windows/Mac/Linux)
3. Install and launch M5Burner

#### Step 2: Prepare Device
1. Connect M5Paper S3 to your computer via USB-C cable
2. Press the power button if device doesn't turn on
3. Ensure device is recognized (check device manager/system profiler)

#### Step 3: Flash Firmware
1. In M5Burner, select **"ESP32-S3"** device type
2. Click **"Add Custom"** or **"Import"**
3. Select the **PaperS3Weather_v1.12.zip** file or folder
4. Click on **PaperS3Weather v1.12** in the list
5. Click **"Burn"** button
6. Wait for flashing to complete (~1-2 minutes)
7. Look for "Success!" message

#### Step 4: Verify Installation
1. Device will reboot automatically
2. You should see "PaperS3Weather Version 1.12" on screen
3. Configuration screen will appear (first boot)

---

### Method 2: Command Line (Advanced Users)

#### Using esptool.py

```bash
# Install esptool if not already installed
pip install esptool

# Navigate to package directory
cd PaperS3Weather_v1.12

# Flash all binaries at correct offsets
esptool.py --chip esp32s3 \
  --port /dev/cu.usbmodem* \
  --baud 1500000 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**Note**: Replace `/dev/cu.usbmodem*` with your actual port:
- **macOS**: `/dev/cu.usbmodem*` or `/dev/tty.usbmodem*`
- **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`
- **Windows**: `COM3`, `COM4`, etc.

#### Using PlatformIO

```bash
# Clone repository
git clone https://github.com/squirmen/PaperS3Weather.git
cd PaperS3Weather

# Checkout v1.12 tag
git checkout v1.12

# Build and upload
pio run -t upload
```

---

## First Time Configuration

After flashing firmware, follow these steps:

### 1. Connect to Configuration Portal

The device will display:
```
Config Mode
Connect to: M5Paper-Weather
Password: configure
Open: 192.168.4.1
```

**On your phone or computer:**
1. Open WiFi settings
2. Connect to network: **M5Paper-Weather**
3. Password: **configure**
4. Open browser and navigate to: **http://192.168.4.1**

### 2. Enter Settings

Fill in the configuration form:

#### WiFi Connection
- **Network Name (SSID)**: Your home WiFi network name
- **Password**: Your WiFi password
- **Note**: Must be 2.4GHz network (ESP32-S3 doesn't support 5GHz)

#### Location
- **City Name**: Your city (e.g., "Auckland", "New York", "Tokyo")
  - Just enter city name, coordinates will be looked up automatically
  - Leave Latitude/Longitude fields blank unless you need manual coordinates

#### Display Preferences
- **Temperature Unit**: Choose Celsius or Fahrenheit

#### Update Schedule
- **Day Time Refresh**: 5-120 minutes (default: 10 minutes)
  - How often to update weather during the day
  - Lower = more updates, more battery use
- **Night Time Refresh**: 15-240 minutes (default: 60 minutes)
  - How often to update at night
  - Longer interval saves battery while you sleep

#### Night Mode
- **Enable Night Mode**: Check to enable (recommended)
- **Start Hour**: 0-23 (default: 22 = 10 PM)
- **End Hour**: 0-23 (default: 5 = 5 AM)

### 3. Save and Restart

1. Click **"Save & Restart"**
2. Device will restart and connect to your WiFi
3. City coordinates will be geocoded automatically
4. Weather data will be fetched and displayed

---

## Accessing Configuration Later

After initial setup, to change configuration:

1. **Press the RESET button** on your M5Paper S3 (side button)
2. Device wakes and displays current weather
3. **Within 30 seconds**, **TAP** the **[CFG]** button in bottom-right corner of screen
4. Configuration portal opens (follow steps above)

**Important**:
- Config window only appears on **manual reset**
- Automatic weather updates (every 10-60 min) skip the wait period
- This saves battery by not waiting unnecessarily

---

## Verification and Testing

### Check Serial Monitor

Connect via serial to see detailed logs:

```bash
# Using PlatformIO
pio device monitor --baud 115200

# Using screen (macOS/Linux)
screen /dev/cu.usbmodem* 115200

# Using Arduino IDE
Tools → Serial Monitor (115200 baud)
```

### What You Should See

```
=================================
PaperS3Weather Version 1.12
Based on Bastelschlumpf design
=================================

WiFi connected!
192.168.4.123
Time configured via NTP
Geocoding city: Auckland
Found: Auckland, New Zealand at -36.8485, 174.7633
Using coordinates: -36.8485, 174.7633 (Auckland)

=== Current Conditions from API ===
Temperature: 18.5
Feels Like: 17.2
Humidity: 72%
Wind: 12.3 @ 245°
Weather Code: 2

Weather fetch successful!
=================================
Night mode: ENABLED
Night hours: 22:00 - 5:00
Current time is: DAY
Refresh interval: 10 minutes
=================================
Entering deep sleep for 600000 ms (10 minutes)
```

---

## Troubleshooting

### Flash Failed / Upload Error

**Problem**: M5Burner shows error during flashing

**Solutions**:
1. Try different USB cable (some are charge-only)
2. Try different USB port on computer
3. Press and hold power button during flash attempt
4. Reduce baud rate to 115200 in M5Burner settings
5. Update USB drivers for your OS

### WiFi Connection Failed

**Problem**: Device can't connect to your WiFi

**Solutions**:
1. Verify WiFi is 2.4GHz (not 5GHz)
2. Check password is correct (case-sensitive)
3. Move device closer to router
4. Check router allows new device connections
5. Try mobile hotspot as test

### Configuration Portal Not Accessible

**Problem**: Can't open http://192.168.4.1

**Solutions**:
1. Ensure you're connected to "M5Paper-Weather" WiFi
2. Disable mobile data on phone (forces use of WiFi)
3. Try `http://` explicitly (not `https://`)
4. Clear browser cache or try different browser
5. Wait 10 seconds after connecting to WiFi

### City Not Found

**Problem**: "Geocoding failed" in serial monitor

**Solutions**:
1. Try city name without state/country
2. Try adding country (e.g., "Portland USA")
3. Use major city nearby
4. Enter manual coordinates instead:
   - Google "[your city] coordinates"
   - Use latlong.net or Google Maps
   - Enter in Latitude/Longitude fields

### Weather Not Updating

**Problem**: Display shows old data or "Failed to fetch weather"

**Solutions**:
1. Check internet connection is working
2. View serial monitor for specific errors
3. Open-Meteo API may be temporarily down (wait 5-10 minutes)
4. Verify coordinates are valid (not 0.0, 0.0)
5. Try manual API test in browser:
   ```
   https://api.open-meteo.com/v1/forecast?latitude=-36.8485&longitude=174.7633&current=temperature_2m
   ```

### Wrong Timezone

**Problem**: Times don't match your local time

**Note**: Open-Meteo uses `timezone=auto` which should detect timezone from coordinates automatically. If times are still wrong, your city might be geocoded to wrong location.

**Solutions**:
1. Use manual coordinates for your exact location
2. Check serial monitor for geocoded city - verify it's correct
3. If you need to manually set timezone, edit `src/main.cpp` (requires rebuilding)

---

## Memory and Performance

### Resource Usage
- **RAM**: 15.2% (49,968 / 327,680 bytes)
- **Flash**: 19.8% (1,295,149 / 6,553,600 bytes)
- Plenty of headroom for future features

### Battery Life Estimates
Based on 1150mAh battery:
- **Default settings** (10 min day, 60 min night): 7-10 days
- **Power saver** (30 min day, 120 min night): 14-21 days
- **Aggressive** (5 min constant): 3-5 days

### Power Consumption
- Deep sleep: ~0.17mA
- Active (WiFi + display): ~150-200mA for 10-15 seconds per update

---

## Additional Resources

- **GitHub Repository**: https://github.com/squirmen/PaperS3Weather
- **Issue Tracker**: https://github.com/squirmen/PaperS3Weather/issues
- **M5Stack Official**: https://docs.m5stack.com/en/core/M5Paper
- **Open-Meteo API**: https://open-meteo.com/
- **PlatformIO Docs**: https://docs.platformio.org/

---

## Credits

- **Original Design**: [Bastelschlumpf](https://github.com/Bastelschlumpf) - M5PaperWeather
- **Weather Data**: [Open-Meteo](https://open-meteo.com/) - Free weather API
- **Hardware**: [M5Stack](https://m5stack.com/) - M5Paper S3 and M5Unified library

---

## License

MIT License - See LICENSE file in repository

---

**Enjoy your PaperS3Weather station!**
