---
publishDate: 2026-08-23
title: Interrupt Zero — Edge-Fused In-Cabin Telematics Hub for Active V2X Hazard Mitigation and Touchless HMI
excerpt: A dual-core ESP32 telematics hub that fuses inertial and pneumatic sensor data to eliminate false-positive crash alerts, paired with an interrupt-driven touchless gesture HMI.
---

<p align="center">
  <img src="/assets/images/cover.png" width="800"><br/>
</p>

---
tags:
  - MYOSA
  - ESP32
  - Automotive
  - SensorFusion
  - IEEE
---

> Fusing motion and pressure at the edge to tell a real crash from a pothole — without ever touching a button.

---

## Acknowledgements

Special thanks to our faculty mentor, **Dr. Androw Sameh**, Department of Electronics and Communications Engineering, Modern Academy for Engineering and Technology, Cairo, Egypt, for his continuous guidance throughout the design and validation of this system. This project is being developed for **IEEE MYOSA Event 6.0**, with a planned live demo at **IEEE SENSORS 2026** in Rotterdam.

---

## Overview

As Intelligent Transport Systems (ITS) advance, vehicular safety increasingly depends on fast, localized edge computing capable of microsecond-level event validation. **Interrupt Zero** is an in-cabin telematics and active safety hub built on the MYOSA platform that tackles two everyday automotive problems at once: airbags/alerts that trigger on potholes and curb strikes instead of real crashes, and dashboard interfaces that pull a driver's eyes off the road.

The system runs real-time multi-sensor fusion on a single deterministic I²C bus, cross-checking high-G physical impacts against transient cabin pressure changes before ever declaring an emergency. Alongside this, a hardware-interrupt-driven gesture engine lets the driver control the heads-up display with a wave of the hand — no glancing down, no buttons.

**Key features:**
* Dual-factor crash validation (inertial + pneumatic) to eliminate false-positive alerts
* Deterministic dual-core task split on the ESP32 (sensing vs. HMI/BLE)
* Touchless, interrupt-driven gesture control via APDS9960 (zero polling overhead)
* Local OLED "black-box" fail-safe that freezes crash telemetry on-screen
* Asynchronous BLE sync to the MYOSA Android app with automatic `.xlsx` forensic export

---

## Demo / Examples

### Images

<p align="center">
  <img src="/assets/images/interrupt-zero/system-architecture.jpg" width="800"><br/>
  <i>System architecture — sensor array on shared I²C bus, dual-core ESP32 task split</i>
</p>

<p align="center">
  <img src="/assets/images/fusion-decision-flow.png" width="800"><br/>
  <i>Dual-Factor Crash Fusion Decision Loop</i>
</p>

### Videos

<video controls width="100%">
  <source src="/interrupt-zero-demo.mp4" type="video/mp4">
</video>

---

## Features (Detailed)

### **1. Deterministic Sensor Topology**

The system runs on the dual-core ESP32 variant of the MYOSA motherboard at 240 MHz, with all four sensor modules connected via the Lego-style JST plug-and-play architecture. To guarantee timing determinism, every module shares a single I²C bus in Fast Mode (400 kHz), with unique 7-bit addresses:

* `0x68` — MPU6050 (Inertial Measurement Unit)
* `0x77` — BMP180 (Barometric Pressure & Temperature)
* `0x39` — APDS9960 (Digital Light & Gesture Sensor)
* `0x3C` — SSD1306 (Monochrome OLED Display)

To meet real-time safety guarantees, firmware execution is split symmetrically across the two ESP32 cores: **Core 0** is dedicated entirely to high-frequency sensor polling, state estimation, and fusion math, while **Core 1** handles non-blocking OLED rendering and the asynchronous BLE transmission stack — keeping display and radio I/O from ever lagging the safety loop.

### **2. Multi-Sensor Fusion for Crash Validation**

Standalone accelerometers are prone to false-positive accident flags on potholes, curb strikes, and routine suspension impacts. Interrupt Zero resolves this with a dual-factor validation matrix combining inertial and pneumatic evidence:

- **Inertial state estimation (MPU6050):** configured at ±8g accelerometer range and ±2000°/s gyroscope range. Pitch and roll are extracted using a complementary filter:

  `θ(t) = α · (θ(t-1) + ω · Δt) + (1 - α) · θacc`, with α = 0.98 and θacc = atan2(Ay, Az)

  Total instantaneous G-force is computed at 100 Hz: `‖A‖ = √(Ax² + Ay² + Az²)`

