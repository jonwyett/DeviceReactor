Of course. This is the culmination of our design process. Here is the comprehensive, "soup-to-nuts" guide to the `DeviceReactor` `AnalogSensor` class, designed to be the definitive resource for any user.

***

# The Ultimate Guide to the DeviceReactor AnalogSensor

## Introduction: Taming the Analog World

The `AnalogSensor` class is the heart of DeviceReactor's approach to real-world hardware interaction. It is a powerful, flexible, and unified interface designed to solve one of the most fundamental challenges in embedded systems: taming the noisy, unpredictable, and continuous nature of analog signals.

Whether you are reading a simple potentiometer, building a light-sensitive night light, or designing a complex, multi-stage thermostat, the `AnalogSensor` class provides a toolbox of sophisticated features to transform a messy raw signal into a clean, stable, and predictable output. This guide will walk you through the entire process, from understanding the nature of analog noise to mastering the advanced features of the library.

---

## 1. The Challenge of Real-World Analog Sensors

In a perfect world, a stationary sensor would produce a perfectly stable value. In reality, the data received from an `analogRead()` call is a constant stream of fluctuating values, even when the physical sensor is not moving.

`Raw ADC readings from a stationary potentiometer:`
`512, 513, 512, 514, 511, 513, 512, 515, 512, 511...`

This instability comes from a variety of real-world factors that this library is designed to manage.

### Common Sources of Instability

*   **Electrical Jitter:** High-frequency noise from other components (motors, WiFi), power supply fluctuations, and the inherent nature of Analog-to-Digital Converters (ADCs) cause the value to flicker by a few points constantly.
*   **Sensor Drift:** Environmental changes, such as temperature affecting a component's resistance, can cause a sensor's baseline value to slowly drift over time, even with no external input.
*   **Contact Noise:** Mechanical sensors like potentiometers can produce "crackle" or noisy spikes as the internal wiper moves across the resistive track, especially on older or cheaper components.
*   **Poor Connections:** The humble breadboard is a common source of noise, where slight variations in contact resistance can cause significant signal fluctuation.

### The Need for Calibration

Not all sensors use the full 0-1023 range of the Arduino's ADC.
*   A **photoresistor (LDR)** in a typical indoor environment might only produce values between `50` (very bright) and `950` (very dark). Using the full `0-1023` range would result in a loss of sensitivity and an inaccurate mapping.
*   A **specialized sensor** might operate on a different voltage, requiring the user to define its specific operational range for accurate readings.

The `AnalogSensor` class provides tools to calibrate the input to the sensor's true operational range, ensuring maximum precision.

---

## 2. The Signal Processing Pipeline: A Mental Model

To understand how the `AnalogSensor` class works, it's essential to visualize its internal signal processing pipeline. Every raw reading goes through a series of stages, each designed to refine the signal. The order of these operations is critical for robust and predictable behavior.

**The Pipeline:**
`Raw ADC Reading` -> **[Smooth]** -> **[Calibrate & Clamp Input]** -> **[Map to Output]** -> **[Clamp Output]** -> **[Invert]** -> `hiResValue` -> **[Stabilize (Q+H or T)]** -> `stableValue` -> **[Zone Check]** -> `onZoneChange Event`

This document will break down each of these stages in detail.

---

## 3. Core Signal Conditioning

These are the fundamental tools for turning a raw, noisy signal into a clean, high-resolution value (`hiResValue`).

### `.smoothing(samples)` - The First Line of Defense

*   **What it does:** A simple moving average filter. It collects a number of raw ADC readings (`samples`) and averages them before passing the result to the next stage.
*   **Why you need it:** This is the primary tool for combating high-frequency electrical jitter. A small amount of smoothing has a dramatic effect on the stability of the signal.
*   **The Trade-Off:** **Stability vs. Latency.** A high sample count (e.g., `100`) creates a very smooth signal but introduces a noticeable delay (100ms on a 1ms tick) between the physical sensor moving and the software value changing. A low sample count (e.g., `10`) is highly responsive with virtually no perceptible lag.
*   **Tuning:**
    *   For user controls like **potentiometers**, use a low value (`5` to `20`) to prioritize responsiveness.
    *   For environmental sensors like **photoresistors or thermistors**, use a higher value (`20` to `100`) to get a more stable reading of ambient conditions.
    *   **The default is `1` (no smoothing).**

### `.inputRange(min, max)` and `.outputRange(min, max)` - Calibration and Mapping

