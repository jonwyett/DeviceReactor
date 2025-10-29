# Analog Sensor Hysteresis and Noise Management

This document discusses the challenges of working with noisy analog sensors in the DeviceReactor library and considerations for implementing threshold-based features like `onHigh()` and `onLow()`.

---

## The Problem: Analog Sensor Noise

### Real-World Analog Behavior

Analog sensors on Arduino rarely produce clean, stable readings. Even when physically stationary, a sensor might output:
```
Raw ADC readings from stationary potentiometer:
512, 513, 512, 514, 511, 513, 512, 515, 512, 511...
```

This noise comes from:
- **Electrical interference** (EMI from motors, WiFi, power supplies)
- **ADC quantization** (10-bit ADC = ±1-2 counts natural jitter)
- **Thermal drift** (components change resistance with temperature)
- **Poor connections** (breadboard contact resistance, wire length)
- **Power supply noise** (affects ADC reference voltage)

### Why This Matters for Thresholds

Consider a potentiometer controlling a fan with threshold detection:
```cpp
device.newAnalogSensor(A0)
  .outputRange(0, 100)
  .onHigh(70, turnFanOn)
  .onLow(65, turnFanOff);
```

**What the user expects:**
- Turn knob past 70 → fan turns on
- Turn knob back below 65 → fan turns off
- Clean state transitions

**What actually happens without proper handling:**
```
Readings near threshold: 69, 71, 70, 69, 71, 72, 70, 69, 71...
Callbacks fired:         -   ON  OFF ON  OFF -   ON  OFF ON...
Result: Fan rapidly cycles on/off (relay clicking, motor stress)
```

---

## Current DeviceReactor Implementation

### Existing Noise Mitigation Features

The AnalogSensor class already has two tools to combat noise:

#### 1. **`.smoothing(samples)`** - Averaging Filter
```cpp
.smoothing(10)  // Average 10 readings before processing
```

**What it does:**
- Accumulates N raw ADC readings
- Calculates average before mapping/processing
- Updates only every N calls to `update()`

**Memory cost:** ~8 bytes per sensor (accumulator + counter)

**Pros:**
- Reduces high-frequency noise significantly
- Simple moving average
- Already implemented

**Cons:**
- Introduces latency (must collect N samples)
- Slows response time
- May not fully eliminate threshold bouncing

#### 2. **`.changeThreshold(delta)`** - Change Detection Filter
```cpp
.changeThreshold(5)  // Only trigger onChange when value changes by ≥5
```

**What it does:**
- Compares new value to previous value
- Only fires `onChange()` callback if `abs(newValue - previousValue) >= delta`
- Effectively creates "dead zones" around the previous value

**Memory cost:** ~0 bytes (reuses existing delta/previousValue variables)

**Pros:**
- Prevents callback spam from small fluctuations
- No latency (immediate when threshold exceeded)
- Already implemented and working

**Cons:**
- Only affects `onChange()` callback
- Doesn't prevent the internal `currentValue` from updating
- Not specifically designed for threshold crossing detection

---

## The Challenge: Adding onHigh() and onLow()

### What We Need to Track

For threshold-based callbacks, we need to track **state**, not just value changes:

```
Sensor value over time:
  100 ║     ╱────────╲
   90 ║    ╱          ╲
   70 ║───┼─────HIGH──┼─── (highThreshold)
   50 ║  ╱ MIDDLE      ╲
   30 ║─┼────LOW────────┼─ (lowThreshold)
   10 ║ │               │
      ║ A               B

Point A: Transition LOW → HIGH (fire onHigh)
Point B: Transition HIGH → LOW (fire onLow)
```

### Three-State Model

The sensor exists in one of three states at any time:
- **LOW**: value ≤ lowThreshold
- **MIDDLE**: lowThreshold < value < highThreshold
- **HIGH**: value ≥ highThreshold

**State transitions that should fire callbacks:**
- `LOW → MIDDLE`: *(no callback)*
- `LOW → HIGH`: fire `onHigh()` *(rare, jumped across middle)*
- `MIDDLE → LOW`: fire `onLow()`
- `MIDDLE → HIGH`: fire `onHigh()`
- `HIGH → MIDDLE`: *(no callback)*
- `HIGH → LOW`: fire `onLow()` *(rare, jumped across middle)*

**Why we need this:**
- Can't just check `if (value > 70)` on every update (would fire repeatedly)
- Must detect the **crossing event** (state transition)
- Must remember what state we were in previously

### The Hysteresis Gap Problem

