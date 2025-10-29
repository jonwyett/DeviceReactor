# Design Document: The Quantized Hysteresis Algorithm

## 1. Introduction: The Core Problem of Analog Instability

When processing analog sensor data, the goal is often to convert a noisy, continuous signal into a clean, discrete set of values. A common technique for this is **quantization**, which "snaps" a reading to the nearest point on a predefined grid. For example, mapping a 0-1023 sensor reading to a 0-100 scale and then quantizing it in steps of 10 should produce values like 0, 10, 20, 30, etc.

However, a simple quantization algorithm has a critical failure mode. An analog signal is never perfectly stable; due to electrical noise, thermal drift, and other factors, it will naturally fluctuate. If a sensor's true value hovers near the midpoint of a quantization step (e.g., 65 on a 0-100 scale), a simple rounding function will produce a wildly oscillating output:

`Smoothed Sensor Value: 64.8, 64.9, 65.0, 65.1, 64.9, 65.2...`
`Quantized Output:      60,   60,   70,   70,   60,   70...`

This oscillation is unacceptable for most applications, leading to flickering displays, chattering relays, and unstable control systems. The primary challenge is to create a system that produces stable, discrete values without this unwanted behavior.

## 2. The Design Goals

To solve this problem, we established two competing design goals. The ideal algorithm must strike a perfect balance between them:

1.  **Value Stability:** The output value must be stable. It should not change unless the sensor's input has made a *decisive* and significant move. It must not oscillate due to minor, real-world noise.
2.  **Value Accuracy:** The output value must be a predictable and true representation of the sensor's state. It should not get "stuck" on an old value or drift over time. The behavior must be consistent, regardless of the sensor's starting position or history.

## 3. The Solution: Quantized Hysteresis

The solution is a robust algorithm called **Quantized Hysteresis**.

The core principle is to create an intelligent "dead-zone" around each quantized step. This dead-zone prevents the output from changing until the input signal has moved decisively into the next zone. Crucially, this dead-zone is mathematically **anchored to the quantization grid itself**, which solves the problems of inaccuracy and drift.

The algorithm is defined by two key parameters:

*   **Quantization Step (Q):** This defines the grid of stable values. For example, `Q=10` creates the grid `..., 60, 70, 80, ...`.
*   **Hysteresis Amount (H):** This defines the size of the "spongy" dead-zone. It is the amount the signal must travel *past* the ideal midpoint boundary before a change is triggered.

### How It Works

The algorithm prevents oscillation by establishing two different trigger thresholds for every boundary: one for increasing values and one for decreasing values.

Let's consider the boundary between `60` and `70`, where the ideal midpoint is `65`.

1.  **Trigger to Increase Value:** To change the output from `60` to `70`, the input signal must not just cross the midpoint of `65`; it must exceed it by the Hysteresis amount `H`.
    *   **`Trigger UP = Midpoint + H`**
    *   If `H=3`, the trigger point is `65 + 3 = 68`. The output will not become `70` until the input value is greater than `68`.

2.  **Trigger to Decrease Value:** To change the output from `70` back down to `60`, the input signal must cross the midpoint of `65` and go below it by the Hysteresis amount `H`.
    *   **`Trigger DOWN = Midpoint - H`**
    *   If `H=3`, the trigger point is `65 - 3 = 62`. The output will not become `60` until the input value is less than `62`.

This creates a stable dead-zone between `62` and `68`. Any sensor noise or fluctuation within this range will have **zero effect** on the output value.

```
      ◄─── The Dead-Zone ───►
      Hysteresis │ Hysteresis
          (H=3)  │  (H=3)
◄────────────────┼────────────────►
<---- Value is 60  [65] Value is 70 ---->
                 │
           Ideal Midpoint

◄─── 62          │          68 ───►
      │          │          │
      └─ Trigger to go DOWN └─ Trigger to go UP
```

### Self-Correcting Accuracy

To ensure the system is always accurate and predictable, the algorithm anchors itself on the very first reading.

