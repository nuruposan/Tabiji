# Tabiji
ESP32-S3 firmware for a portable GNSS logger, supporting GPX file generation over Bluetooth/USB.

## Project Overview

This project aims to build a DIY GPS logger that integrates a XIAO board and a GNSS module into a 3D-printed enclosure.

### Current Status

- Currently undergoing functional testing on a breadboard.
- Developing firmware to output GPX files externally via Bluetooth or Serial communication.

## Components

* **Microcontroller Board**: [Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
* **GNSS Module**: [L76K GNSS Module for SeeedStudio XIAO](https://wiki.seeedstudio.com/get_start_l76k_gnss/)
* **Sound feedback**: piezo buzzer (BUZZ)
* **Control Input**: 3x push button switches (BTN1-3)
* **Power Source**: 500 mAh LiPo Battery

## Wiring on Breadboard

```text
       D0 --------- BUZZ ----+
       D1 --------- BTN1 ----+
       D2 --------- BTN2 ----+---- GND
       D3 --------- BTN3 ----+
XIAO   
ESP32  D5 --------- WUP 
S3     D6(TX) ----- RX    L76K
       D7(RX) ----- TX    GNSS  U.FL  ----- Ext. Antenna
       3V3 -------- 3V3   mod
       GND -------- GND
       
       BAT+ ------- LiPo
       BAT- ------- Battery
```