*   **What they do:** These methods define the mapping from the sensor's calibrated hardware range to a desired user-facing output range.
*   **Why you need them:** This is how you translate a raw ADC value like `873` into a meaningful number like `85` (percent) or `217` (for an LED). The pipeline first **clamps** the smoothed value to the `inputRange` and then **maps** it to the `outputRange`.
*   **Example:**
    ```cpp
    // A photoresistor only outputs between 50 and 950.
    // We want to map this to an LED's brightness (0-255).
    mySensor.inputRange(50, 950)
            .outputRange(0, 255);
    ```

### `.invert()` - Flipping the Logic

*   **What it does:** Inverts the final mapped value within the defined `outputRange`. A value of `20` in a `0-100` range becomes `80`.
*   **Why you need it:** Many sensors work "backwards." A photoresistor's resistance goes *down* as light increases, resulting in a lower `analogRead` value. If you're building a night light, you want a *high* output value when it's dark (low reading). Inversion handles this logic cleanly.

---

## 4. Achieving Stability: The Two Modes

After the signal has been smoothed, calibrated, and mapped, it is fed into one of two mutually exclusive **stability modes**. These modes determine how the sensor's final value is "committed" and when the `.onChange()` event is fired.

### Mode A: Continuous Debouncing (`.changeThreshold(T)`)

*   **Purpose:** For debouncing a smooth, continuous analog signal without forcing it into discrete steps.
*   **Ideal Use Case:** A potentiometer controlling LED brightness (0-255) where you don't want the `onChange` callback firing on every tiny jitter.
*   **Mechanism:** A simple, **"floating anchor" dead-zone**. It prevents the `onChange` event from firing unless the new `hiResValue` has changed by more than `T` units from the *last committed value*.
*   **Example Trace (`.changeThreshold(5)`):**
    *   Last committed value is `100`. The dead-zone is `96-104`.
    *   `hiResValue` stream: `101, 102, 103, 104, 105...`
    *   `onChange` fires **only at 105**. The anchor now "floats" to `105`.

### Mode B: Discrete Stability (`.quantize(Q, H)`)

*   **Purpose:** For converting a noisy analog signal into a rock-solid, discrete set of values on a **uniform grid**. This is the most powerful stability mode.
*   **Ideal Use Case:** A volume knob that should only output `0, 10, 20...`; a sensor that should snap to whole-degree temperature readings; or as a signal conditioner for the Zone system.
*   **Mechanism:** The advanced, **"grid-anchored" Quantized Hysteresis** algorithm. This creates a stable dead-zone around the midpoint of every grid line.
    *   `Q (step)`: The quantization step, which defines the grid. `quantize(10)` creates the grid `0, 10, 20...`.
    *   `H (hysteresis)`: The amount the value must cross *past* the midpoint to trigger a change.
*   **API:**
    *   `.quantize(step)`: The simple version. `H` is automatically calculated as `Q/4` for a balanced feel.
    *   `.quantize(step, hysteresis)`: The advanced version for explicitly setting `H` to tune the sensor's "feel."

| Feature | Core Purpose | Best For... | Analogy |
| :--- | :--- | :--- | :--- |
| **`.changeThreshold(T)`** | **Debouncing a continuous signal** | A smooth slider that shouldn't fire events on every tiny wiggle. | A simple floating window of tolerance. |
| **`.quantize(Q, H)`** | **Stabilizing a discrete signal** | A 5-position switch or a stepped volume knob that must not oscillate between levels. | A series of fixed, stable detents. |

**Rule of Precedence:** These modes are mutually exclusive. If `.quantize()` is called, the `.changeThreshold()` setting is ignored.

---

## 5. The Zone System: High-Level State Management

The Zone System is the library's most advanced feature. It builds on top of the stable signal to create a powerful, declarative state machine. Its purpose is to fire an event **only on the single moment the sensor's value transitions into a new, user-defined range.**

*   **Core Concept:** A Zone is a range of values associated with a specific ID. You can define any number of zones, even with non-uniform sizes and gaps between them.
*   **The Magic:** The library internally tracks the sensor's previous zone and current zone. The `.onZoneChange()` callback is only fired when they differ. This abstracts away all the complex state-tracking logic from your code.

### The API

*   **`.addZone(byte id, int min_value, int max_value)`:** Defines a new zone with a unique ID and an inclusive range.
*   **`.onZoneChange(callback)`:** Registers a callback that receives the ID of the newly entered zone.

### The Golden Path: Layering for Ultimate Stability

For the most robust possible system, you should use a **two-layer stability model**.

1.  **Layer 1: Signal Conditioning (`.quantize`)**: First, use `.quantize()` to turn the noisy signal into a stable, discrete integer. A `quantize(1)` is often perfect for this, as it kills all jitter around the whole numbers.
2.  **Layer 2: State Machine (`.addZone`)**: Then, build your Zone system on top of that perfectly clean signal.