*   **On Initialization:** The first sensor reading is taken and immediately snapped to its nearest quantization grid point. If the sensor starts at `63`, the system's internal value is set to `60`. This prevents an arbitrary starting point from creating an offset in the dead-zone, immediately solving the accuracy problem. The system is always perfectly aligned with the grid.

## 4. Why This Approach is Superior

This Quantized Hysteresis algorithm successfully achieves both of our primary design goals.

*   **It Achieves Perfect Stability:** By creating a dead-zone around every quantization boundary, all minor oscillations are completely ignored. The output is rock-solid until a deliberate and significant change is made.

*   **It Maintains Predictable Accuracy:** By anchoring the dead-zones to the fixed quantization grid, the behavior is always consistent and repeatable. A user can rely on the fact that the trigger point to go from `60` to `70` will *always* be the same, eliminating frustrating inconsistencies.

*   **It is Highly Tunable:** The "feel" of the sensor can be tuned by adjusting the relationship between `Q` and `H`. A small `H` relative to `Q` creates a responsive, sensitive feel. A larger `H` creates a "heavier," more deliberate feel with higher noise immunity. This allows the algorithm to be adapted to a wide variety of applications, from sensitive sliders to heavy-duty mode switches. A balanced default (`H = Q / 4`) provides excellent results for most common use cases.

## 5. The Signal Processing Pipeline

To correctly implement the Quantized Hysteresis algorithm, it must be the final step in a larger signal processing chain. Each step in this pipeline serves to progressively refine the raw sensor data, ensuring that the final logic operates on a clean, predictable signal. The order of these operations is critical.

The complete pipeline, from raw hardware reading to final stable output, is as follows:

1.  **Read Raw ADC Value:** The process begins by taking a raw, integer reading from the hardware's Analog-to-Digital Converter (e.g., a value from 0-1023 on an Arduino).

2.  **Smooth the Raw Signal:** To combat high-frequency noise, the last N raw readings are averaged. This smoothing is most effective when performed on the raw, high-resolution data before any information is lost through mapping or clamping.

3.  **Calibrate and Clamp the Input:** The smoothed value is then clamped to a user-defined input range. This step is for sensor calibration, allowing the user to specify the sensor's real-world minimum and maximum readings (e.g., a photoresistor that only outputs values from 150 to 850).

4.  **Map to Output Range:** The calibrated value is then mapped from the input range to the desired user-facing output range (e.g., 0-100). This produces a high-resolution, floating-point or integer value that represents the sensor's current state in the desired scale. We will refer to this as the **`hiResValue`**.

5.  **Apply Quantized Hysteresis Logic:** This is the final and most critical step. The `hiResValue` is fed into the Quantized Hysteresis algorithm. The algorithm compares this real-time value against the last stable *reported* value to determine if a state change is warranted. It is this step that prevents oscillation.

6.  **Update State and Fire Events:** Only if the Quantized Hysteresis logic confirms a decisive change does the system update its public-facing value. When the value is updated, an `onChange` event is fired to notify the rest of the program.

## 6. Detailed Algorithm and State Management

To implement the pipeline, the system must maintain at least one state variable:

*   **`currentReportedValue`:** The stable, quantized integer value that is exposed publicly. This is the value returned by `.value()` and passed to the `onChange` callback.

The algorithm uses this state in two distinct phases: Initialization and the Update Loop.

### Initialization

To solve the problem of an arbitrary starting position creating an accuracy offset, the system must be anchored to the quantization grid on its very first run.

1.  Perform a complete pass of the signal processing pipeline (Steps 1-4) to get the initial `hiResValue`.
2.  Immediately quantize this value to find its true grid point:
    `currentReportedValue = round(initialHiResValue / Q) * Q`
3.  This value is now stored as the initial stable state.

### The Update Loop

On every subsequent update, the full logic is executed:

1.  Calculate the latest `hiResValue` by executing steps 1-4 of the signal processing pipeline.
2.  Let `V_old` be the current stable value (`currentReportedValue`).
3.  Determine the `V_potential`, which is the ideal quantized value for the current `hiResValue`:
    `V_potential = round(hiResValue / Q) * Q`
