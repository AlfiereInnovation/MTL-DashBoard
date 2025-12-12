# MTL-DashBoard
dashboard with Date-Time, Calendar, STM Status and Weather Status

## License
This project is released under the MIT License.
See the LICENSE file for details.

# E-Paper Status Dashboard (STM + Weather + Calendar)

A minimalist **e-paper desk dashboard** designed for daily, low-power information display.

This project runs on a **CrowPanel 5.79" monochrome e-paper display** and shows, in portrait mode:

- **Local time (Montreal)** with full monthly calendar  
- **STM Montréal metro service status** (Lines Green, Orange, Yellow, Blue)  
- **Current weather** from OpenWeatherMap, with custom monochrome icons  

The display updates periodically and consumes power only during refreshes, making it ideal as an always-on desk or shelf device.

---

## Hardware Overview

### Display & Controller
- **CrowPanel 5.79" e-Paper (272 × 792 px, monochrome)**
- ESP32-based controller (integrated in the CrowPanel)
- SPI interface for the e-paper panel

### Enclosure
The enclosure is a **3D-printed vertical stand**, designed to:
- Hold the e-paper display in portrait orientation
- Slightly tilt the screen for desk readability
- Expose the USB cable at the rear for power and programming

The enclosure shown in the photo is printed in **white PLA**, but the design is material-agnostic.

The repository includes a dedicated folder with:
- **Fusion 360 source model**
- **STEP export** (editable in most CAD tools)
- **STL file** ready for slicing and printing

---

## Software Overview

### IDE
This project is developed using the **Arduino IDE**.

- Tested with Arduino IDE **2.x**
- Board support: **ESP32 by Espressif Systems**

---

## Required Arduino Libraries

All libraries are available through the **Arduino Library Manager** unless stated otherwise.

### Core
- **ESP32 Arduino Core**  
  Boards Manager → *“esp32 by Espressif Systems”*

### Display & Graphics
- **GxEPD2** (by Jean-Marc Zingg)  
  Used to drive the 5.79" e-paper panel  
- **Adafruit GFX Library**  
  Base graphics primitives and font handling

### Fonts
- Built-in Adafruit GFX fonts:
  - FreeSans  
  - FreeSansBold  
  - FreeMonoBold  

(No external font files required.)

### Networking & Data
- **ArduinoJson**  
  Used to parse OpenWeatherMap API responses  
- **WiFi / WiFiClientSecure / HTTPClient**  
  Standard ESP32 networking libraries (included with ESP32 core)

---

## External Services

### OpenWeatherMap
Used to fetch current weather conditions.

You need:
- A free OpenWeatherMap account
- An **API key**

Website:  
https://openweathermap.org/api

---

### STM Montréal Metro Status
Metro status is fetched from the official STM website:

https://www.stm.info/en/info/service-updates/metro
The project requests **uncompressed HTML** (no gzip) and extracts the service status text directly from the page.

⚠️ Note:  
If STM changes their website structure or forces gzip compression, parsing may stop working. This is documented in the code.

---

## Configuration (Important)

in the code, lines 41 to 43, change these by adding your credentials:

#define WIFI_SSID     "your_wifi_name"
#define WIFI_PASSWORD "your_wifi_password"
#define OWM_API_KEY   "your_openweathermap_api_key"


How to Build & Flash
1. Install Arduino IDE
2. Install ESP32 board support
3.Install required libraries via Library Manager
4.Open the .ino file
5.Configure your credentials as specified above
6.Select the correct ESP32 board and port in the Arduino IDE menu
7.Upload the sketch

After flashing:
1.The device connects to Wi-Fi
2.Syncs time via NTP (Montreal timezone)
3.Fetches weather and STM status
4.Draws the full UI on the e-paper display


Display Update Logic
- Screen refresh: every 1 minute
- Data refresh (weather + STM): every 10 minutes

E-paper only consumes power during refresh, making the device efficient even when powered continuously.

3D Printing Notes
- Material: PLA, PETG, or similar
- No supports required
- Designed for standard 0.4 mm nozzle
- Orientation: print with the base flat on the build plate

The STEP file allows easy modification:

- Adjust tilt angle
- Add wall mounting
- Adapt for other e-paper sizes


## License
This project is released under the MIT License.
You are free to use, modify, and redistribute the code and designs.
Attribution is appreciated.
No warranty is provided.
See the LICENSE file for full text.

## Disclaimer
This project is provided as a demo and reference implementation.
It relies on third-party services and public web pages that may change over time.
