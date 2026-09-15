# Tabiji
ESP32-S3 firmware for a portable GPS/GNSS logger, supporting GPX file generation over Bluetooth/USB.

## Project Overview

This project aims to build a DIY GPS logger that integrates a XIAO board and a GNSS module into a 3D-printed enclosure.

### Current Status

- Currently undergoing functional testing on a breadboard.
- Developing firmware to output GPX files externally via Bluetooth or Serial communication.

## Components

* **GPS Logger Unit**
  * **Microcontroller Board**: [Seeed Studio XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
  * **GNSS Module**: [L76K GNSS Module for SeeedStudio XIAO](https://wiki.seeedstudio.com/get_start_l76k_gnss/)
  * **Power source**: 1x LiPo Battery
  * **Control input**: 3x push button switches (BTN1-3)
  * **Sound feedback**: 1x piezo buzzer (SPK1)

* **Log Downloader unit**
  * **Microcontroller board**: [AtomS3-Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite)
  * **SD/TF card module**: [Atomic TFCard Base](https://docs.m5stack.com/en/atom/Atomic%20TF-Card%20Reader)
  * **Sound feedback**: 1x piezo buzzer (SPK2)
  

## Wiring on Breadboard

### GPS Logger Unit

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

### Log Downloader Unit

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