4.  If `V_potential` is the same as `V_old`, the value has not crossed an ideal midpoint. Do nothing and end the update.
5.  **If `V_potential` is greater than `V_old` (value is increasing):**
    *   Calculate the upper trigger threshold: `trigger_up = (V_old + Q/2) + H`
    *   Check if `hiResValue >= trigger_up`.
    *   If true, a decisive change has occurred. Commit the new value:
        *   `currentReportedValue = V_potential`
        *   Fire `onChange(currentReportedValue)` event.
6.  **If `V_potential` is less than `V_old` (value is decreasing):**
    *   Calculate the lower trigger threshold: `trigger_down = (V_old - Q/2) - H`
    *   Check if `hiResValue <= trigger_down`.
    *   If true, a decisive change has occurred. Commit the new value:
        *   `currentReportedValue = V_potential`
        *   Fire `onChange(currentReportedValue)` event.

## 7. The Final `AnalogSensor` Architecture: Signal Processing Modes and Event Channels

The `AnalogSensor` class provides two distinct, mutually exclusive modes for signal processing ("cleaning"), and two distinct event channels to react to changes.

### Two Signal Processing Modes:

1.  **Continuous Debouncing Mode (`.changeThreshold(T)`)**
    *   **Purpose:** For debouncing a smooth, continuous analog signal.
    *   **Ideal Use Case:** A potentiometer controlling LED brightness (0-255) where you don't want the `onChange` callback firing on every tiny jitter.
    *   **Mechanism:** A simple, "floating anchor" dead-zone.

2.  **Discrete Stability Mode (`.quantize(Q, H)`)**
    *   **Purpose:** For converting a noisy analog signal into a rock-solid, discrete set of values on a **uniform grid**.
    *   **Ideal Use Case:** A volume knob that should only output `0, 10, 20...` or a sensor that should snap to whole-degree temperature readings.
    *   **Mechanism:** The advanced, "grid-anchored" Quantized Hysteresis algorithm.

### Two Event Channels:

1.  **`.onChange(callback)`**
    *   **Fires when:** The final, processed **value** changes.
    *   **Receives:** The new integer value (e.g., `128`, `20`, `72`).
    *   **Behavior:** Its firing logic is governed by the active signal processing mode (`changeThreshold` or `quantize`).

2.  **`.onZoneChange(callback)`**
    *   **Fires when:** The processed value crosses into a new, user-defined **zone**.
    *   **Receives:** The ID of the new zone (e.g., `1`, `2`, `3`).
    *   **Behavior:** It defines a **non-uniform grid** for state management. It can be used with or without a signal processing mode, but it is most powerful when layered on top of `.quantize()` for ultimate stability.

## 8. API Design: The Quantized Hysteresis Configuration (`.quantize()`)

The library's API for configuring Quantized Hysteresis is designed to be both simple for beginners and powerful for advanced users. It provides two overloaded versions of the `.quantize()` method.

### `.quantize(int step)`
*   **Purpose:** The simple, "it just works" method. It enables Quantized Hysteresis with a sane, balanced default for stability. This is the recommended approach for the majority of users.
*   **Behavior:**
    1.  Sets the **Quantization Step (Q)** to the provided `step`.
    2.  **Automatically calculates and sets a balanced Hysteresis amount (H)** internally, using the formula `H = Q / 4`. This provides an excellent out-of-the-box feel, balancing responsiveness with noise immunity.
    3.  Disables the Change Threshold mode if it was previously active.
*   **Example:**
    ```cpp
    // Creates a 0-100 sensor with 11 stable steps (0, 10, 20...).
    // Hysteresis is automatically set to 2 (10 / 4), providing a
    // great out-of-the-box feel.
    mySensor.outputRange(0, 100).quantize(10);
    ```

### `.quantize(int step, int hysteresis)`
*   **Purpose:** The advanced method for fine-tuning the sensor's "feel" and noise immunity.
*   **Who it's for:** Advanced users who need to override the default stability settings for specific applications or highly noisy environments.
*   **Behavior:**
    1.  Sets the **Quantization Step (Q)** to the provided `step`.
    2.  Sets the **Hysteresis amount (H)** to the explicitly provided `hysteresis` value.
    3.  Disables the Change Threshold mode if it was previously active.
