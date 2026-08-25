---
publishDate: 2026-08-23
title: Interrupt Zero — Edge-Fused In-Cabin Telematics Hub for Active V2X Hazard Mitigation and Touchless HMI
excerpt: A dual-core ESP32 telematics hub that fuses inertial and pneumatic sensor data to eliminate false-positive crash alerts, paired with an interrupt-driven touchless gesture HMI.
---

<p align="center">
  <img src="/assets/images/cover.png" width="800"><br/>
</p>




**tags:**
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
  <img src="/assets/images/fusion-decision-flow.png" width="800"><br/>
  <i>Dual-Factor Crash Fusion Decision Loop</i>
</p>

<p align="center">
  <img src="/assets/images/scenario-a.png" width="800"><br/>
  <i>Scenario A -Road Anomaly</i>
</p>

<p align="center">
  <img src="/assets/images/scenario-b.png" width="800"><br/>
  <i>Scenario B -True Accident</i>
</p>

### Videos

<video controls width="100%">
  <source src="/assets/videos/interrupt-zero-myosa.mp4" type="video/mp4">
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

### **3. Touchless, Interrupt-Driven HMI (Design)**

The gesture interface is architected around the APDS9960's interrupt pin, wired directly to an external interrupt-enabled GPIO instead of continuous I²C polling. The gesture engine is designed to stay fully asleep until a hand enters the 10 cm proximity envelope, waking an isolated ISR on Core 1. Gestures are decoded via a software state machine:

* **Swipe Left/Right** — switch between OLED telemetry panels (artificial horizon w/ angular roll, and cabin climate/ambient light index)
* **Swipe Up/Down** — acknowledge and clear minor threshold alerts without requiring visual attention

*This subsystem is implemented at the firmware/wiring level; full end-to-end gesture calibration is ongoing.*

### **4. Black-Box Fail-Safe & Mobile Sync**

Under normal operation, the SSD1306 draws a live graphical dashboard. The instant the fusion loop confirms a crash, the firmware halts normal rendering and enters an **Emergency Black-Box Routine**, freezing peak acceleration, force vectors, and pressure deltas on-screen — readable even if the vehicle loses connection to the main infotainment system.

In parallel, telemetry is packed into binary arrays for streaming over BLE to our companion **Flutter app**, which — once integration is complete — will compile the local telemetry log, auto-export the 5-second pre-impact data stream to `.xlsx` for forensic review, and trigger emergency location sharing. The BLE payload format and firmware-side packet assembly are implemented; app-side integration is in progress.

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
**Hardware**

* MYOSA Mini IoT Kit — dual-core ESP32 motherboard (240 MHz)
* MPU6050 — inertial measurement (I²C 0x68)
* BMP180 — barometric pressure/temperature (I²C 0x77)
* APDS9960 — gesture & ambient light (I²C 0x39, interrupt-driven)
* SSD1306 — 128x64 monochrome OLED (I²C 0x3C)

**Embedded / Firmware**

* FreeRTOS — dual-core task pinning (sensing on Core 0, HMI/BLE on Core 1)
* PlatformIO / Arduino-ESP32 — firmware build & flashing

**Communication**

* I²C (Fast Mode, 400 kHz) — sensor bus
* Bluetooth Low Energy (BLE) — telemetry sync (firmware-side complete)

**Mobile / Software**

* Flutter — cross-platform companion app for telemetry, forensic export, alerts (in development)

**Math / Signal Processing (implemented in firmware, not a library)**

* Complementary filter (pitch/roll estimation)
* Discrete derivative (dP/dt pressure tracking)
* Dual-threshold sensor fusion logic

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
Wire (built-in, no install needed)
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
## Novelty

Interrupt Zero introduces several distinct contributions beyond conventional single-sensor crash detection systems:

* **Dual-Factor Crash Validation (Inertial + Pneumatic)**
Unlike standalone accelerometer-based systems that flag potholes and curb strikes as accidents, Interrupt Zero cross-validates high-G impact events against transient cabin pressure changes — eliminating false positives without sacrificing detection speed.

* **Deterministic Dual-Core Task Isolation**
Safety-critical fusion math runs entirely isolated on Core 0, while HMI rendering and BLE communication run on Core 1 — guaranteeing the accident-detection loop is never delayed by display or radio I/O, a separation many hobbyist-grade IoT safety projects don't enforce.

* **Fully Touchless, Interrupt-Driven HMI**
Rather than continuous sensor polling (which wastes cycles and power), the gesture interface wakes only on proximity via a dedicated interrupt line — enabling zero-distraction driver interaction with near-zero idle overhead.

* **Local Black-Box Fail-Safe**
Even if wireless connectivity or the companion app fails, critical crash telemetry remains readable directly on the onboard OLED — ensuring diagnostic data survives independent of network conditions.

* **Low-Cost, Modular Hardware Realization**
Built entirely on plug-and-play MYOSA modules with standard JST connectors, the system proves that dual-factor crash validation — typically found in expensive proprietary automotive ECUs — can be replicated on accessible, modular, education-grade hardware.

---

## Contribution Notes (Optional)

This project is developed by Team **Interrupt Zero** (Omar Nour, Youssef Jaber) under the mentorship of Dr. Androw Sameh, Modern Academy for Engineering and Technology, Cairo, Egypt, for IEEE MYOSA Event 6.0 and IEEE SENSORS 2026. Issues and suggestions are welcome via the GitHub repository.