Even with smoothing and changeThreshold, we can still get rapid state transitions:

```
Scenario: Potentiometer held steady near threshold
- smoothing(10): readings averaged
- changeThreshold(2): small changes ignored
- onHigh(70): threshold set at 70

Smoothed readings: 68, 69, 70, 71, 70, 69, 70, 71, 70, 69...
State:             M   M   H   H   M   M   H   H   M   M...
Callbacks:             ON      OFF     ON      OFF
Result: Still bouncing!
```

**The root issue:** A single threshold value creates a **critical boundary** where any noise causes state flipping.

### Classic Solution: Hysteresis

Instead of one threshold, use **two different thresholds** depending on direction:

```
Enter HIGH state:  value must exceed 70
Exit HIGH state:   value must drop below 68

  75 ║        ╱────────╲
  70 ║───────┼─enter H─┼───── (entering threshold)
  68 ║───────┼─exit H──┼───── (exiting threshold)
  65 ║      ╱ MIDDLE    ╲
  60 ║─────┼────────────┼────
     ║
```

**With 2-unit hysteresis:**
- Reading 69.5 (noise): already HIGH, stays HIGH (below 70 but above 68)
- Reading 67.5: triggers exit, transitions to MIDDLE
- No bouncing in the gap between 68-70

---

## Design Considerations for Implementation

### Option 1: User Responsibility (Document Only)
```cpp
// User must configure properly - document in README
device.newAnalogSensor(A0)
  .smoothing(10)           // Smooth noise
  .changeThreshold(3)      // Ignore small changes
  .onHigh(70, turnFanOn)
  .onLow(65, turnFanOff);  // 5-unit gap = manual hysteresis
```

**Pros:** No code changes, maximum user control
**Cons:** Easy to misuse, footgun for beginners, confusing API (why two thresholds?)

### Option 2: Automatic Hysteresis Based on changeThreshold
```cpp
device.newAnalogSensor(A0)
  .changeThreshold(5)      // Use this as hysteresis gap
  .onHigh(70, callback);   // Enters at 70, exits at 65
```

**Pros:** Reuses existing setting, no new parameters
**Cons:** Not obvious that changeThreshold affects onHigh/onLow behavior

### Option 3: Separate Hysteresis Parameter
```cpp
.onHigh(70, 3, callback)  // threshold=70, hysteresis=3 (exit at 67)
```

**Pros:** Explicit and clear
**Cons:** Extra parameter, API verbosity, easy to forget

### Option 4: Smart Default with Override
```cpp
// Default: auto-calculate hysteresis (2-3 units or 2% of range)
.onHigh(70, callback)

// Advanced: explicit control
.changeThreshold(5)  // Also used for hysteresis
.onHigh(70, callback)
```

**Pros:** Works well by default, configurable for advanced users
**Cons:** "Magic" behavior may surprise some users

---

## Interaction Between Features

### How smoothing, changeThreshold, and hysteresis interact:

```
ADC Reading (raw)
     ↓
[smoothing] ← accumulates N samples, averages
     ↓
Smoothed raw value
     ↓
[map()] ← input range → output range
     ↓
[invert] ← if enabled (future feature)
     ↓
[clamp] ← to output range
     ↓
Mapped value
     ↓
[threshold checks] ← onHigh/onLow with hysteresis
     ↓ (state change?)
     ↓ YES → fire onHigh/onLow callbacks
     ↓
[change detection] ← abs(new - previous) >= changeThreshold?
     ↓ YES → fire onChange callback
     ↓
Update previousValue, currentValue
```

**Key insight:** Threshold hysteresis and changeThreshold serve different purposes:
- **Hysteresis:** Prevents state bouncing (LOW/MIDDLE/HIGH)
- **changeThreshold:** Prevents onChange callback spam

Both can be active simultaneously and complement each other.

---

## Memory Considerations

### Current AnalogSensor Memory Usage (per sensor):
```cpp
// Existing members
byte pin;                    // 1 byte
int inputMin, inputMax;      // 4 bytes
int outputMin, outputMax;    // 4 bytes
byte delta;                  // 1 byte (changeThreshold)
int currentValue;            // 2 bytes
int previousValue;           // 2 bytes
byte avgSamples;             // 1 byte
byte avgCount;               // 1 byte
long accumulatedSum;         // 4 bytes
bool hasChangeFunc;          // 1 byte (could be bit-packed)
byteParamCallback changed;   // 2-4 bytes (function pointer)
// Total: ~23-25 bytes per sensor
```