*   **Example:**
    ```cpp
    // Creates a 4-position mode switch (0, 1, 2, 3).
    // The user wants a very "heavy" feel, so they provide a large
    // Hysteresis value of 1 (equal to Q).
    mySensor.outputRange(0, 3).quantize(1, 1);
    ```

## 9. High-Level Configuration with Data-Driven Presets (`.configure()`)

To improve usability, reduce boilerplate code, and enhance maintainability, the library provides a high-level `configure()` method. This method allows users to select from a list of pre-defined "recipes" that automatically set the underlying smoothing, range, and stability parameters to sensible defaults. This feature is implemented using a **data-driven design**, which cleanly separates configuration data from program logic.

### The `PresetConfig` Structure

A dedicated structure, `AnalogSensor::PresetConfig`, is defined to hold all the parameters for a single preset. This ensures that each recipe is a complete, self-contained unit of data.

```cpp
// Inside the AnalogSensor class definition in the .h file
struct PresetConfig {
    byte smoothing_samples;
    int output_min;
    int output_max;
    int quantize_step;      // Our "Q" value
    int hysteresis_amount;    // Our "H" value
    int change_threshold;   // Our "T" value for non-quantized mode
};
```

### The Configuration Table

A static, constant array of `PresetConfig` structures serves as the central "cookbook" for the library. Each entry in the array corresponds to a preset defined in the `AnalogSensor::Preset` enum. This table is highly memory-efficient, as it is stored once in program memory (Flash) and shared across all `AnalogSensor` instances.

The primary benefit of this data-driven design is **extensibility** and **maintainability**. To add a new preset to the library, a developer only needs to:
1.  Add a new name to the `Preset` enum.
2.  Add a corresponding line of data to the configuration table.

The core `.configure()` method does not need to be modified, eliminating the risk of introducing logic errors.

```cpp
// In your AnalogSensor.cpp file

// This array holds the configuration data for each preset.
// The order MUST EXACTLY MATCH the order of the Preset enum.
const AnalogSensor::PresetConfig AnalogSensor::preset_configs[] = {
    // Preset 0: RAW_DATA
    { /*smoothing*/ 1, /*out_min*/ 0, /*out_max*/ 1023, /*Q*/ 0, /*H*/ 0, /*T*/ 1 },

    // Preset 1: POT_FOR_LED
    { /*smoothing*/ 8, /*out_min*/ 0, /*out_max*/ 255,  /*Q*/ 0, /*H*/ 0, /*T*/ 2 },

    // Preset 2: POT_FOR_SERVO
    { /*smoothing*/ 8, /*out_min*/ 0, /*out_max*/ 180,  /*Q*/ 0, /*H*/ 0, /*T*/ 2 },

    // Preset 3: POT_FOR_PERCENTAGE
    // Note: T is irrelevant here because Q > 0
    { /*smoothing*/ 10, /*out_min*/ 0, /*out_max*/ 100, /*Q*/ 5, /*H*/ 1, /*T*/ 1 },

    // Preset 4: SWITCH_5_POSITION
    // Heavy H value to feel like a switch with detents
    { /*smoothing*/ 12, /*out_min*/ 0, /*out_max*/ 4,   /*Q*/ 1, /*H*/ 1, /*T*/ 1 }
};
```

### The `.configure(Preset preset)` Method

The `.configure()` method is now a simple and "automatic" engine. Its only job is to:
1.  Receive a `Preset` enum from the user.
2.  Use the enum's value as an index to look up the correct `PresetConfig` recipe in the data table.
3.  Call the base setter methods (`.smoothing()`, `.outputRange()`, `.quantize()`, etc.) using the values found in the recipe.

