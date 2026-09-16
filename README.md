# 🗺️Tabiji - a GPS logger 
ESP32-S3 firmware for a portable GPS/GNSS logger, supporting GPX file generation over Bluetooth/USB.

## 📍Project Overview

This project aims to build a DIY GPS logger system consisting of two units: a logger unit that records GPS/GNSS data and a log downloader unit that downloads the logs and saves them to an SD card.

The logger unit integrates a XIAO board and a GNSS module into a 3D-printed enclosure. The log downloader unit uses an AtomS3-Lite and an SD/TF card module to transfer the recorded logs and save it in GPX format to an SD card.


### 🚦Status of this project

- Currently undergoing functional testing on a breadboard.
- Developing firmware to output GPX files externally via Bluetooth or Serial communication.

## 🛠️ Development Environment & Libraries

This project is built using:
* **IDE**:
  * [VS Code](https://visualstudio.com) with [PlatformIO IDE](https://platformio.org) extension
* **Libraries used**:
  * [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) (for parsing NMEA data from the GNSS module)
  * [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (for efficient Bluetooth Low Energy communication)

## 📦GPS Logger Unit

### Components

* **Microcontroller Board**: [Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
* **GNSS Module**: [L76K GNSS Module for SeeedStudio XIAO](https://wiki.seeedstudio.com/get_start_l76k_gnss/)
* **Power source**: 1x LiPo Battery
* **Control input**: 3x push button switches (BTN1-3)
* **Sound feedback**: 1x piezo buzzer (SPK1)

### Wiring (currently on breadboard)

```text
|       |D0 ------------ [ SPK1 ] ----+---- GND
|       |D1 ------------ [ BTN1 ] ----+
|       |D2 ------------ [ BTN2 ] ----+
|       |D3 ------------ [ BTN3 ] ----+
| XIAO  |
| ESP32 |D5 --------- WUP|      |
| S3    |D6(TX) ------ RX| L76K |
| board |D7(RX) ------ TX| GNSS |U.FL ----- Ext. Antenna
|       |3V3 -------- 3V3| mod  |
|       |GND -------- GND|      |
|       |
|       |BAT+ --------- +| LiPo |
|       |BAT- --------- -| BAT  |
```

## 📥Log Downloader Unit

The AtomS3-Lite and the TFCard module are stacked together to form the Log Downloader Unit. A piezo buzzer is additionally connected to an unused pin on the TFCard module for sound feedback.

### Components

* **Microcontroller board**: [AtomS3-Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite)
* **SD/TF card module**: [Atomic TFCard Base](https://docs.m5stack.com/en/atom/Atomic%20TF-Card%20Reader)
* **Sound feedback**: 1x piezo buzzer (SPK2)

### Wiring

```text
|        |3V3 ------  3V3|        |
|        |G6 ------- MOSI|        |
|        |G7 -------- CLK|        |
| ATOMS3 |G8 ------- MISO| TFCARD |
| Lite   |               | Base   |
| board  |5V  -------- 5V| module |
|        |GND ------- GND|        |GND ----------+
|        |               |        |              |
|        |G38 -----------|------------------- [ SPK2 ]
```



## 🚀Getting Started

### 1. Setup Project

Clone this repository to your preferred local directory and open it with VS Code:
```bash
git clone https://github.com
```
*PlatformIO will automatically detect the configuration and download the required libraries upon opening.*

### 2. Flashing the Firmware

1. Connect your wired **XIAO ESP32-S3** board to your PC via a USB-C cable.
2. Click the **PlatformIO: Upload** button (the arrow icon in the bottom status bar) or run ```pio run --target upload``` in the VS Code terminal.