- **Pneumatic event verification (BMP180):** running in Ultra High Resolution mode (oversampling setting 3, 0.03 hPa resolution), tracking the discrete time derivative of cabin pressure (dP/dt) to detect the compression wave that accompanies real structural impact.

- **Fusion decision loop:** a critical accident state is only declared when **both** conditions hold simultaneously:

  `‖A‖ > 4.5g` **AND** `dP/dt > ΔP_threshold`

  A high-G spike without a matching pressure spike (e.g., a heavy pothole) is filtered out as a routine road anomaly.

### **3. Touchless, Interrupt-Driven HMI**

Instead of continuously polling the APDS9960 gesture sensor over I²C, its interrupt pin is wired directly to an external interrupt-enabled GPIO. The gesture engine stays fully asleep until a hand enters the 10 cm proximity envelope, waking an isolated ISR on Core 1. Gestures are decoded via a software state machine:

* **Swipe Left/Right** — switch between OLED telemetry panels (artificial horizon w/ angular roll, and cabin climate/ambient light index)
* **Swipe Up/Down** — acknowledge and clear minor threshold alerts without requiring visual attention

### **4. Black-Box Fail-Safe & Mobile Sync**

Under normal operation, the SSD1306 draws a live graphical dashboard. The instant the fusion loop confirms a crash, the firmware halts normal rendering and enters an **Emergency Black-Box Routine**, freezing peak acceleration, force vectors, and pressure deltas on-screen — readable even if the vehicle loses connection to the main infotainment system.

In parallel, telemetry is packed into binary arrays and streamed over BLE to the MYOSA Android app. When the app detects the Critical Accident Flag, it compiles the local telemetry log, auto-exports the 5-second pre-impact data stream to `.xlsx` for forensic review, and triggers emergency location sharing.

---

## Usage Instructions

Flash the firmware to the MYOSA ESP32 motherboard, connect all four sensor modules via their JST connectors on the shared I²C bus, and power on. The OLED will boot into the live dashboard view automatically.

```plaintext
# Example: flashing via PlatformIO
pio run -t upload --upload-port /dev/ttyUSB0
```

Pair the board with the MYOSA Android app over BLE to receive live telemetry and automatic crash-event exports.

```cpp
// Example: dual-threshold fusion check (Core 0 task)
if (accelMagnitude > 4.5 && pressureDerivative > dP_THRESHOLD) {
    triggerEmergencyState();
}
```

---

## Tech Stack

* **MYOSA Mini IoT Kit** — dual-core ESP32 motherboard (240 MHz)
* **MPU6050** — inertial measurement (I²C 0x68)
* **BMP180** — barometric pressure/temperature (I²C 0x77)
* **APDS9960** — gesture & ambient light (I²C 0x39, interrupt-driven)
* **SSD1306** — 128x64 monochrome OLED (I²C 0x3C)
* **FreeRTOS** — dual-core task pinning (sensing on Core 0, HMI/BLE on Core 1)
* **Bluetooth Low Energy (BLE)** — async telemetry sync to the MYOSA Android app
* **PlatformIO / Arduino-ESP32** — firmware build & flashing

---

## Requirements / Installation

```bash
pip install platformio
```

Firmware libraries (via PlatformIO `lib_deps` or Arduino Library Manager):

```plaintext
Adafruit SSD1306
Adafruit GFX Library
SparkFun APDS9960 RGB and Gesture Sensor
Adafruit BMP085 Library
Wire
```

---

## File Structure (Optional)

```
/interrupt-zero
  ├─ src/
  │   ├─ main.cpp
  │   ├─ fusion.cpp
  │   ├─ hmi.cpp
  │   └─ ble_sync.cpp
  ├─ include/
  ├─ platformio.ini
  └─ README.md
```

---

## License (Optional)

MIT License — see `LICENSE` file in the repository.

---

## Contribution Notes (Optional)

This project is developed by Team **Interrupt Zero** (Omar Nour, Youssef Jaber) under the mentorship of Dr. Androw Sameh, Modern Academy for Engineering and Technology, Cairo, Egypt, for IEEE MYOSA Event 6.0 and IEEE SENSORS 2026. Issues and suggestions are welcome via the GitHub repository.