```cpp
// The new, data-driven configure method
AnalogSensor& AnalogSensor::configure(Preset preset) {
    // Cast the enum to an integer to use it as an array index.
    int index = static_cast<int>(preset);

    // Get the configuration "recipe" from our data table.
    const PresetConfig& config = preset_configs[index];

    // Apply the common settings.
    this->smoothing(config.smoothing_samples)
        .outputRange(config.output_min, config.output_max);

    // Apply the stability logic based on the recipe.
    if (config.quantize_step > 0) {
        // Quantization is enabled in this recipe.
        // If H is 0 in the table, we use the auto-calculated default.
        int hysteresis = (config.hysteresis_amount > 0)
                            ? config.hysteresis_amount
                            : config.quantize_step / 4;
        this->quantize(config.quantize_step, hysteresis);
    } else {
        // This is a continuous (non-quantized) recipe.
        this->changeThreshold(config.change_threshold);
    }

    return *this;
}
```

### Usage and Customization

The `.configure()` method should be the *first* method called in a configuration chain. Any subsequent setter calls will override the preset's values, providing both a simple starting point and the ability to customize.

**Example of a Customized Preset:**
```cpp
// I want the standard percentage knob, but my sensor is very noisy
// and I need it to be in steps of 10, not 5.
device.newAnalogSensor(A0)
      .configure(AnalogSensor::POT_FOR_PERCENTAGE) // Start with the preset...
      .smoothing(20)                               // ...then override smoothing...
      .quantize(10);                               // ...and override the quantization.
```

## 10. Zone-Based State Management (`.addZone()` and `.onZoneChange()`)

For applications that need to react when a sensor value enters specific ranges, `DeviceReactor` provides a powerful and flexible **Zone System**. This is the ideal solution for building thermostats, multi-position switches, fluid-level controllers, or any system that has discrete states based on a continuous analog input. The library manages the complexity of state transitions internally, firing an event **only when the active zone changes.**

The core idea is that an `AnalogSensor` doesn't just have a value; it can be in one of several user-defined **Zones**. The library's job is to manage which zone the sensor is in and notify the user *only when the zone changes*. This system generalizes the concept of `onHigh`/`onLow` into an arbitrary number of user-defined ranges.

### Defining Zones: `.addZone(byte id, int min_value, int max_value)`
*   **Purpose:** Defines a new zone for the sensor.
*   **Parameters:**
    *   `id`: A unique number you assign to this zone (e.g., 1, 2, 3...). This ID will be passed to the `onZoneChange` callback.
    *   `min_value`, `max_value`: The inclusive range of the sensor's output value for this zone.
*   **Behavior:** Adds this zone definition to an internal list for the sensor. Zones can be defined with gaps between them to create implicit hysteresis.

**Example: Defining Thermostat Zones**
```cpp
// Zones are inclusive. Note the 1-unit gap for hysteresis.
// 0-68 is Zone 1 (HEATER)
// 69-74 is Zone 2 (NORMAL)
// 75-100 is Zone 3 (COOLER)
tempSensor.addZone(1, 0, 68)
          .addZone(2, 69, 74)
          .addZone(3, 75, 100);
```

### The Event Callback: `.onZoneChange(callbackFunction)`
*   **Purpose:** Registers a single callback that fires whenever the active zone changes.
*   **Callback Signature:** `void myFunction(byte newZoneID)`
*   **Behavior:** The callback is automatically called with the new Zone ID only on the frame that the sensor's value transitions from one zone to another.

**Example: The Thermostat using a Single Callback**
```cpp
#define ZONE_HEATER 1
#define ZONE_NORMAL 2
#define ZONE_COOLER 3

void setup() {
    device.newAnalogSensor(A0)
      .outputRange(0, 100) // Mapped to Fahrenheit
      .smoothing(20)
      // Define the zones
      .addZone(ZONE_HEATER, 0, 68)
      .addZone(ZONE_NORMAL, 69, 74)
      .addZone(ZONE_COOLER, 75, 100)
      // Register the single callback
      .onZoneChange(handleZoneChange);
}

void handleZoneChange(byte newZone) {
    Serial.print("Entering new zone: ");
    Serial.println(newZone);

    switch (newZone) {
        case ZONE_HEATER:
            turnHeaterOn();
            break;
        case ZONE_NORMAL:
            turnEverythingOff();
            break;
        case ZONE_COOLER:
            turnCoolerOn();
            break;
    }
}

void loop() {
  device.update();
}
```