This layering ensures that the Zone logic is never exposed to a flickering value, guaranteeing that the `onZoneChange` event fires exactly once per transition.

---

## 6. Practical Application & Recipes

### Using Presets (`.configure()`)

For common tasks, the library provides pre-configured "recipes" to get you started instantly. Call `.configure()` *first* in your chain, then optionally override any setting.

| Preset | Description | Key Settings |
| :--- | :--- | :--- |
| `RAW_DATA` | The default. No processing. | `outputRange(0, 1023)` |
| `POT_FOR_LED` | Smooth control for an LED. | `smoothing(8)`, `outputRange(0, 255)`, `changeThreshold(2)` |
| `POT_FOR_SERVO` | Smooth control for a servo. | `smoothing(8)`, `outputRange(0, 180)`, `changeThreshold(2)` |
| `POT_FOR_PERCENTAGE` | A stable 0-100% knob. | `smoothing(10)`, `outputRange(0, 100)`, `quantize(5)` |
| `SWITCH_5_POSITION`| A heavy, 5-position switch. | `smoothing(12)`, `outputRange(0, 4)`, `quantize(1, 1)` |

**Example: A Customized Preset**
```cpp
// Start with the percentage preset, but make it more stable and use larger steps.
mySensor.configure(AnalogSensor::POT_FOR_PERCENTAGE) // Starts with Q=5
        .smoothing(25)                             // Override smoothing
        .quantize(10);                             // Override quantization
```

### Building From Scratch: Examples

#### Example 1: The Simple Analog Dimmer
```cpp
// Goal: A smooth, responsive dimmer for an LED.
device.newAnalogSensor(A0)
  .outputRange(0, 255)      // Map to LED brightness
  .smoothing(10)            // Low smoothing for responsiveness
  .changeThreshold(4)       // Only fire onChange for meaningful changes
  .onChange(dimLED);
```

#### Example 2: The 5-Position Mode Switch
```cpp
// Goal: A rock-solid 5-position switch (0-4) from a potentiometer.
device.newAnalogSensor(A0)
  .outputRange(0, 4)        // Map to 5 distinct values
  .smoothing(15)
  // Use aggressive Quantized Hysteresis.
  // Q=1 snaps to whole numbers.
  // H=1 creates a very wide, stable dead-zone between numbers,
  // making it feel like a switch with heavy "detents".
  .quantize(1, 1)
  .onChange(setMode);
```

#### Example 3: The Thermostat (The "Golden Path")
```cpp
#define ZONE_HEATER 1
#define ZONE_NORMAL 2
#define ZONE_COOLER 3

device.newAnalogSensor(A0)
  .outputRange(0, 100)      // Sensor mapped to Fahrenheit
  .smoothing(20)            // High smoothing for stable temp readings
  
  // --- Layer 1: Signal Conditioning ---
  // Stabilize the signal into clean, whole-degree steps.
  .quantize(1, 0) // Q=1, H=auto(0). Kills all sub-integer jitter.
  
  // --- Layer 2: State Machine ---
  // Define zones with a dead-zone for hysteresis.
  .addZone(ZONE_HEATER, 0, 67)
  .addZone(ZONE_NORMAL, 68, 74) // The dead-zone
  .addZone(ZONE_COOLER, 75, 100)
  .onZoneChange(handleZoneChange);
```

---

## 7. Quick Reference API

*   **`.smoothing(byte samples)`**: Averages `samples` readings. Default: `1`.
*   **`.inputRange(int min, int max)`**: Calibrates the expected raw ADC range. Default: `0, 1023`.
*   **`.outputRange(int min, int max)`**: Maps the value to a new scale. Default: `0, 1023`.
*   **`.invert()`**: Flips the final value within the `outputRange`.
*   **`.changeThreshold(int delta)`**: Enables continuous debouncing mode. `onChange` fires when `abs(new - old) >= delta`.
*   **`.quantize(int step)`**: Enables discrete stability mode. `H` is automatically set to `step / 4`.
*   **`.quantize(int step, int hysteresis)`**: Enables discrete stability mode with an explicit `H` value.
*   **`.addZone(byte id, int min, int max)`**: Defines a stateful zone.
*   **`.configure(Preset preset)`**: Applies a pre-configured recipe.
*   **`.onChange(void (*callback)(int value))`**: Registers a callback for value changes.
*   **`.onZoneChange(void (*callback)(byte zoneID))`**: Registers a callback for zone changes.
*   **`int value()`**: Returns the last stable, committed value of the sensor.