# Future Feature Ideas

This document lists potential convenience features to be added to the DeviceReactor library.

---

## AnalogSensor Features


### `.onHigh(callback)` & `.onLow(callback)`
- **What it does:** Lets you define a threshold and triggers a callback once when the sensor value crosses it. For example, `.onHigh(50, turnOnFan)`.
- **Why it's useful:** It simplifies creating responsive systems, like turning on a light when a photoresistor detects darkness, without requiring the user to manage the state logic in their main loop.

---