### Implementation Details (Conceptual)

1.  **The `Zone` Struct:** Internally, the library would manage a collection of `Zone` structs:
    ```cpp
    struct Zone {
        byte id;
        int min_val;
        int max_val;
    };
    ```
2.  **State Variables:** Each `AnalogSensor` instance needs to track `byte currentZoneID` and `byte previousZoneID`.
3.  **Logic in `update()`:** After the final `currentValue` is calculated, the `update()` loop iterates through the defined zones to find which zone `currentValue` falls into. If this new zone differs from `previousZoneID`, the `onZoneChange` callback is fired, and `previousZoneID` is updated.

## 11. The Complete Architecture: A Two-Layer System for Ultimate Stability

The `AnalogSensor` architecture culminates in a powerful two-layer system that addresses both signal noise and state management with unparalleled robustness. This design recognizes that while the Zone System acts as a state machine, it operates most reliably when fed a clean, stable signal.

### Layer 1: The Signal Conditioner (`.quantize()`)

*   **Purpose:** To transform a noisy, fluctuating analog signal into a decisive, stable, discrete value.
*   **Mechanism:** The full **Quantized Hysteresis** (`Q+H`) algorithm.
*   **Analogy:** This layer acts as a Schmitt trigger, cleaning up the signal before any state-based logic processes it. It prevents micro-oscillations of the raw value.

### Layer 2: The State Machine (`.addZone()` + `.onZoneChange()`)

*   **Purpose:** To take the clean, stable value from Layer 1 and determine if it represents a meaningful change in the system's state.
*   **Mechanism:** The **Zone System**, which tracks the previous and current zones and fires a callback only on a transition.
*   **Analogy:** This layer acts as the logic gate (or microcontroller) that receives a clean input from the signal conditioner and acts upon it. It defines a **non-uniform quantization** grid, where each step corresponds to a user-defined zone.

### How it Works in Practice (The API)

Users chain the methods in the logical order of the processing pipeline. The library applies them in that order, ensuring that the signal is conditioned before zone logic is applied.

**Example: A Robust Thermostat Combining Layers**
```cpp
// Correct API usage to build a robust thermostat
device.newAnalogSensor(A0)
    .outputRange(0, 100)
    .smoothing(20)

    // --- LAYER 1: Signal Conditioning ---
    // First, stabilize the input signal itself. We'll snap it to whole
    // degrees with a hysteresis of 1 degree to prevent any flicker.
    // The value passed to the Zone checker will now be perfectly stable.
    .quantize(1, 1)  // Q=1, H=1

    // --- LAYER 2: State Machine ---
    // Now, define the zones that will operate on that clean signal.
    .addZone(ZONE_HEATER, 0, 68)
    .addZone(ZONE_NORMAL, 69, 74)
    .addZone(ZONE_COOLER, 75, 100)
    .onZoneChange(handleZoneChange);
```

### Why This Two-Layer Approach is So Powerful

*   **Separation of Concerns:** The user can solve the noise problem and the state logic problem independently. This allows for flexible tuning:
    *   If a sensor is extremely clean, `.quantize()` might be skipped, relying on the gaps between zones for implicit hysteresis.
    *   For very noisy sensors, aggressive `smoothing` and `quantize` settings can be applied, and the Zone logic remains unaffected.
*   **Complete Stability:** This two-layer approach is bulletproof:
    1.  `Q+H` prevents the *value* from oscillating at a micro-level.
    2.  The gaps between zones provide *hysteresis* for the state machine at a macro-level.
    3.  The `onZoneChange` logic ensures the callback is a *one-shot trigger* on state entry.
*   **Generalization:** The realization that "zones are a non-uniform quantization" clarifies the entire architecture. The library provides both uniform (`.quantize()`) and non-uniform (`.addZone()`) ways to discretize an analog signal, with a robust event system to react to changes in that discrete state.