### Additional Members Needed for onHigh/onLow:
```cpp
byte thresholdState;         // 1 byte (LOW/MIDDLE/HIGH enum)
int highThreshold;           // 2 bytes
int lowThreshold;            // 2 bytes
int hysteresisGap;           // 2 bytes
bool hasHighCallback;        // 1 bit (can pack)
bool hasLowCallback;         // 1 bit (can pack)
voidCallback onHighFunc;     // 2-4 bytes
voidCallback onLowFunc;      // 2-4 bytes
// Additional: ~14-16 bytes per sensor
```

**Total with thresholds: ~37-41 bytes per sensor**

In a memory-constrained environment (Arduino Uno = 2KB RAM):
- 10 sensors without thresholds: 250 bytes
- 10 sensors with thresholds: 410 bytes
- Cost: 160 bytes for the feature

**Optimization opportunity:** Only allocate threshold state if user calls `onHigh()` or `onLow()`. This would require more complex memory management or compile-time configuration.

---

## Real-World Example: Potentiometer-Controlled Fan

### Without Proper Configuration (BAD)
```cpp
device.newAnalogSensor(A0)
  .outputRange(0, 100)
  .onHigh(70, turnFanOn);

// Readings: 69, 71, 70, 69, 71, 70, 71...
// Behavior: Fan clicks on/off rapidly, relay damaged
```

### With Smoothing Only (BETTER)
```cpp
device.newAnalogSensor(A0)
  .smoothing(10)
  .outputRange(0, 100)
  .onHigh(70, turnFanOn);

// Smoother readings: 69.2, 69.8, 70.1, 69.9, 70.3...
// Behavior: Still some bouncing, but less frequent
```

### With Smoothing + Hysteresis (GOOD)
```cpp
device.newAnalogSensor(A0)
  .smoothing(10)
  .changeThreshold(3)  // 3-unit hysteresis
  .outputRange(0, 100)
  .onHigh(70, turnFanOn)
  .onLow(60, turnFanOff);  // 10-unit separation (belt-and-suspenders)

// Behavior: Solid state transitions, no bouncing
```

### With Auto Hysteresis (IDEAL)
```cpp
device.newAnalogSensor(A0)
  .smoothing(5)
  .outputRange(0, 100)
  .onHigh(70, turnFanOn)
  .onLow(60, turnFanOff);

// Library automatically applies 2-3 unit hysteresis
// User doesn't need to think about it
// Behavior: Clean transitions out of the box
```

---

## Recommendations for Implementation

1. **Use three-state model** with single `byte thresholdState` enum (cleaner than 3 bools)

2. **Apply automatic hysteresis** to threshold checks:
   - If user set `changeThreshold()`, use that value
   - Otherwise, default to `max(2, (outputMax - outputMin) / 50)` (2% of range)
   - This prevents most naive usage footguns

3. **Document clearly in README:**
   - Explain that analog sensors are noisy
   - Show examples with and without smoothing
   - Recommend `smoothing(5-10)` for most use cases
   - Explain how hysteresis works (even if automatic)

4. **Provide debug feedback:**
   ```cpp
   #ifdef DEVICE_REACTOR_DEBUG
     DR_DEBUG_PRINT("Threshold state transition: ");
     DR_DEBUG_PRINT(oldState);
     DR_DEBUG_PRINT(" -> ");
     DR_DEBUG_PRINTLN(newState);
   #endif
   ```

5. **Consider callback signature options:**
   - No-param: `void turnFanOn()` - simple, most common
   - With-param: `void onThreshold(byte value)` - useful for logging
   - Current library uses `byteParamCallback` but could have both variants

6. **Test edge cases:**
   - Threshold exactly at outputMin or outputMax
   - Inverted ranges (inputMax < inputMin)
   - Very small output ranges (0-10)
   - Both onHigh and onChange configured simultaneously

---

## Summary

Analog sensor noise is a fundamental challenge in embedded systems. The DeviceReactor library already provides excellent tools (`smoothing()` and `changeThreshold()`), but adding threshold-based callbacks requires careful state management and hysteresis to avoid rapid state oscillation. The implementation should balance:

- **Ease of use:** Work well with sensible defaults
- **Memory efficiency:** ~15 bytes overhead per sensor
- **Flexibility:** Advanced users can tune behavior
- **Safety:** Prevent common footguns (bouncing relays)

The three-state model (LOW/MIDDLE/HIGH) with automatic hysteresis provides the best balance for most users while maintaining the library's philosophy of zero-cost abstractions and memory efficiency.
