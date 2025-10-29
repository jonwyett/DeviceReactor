# DeviceReactor

**Modern event-driven hardware framework for Arduino**

DeviceReactor provides a type-safe, fluent API for managing hardware components on Arduino-compatible microcontrollers. Write clean, expressive code without wrestling with debouncing, timing, or polling logic.

---

## Features

- **🎯 Type-Safe Handles** - Component references via byte indices eliminate runtime lookup errors
- **⛓️ Fluent API** - Method chaining makes code self-documenting and discoverable
- **🎛️ Generic Sensors** - A unified `AnalogSensor` class for potentiometers, photoresistors, etc.
- **⏱️ Modern Timers** - `device.after()`, `device.every()`, and `device.repeat()` for flexible timing
- **⚡ Zero-Cost Abstractions** - Only pay for components you use via compile-time optimization
- **🔧 Easy Configuration** - Define component counts in your sketch, not the library

---

## Installation

### Via Arduino Library Manager

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for **"DeviceReactor"**
4. Click **Install**

### Manual Installation

1. Download the [latest release](https://github.com/yourusername/DeviceReactor/releases)
2. Extract to your Arduino `libraries` folder (usually `~/Documents/Arduino/libraries/`)
3. Restart Arduino IDE

---

## Quick Start
```cpp
// 1. Define component counts BEFORE including the library
#define TOTAL_LEDS 1
#define TOTAL_BUTTONS 1

// 2. Include DeviceReactor
#include <DeviceReactor.h>

Device device;

byte statusLED;

void setup() {
  // 3. Create components - returns handles for later use
  statusLED = device.newLED(13);

  // 4. Configure with fluent API
  device.newButton(2)
    .onPress(buttonPressed);
}

void buttonPressed() {
  device.led(statusLED).flip();
}

void loop() {
  device.update();  // Required every loop
}
```

---

## Configuration

Define component counts **before** including the library:

```cpp
#define TOTAL_LEDS 5
#define TOTAL_BUTTONS 3
#define TOTAL_ANALOG_SENSORS 2
#define TOTAL_ROTARY_ENCODERS 1
#define TOTAL_INTERVALS 5

#include <DeviceReactor.h>
```

**Memory Optimization:** Omit any `#define` for components you don't use. Undefined components default to 0 and consume **zero** memory.

---

## API Reference

### LEDs

```cpp
// Create LED (returns handle)
byte led = device.newLED(pin);
byte rgb = device.newLED(pinR, pinG, pinB);  // RGB LED

// Basic control
device.led(led).turnOn();
device.led(led).turnOff();
device.led(led).flip();

// Dimming (PWM pin required)
device.led(led).setLevel(128);  // 0-255

// RGB colors
device.led(rgb).setColor(255, 0, 0);  // Red
device.led(rgb).setCommonAnode(false);  // For common cathode RGB LEDs (default: true)

// Effects
device.led(led).blink(500);           // Blink forever, 500ms interval
device.led(led).blink(500, 5);        // Blink 5 times
device.led(led).pulse(1000, 3, 0, 255);  // Pulse 3 times, 1 second cycle
device.led(led).fadeIn(1000);         // Fade in over 1 second
device.led(led).fadeIn(1000, 128);    // Fade in to level 128
device.led(led).fadeOut(500);         // Fade out over 500ms

// Method chaining
device.led(led).setLevel(200).turnOn();
device.led(rgb).setColor(0, 255, 0).blink(300);
```

---

### Buttons

```cpp
// Create and configure with fluent API
device.newButton(pin)
  .onPress(pressCallback)
  .onRelease(releaseCallback);

// Button modes (optional, defaults to INPUT_PULLUP)
device.newButton(pin, BUTTON_INPUT_PULLUP);  // Default
device.newButton(pin, BUTTON_PRESS_HIGH);
device.newButton(pin, BUTTON_PRESS_LOW);

// Automatic debouncing (50ms minimum between events, configurable via DEBOUNCE_DELAY)
```

---

### Analog Sensors

The `AnalogSensor` class provides a generic interface for any sensor that produces a variable voltage, read on an analog input pin. This is the class to use for **potentiometers, photoresistors (LDRs), thermistors, flex sensors**, and more.

It includes built-in features for smoothing noisy readings, mapping input values to a practical output range, and handling value changes.

```cpp
// Create with fluent configuration for a potentiometer
device.newAnalogSensor(A0)
  .outputRange(0, 100)      // Map to 0-100 scale
  .smoothing(10)            // Average 10 readings for stability
  .changeThreshold(5)       // Only fire callback when value changes by ±5
  .onChange(sensorChanged);

void sensorChanged(int value) {
  // value is mapped to outputRange (0-100 in this example)
  Serial.println(value);
}

// Advanced: Calibrate hardware range for a photoresistor
device.newAnalogSensor(A1)
  .inputRange(50, 950)      // If the sensor doesn't use the full 0-1023 range
  .outputRange(0, 255)      // Map to LED brightness
  .onChange(callback);

// Invert sensor values (useful for backwards sensors)
device.newAnalogSensor(A2)
  .outputRange(0, 100)
  .invert()                 // 20 becomes 80, 30 becomes 70, etc.
  .onChange(callback);      // Photoresistor: lower voltage = more light

// Quantize to fixed increments (snap to grid)
device.newAnalogSensor(A3)
  .outputRange(0, 100)
  .quantize(25)             // Returns ONLY 0, 25, 50, 75, 100
  .onChange(callback);      // Perfect for zone detection

// Get current value anytime
byte sensorHandle = device.newAnalogSensor(A0).outputRange(0, 100);
int currentValue = device.analogSensor(sensorHandle).value();
```

#### Understanding `changeThreshold()` vs `quantize()`

These two methods serve different purposes and can be used together:

**`.changeThreshold(delta)`** - Anti-noise filter for callbacks
- Prevents callback spam from sensor jitter
- Only fires `onChange()` when value changes by ≥ delta
- Does NOT change the actual values returned
- Example: `.changeThreshold(5)` with readings `23, 24, 26, 28, 29, 30, 33...` fires callbacks at `23, 28, 33` (arbitrary values)

**`.quantize(step)`** - Snap values to a fixed grid
- Rounds values to exact multiples (grid alignment)
- Returns clean, predictable values
- Perfect for zone detection or discrete states
- Example: `.quantize(10)` with readings `23, 24, 26, 28, 29, 30, 33...` always returns `20, 20, 30, 30, 30, 30, 30`

**Using both together:**
```cpp
device.newAnalogSensor(A0)
  .outputRange(0, 100)
  .smoothing(10)         // Reduce electrical noise
  .quantize(25)          // Snap to 0, 25, 50, 75, 100
  .changeThreshold(10)   // Only fire callback when crossing zones
  .onChange(setZone);    // Receives exactly 0, 25, 50, 75, or 100
```

#### Initial Value Behavior

Analog sensors read their initial value on the **first call to `.value()`** or on the **first `device.update()`** call (whichever comes first). The `onChange()` callback **only fires when the value actually changes**, not on the initial read.

```cpp
void setup() {
  byte pot = device.newAnalogSensor(A0)
    .outputRange(0, 255)
    .onChange(handleChange);

  // Get initial value immediately in setup (triggers initial read)
  int initial = device.analogSensor(pot).value();
  device.led(led).setLevel(initial).turnOn();  // LED matches pot at startup
}

void handleChange(int value) {
  // Only fires when pot actually moves
  Serial.println("Pot changed!");
  device.led(led).setLevel(value);
}

void loop() {
  device.update();  // Subsequent calls fire onChange only when value changes
}
```

**Note:** The initial read uses all configured settings (smoothing, mapping, quantization, etc.) and pre-fills the smoothing buffer for consistent behavior.

---

### Rotary Encoders

```cpp
// Create and configure
device.newRotaryEncoder(swPin, dtPin, clkPin)
  .onClockwise(cwCallback)
  .onCounterClockwise(ccwCallback)
  .onPress(pressCallback)      // Button press
  .onRelease(releaseCallback); // Button release
```

**Note:** Assumes standard quadrature encoders (KY-040 style). Swap DT/CLK pins if rotation direction is reversed.

---

### Intervals (Timers)

```cpp
// One-shot timer (runs once after delay)
device.after(1000, callback);

// Repeating timer (runs forever)
device.every(2000, callback);

// Limited repetition (runs N times then stops)
device.repeat(500, 5, callback);  // Run 5 times, 500ms apart

// Pass data to callbacks
IntervalHandle timer = device.after(500, turnOffLED)
  .withMessage(ledHandle);

// Stop a timer
timer.stop();

// Pause and resume a timer
IntervalHandle gameTimer = device.every(100, gameLoop);
gameTimer.pause();   // Temporarily stop execution
gameTimer.resume();  // Continue from where it left off

// Callback signature
void callback(byte message) {
  // message is the value passed via .withMessage()
}

// Example: Blink LED manually
void setup() {
  byte led = device.newLED(13);
  device.led(led).turnOn();
  device.after(1000, turnOffLED).withMessage(led);
}

void turnOffLED(byte ledHandle) {
  device.led(ledHandle).turnOff();
}
```

**Note:** Maximum interval duration is ~49 days (4,294,967,295ms).

---

### Debug Mode

DeviceReactor provides two types of diagnostic output: informational debug messages and fatal error messages.

#### Informational Debug Messages

To enable detailed, non-fatal debug messages (e.g., for state changes or component creation), define the `DEVICE_REACTOR_DEBUG` macro to a serial port object **before** including the library header. This is used for warnings like "ERROR: Too many LEDs" or "ERROR: No interval slot available."

```cpp
// Define DEVICE_REACTOR_DEBUG to your desired serial port
#define DEVICE_REACTOR_DEBUG Serial  // Must be BEFORE #include

#define TOTAL_LEDS 1
#include <DeviceReactor.h>

void setup() {
  // Make sure to initialize the serial port
  Serial.begin(9600);
  
  // DeviceReactor will now print detailed info to Serial
}
```

#### Fatal Error Messages

Critical errors, such as attempting to create too many buttons or analog sensors, or trying to access a component with an invalid handle, are treated as fatal. These messages are always printed directly to the primary `Serial` port (regardless of `DEVICE_REACTOR_DEBUG`) and will halt the microcontroller to prevent further issues.

Example of a fatal error message:
`FATAL ERROR: Invalid LED handle. Halting.`

---

## Understanding Callbacks

A callback is a function you write that DeviceReactor calls automatically when a specific event occurs, like a button press or a sensor value changing. This is the core of the library's event-driven approach.

Your callback functions must have a specific "signature" (the parameters it accepts) to work with the components that call them. There are two types of callbacks in DeviceReactor.

### 1. No-Parameter Callback

This is the simplest callback. It takes no arguments and is used for events that don't need to pass extra data.

**Signature:**
```cpp
void yourFunctionName();
```

**Used By:**
*   `Button`: `onPress()`, `onRelease()`
*   `RotaryEncoder`: `onClockwise()`, `onCounterClockwise()`, `onPress()`, `onRelease()`

**Example:**
```cpp
#define TOTAL_BUTTONS 1
#include <DeviceReactor.h>

Device device;

// Define the callback function.
// It matches the 'void callback()' signature.
void buttonWasPressed() {
  Serial.println("Button event!");
}

void setup() {
  Serial.begin(9600);
  
  // Pass the function name to the component.
  device.newButton(2).onPress(buttonWasPressed);
}

void loop() {
  device.update();
}
```

### 2. Callback with a Single Parameter

Callbacks that receive data about the event use different parameter types depending on the component:

#### `int` Parameter (for AnalogSensor)

**Signature:**
```cpp
void yourFunctionName(int value);
```

**Used By:**
*   `AnalogSensor`: The `value` is the sensor's new reading after mapping and smoothing. Uses `int` to support the full 10-bit ADC range (0-1023) and custom output ranges beyond 0-255.

**Example:**
```cpp
#define TOTAL_ANALOG_SENSORS 1
#define TOTAL_LEDS 1
#include <DeviceReactor.h>

Device device;
byte myLED;

// Define the callback function.
// It matches the 'void callback(int value)' signature.
void sensorValueChanged(int newBrightness) {
  Serial.print("Sensor value changed to: ");
  Serial.println(newBrightness);
  device.led(myLED).setLevel(newBrightness).turnOn();
}

void setup() {
  Serial.begin(9600);
  myLED = device.newLED(9); // PWM pin

  // Pass the function name to the component.
  device.newAnalogSensor(A0)
    .outputRange(0, 255)
    .onChange(sensorValueChanged);
}

void loop() {
  device.update();
}
```

#### `byte` Parameter (for Intervals)

**Signature:**
```cpp
void yourFunctionName(byte value);
```

**Used By:**
*   `Interval` (`after`/`every`/`repeat`): The `value` is the data you optionally passed using `.withMessage()`. If no message is sent, it defaults to `0`.

---

## Examples

### Blink Built-in LED

```cpp
#define TOTAL_LEDS 1
#include <DeviceReactor.h>

Device device;

void setup() {
  byte led = device.newLED(13);
  device.led(led).blink(500);
}

void loop() {
  device.update();
}
```

---

### Button Toggle LED

```cpp
#define TOTAL_LEDS 1
#define TOTAL_BUTTONS 1
#include <DeviceReactor.h>

Device device;
byte led;

void setup() {
  led = device.newLED(13);
  device.newButton(2).onPress(toggle);
}

void toggle() {
  device.led(led).flip();
}

void loop() {
  device.update();
}
```

---

### Photoresistor Night Light (with Inversion)

```cpp
#define TOTAL_LEDS 1
#define TOTAL_ANALOG_SENSORS 1
#include <DeviceReactor.h>

Device device;
byte nightLight;

void setup() {
  Serial.begin(9600);
  nightLight = device.newLED(9);  // PWM pin

  // Photoresistor: resistance goes DOWN as light increases
  // We want LED brightness to go UP as light decreases
  device.newAnalogSensor(A0)
    .inputRange(50, 950)      // Calibrate to actual sensor range
    .outputRange(0, 255)      // Map to LED brightness
    .invert()                 // Invert: dark room = high value
    .smoothing(10)            // Smooth out noise
    .onChange(adjustLight);

  Serial.println("Night light active - LED brightens in darkness");
}

void adjustLight(int brightness) {
  device.led(nightLight).setLevel(brightness).turnOn();
  Serial.print("Light level: ");
  Serial.println(brightness);
}

void loop() {
  device.update();
}
```

---

### Analog Dimmer (using a Potentiometer)

```cpp
#define TOTAL_LEDS 1
#define TOTAL_ANALOG_SENSORS 1
#include <DeviceReactor.h>

Device device;
byte led;

void setup() {
  led = device.newLED(9);  // PWM pin required

  device.newAnalogSensor(A0)
    .outputRange(0, 255)
    .smoothing(10)
    .onChange(dimLED);
}

void dimLED(int brightness) {
  device.led(led).setLevel(brightness).turnOn();
}

void loop() {
  device.update();
}
```

---

### Zone-Based Control (Quantized Sensor)

```cpp
#define TOTAL_LEDS 3
#define TOTAL_ANALOG_SENSORS 1
#include <DeviceReactor.h>

Device device;
byte redLED, yellowLED, greenLED;

void setup() {
  Serial.begin(9600);

  redLED = device.newLED(9);
  yellowLED = device.newLED(10);
  greenLED = device.newLED(11);

  // Potentiometer controls 4 distinct zones
  device.newAnalogSensor(A0)
    .outputRange(0, 100)
    .smoothing(10)           // Smooth out electrical noise
    .quantize(33)            // Snap to exactly 0, 33, 66, 99
    .changeThreshold(20)     // Only fire when crossing zone boundaries
    .onChange(setZone);

  Serial.println("Turn knob to change zones: 0=red, 33=yellow, 66=green, 99=all");
}

void setZone(int zone) {
  // Turn off all LEDs
  device.led(redLED).turnOff();
  device.led(yellowLED).turnOff();
  device.led(greenLED).turnOff();

  Serial.print("Zone changed to: ");
  Serial.println(zone);

  // Activate based on clean quantized value
  if (zone == 0) {
    device.led(redLED).turnOn();
  } else if (zone == 33) {
    device.led(yellowLED).turnOn();
  } else if (zone == 66) {
    device.led(greenLED).turnOn();
  } else if (zone == 99) {
    device.led(redLED).turnOn();
    device.led(yellowLED).turnOn();
    device.led(greenLED).turnOn();
  }
}

void loop() {
  device.update();
}
```

---

### Smooth LED Fade Effects

```cpp
#define TOTAL_LEDS 1
#define TOTAL_BUTTONS 1
#include <DeviceReactor.h>

Device device;
byte statusLED;

void setup() {
  statusLED = device.newLED(9);  // PWM pin required

  // Button cycles through fade effects
  device.newButton(2).onPress(nextEffect);

  // Start with fade in
  device.led(statusLED).fadeIn(2000);
}

void nextEffect() {
  static byte effectMode = 0;
  effectMode++;
  if (effectMode > 2) effectMode = 0;

  switch (effectMode) {
    case 0:
      device.led(statusLED).fadeIn(2000);
      break;
    case 1:
      device.led(statusLED).fadeOut(2000);
      break;
    case 2:
      device.led(statusLED).pulse(2000, 3, 0, 255);  // Pulse 3 times
      break;
  }
}

void loop() {
  device.update();
}
```

---

### RGB Color Cycling

```cpp
#define TOTAL_LEDS 1
#define TOTAL_INTERVALS 1
#include <DeviceReactor.h>

Device device;
byte rgbLED;
byte hue = 0;

void setup() {
  rgbLED = device.newLED(3, 5, 6);  // R, G, B on PWM pins
  device.every(50, cycleColor);
}

void cycleColor(byte msg) {
  hue += 5;
  device.led(rgbLED).setColor(hue, 255 - hue, 128).turnOn();
}

void loop() {
  device.update();
}
```

---

### Multiple Components

```cpp
#define TOTAL_LEDS 2
#define TOTAL_BUTTONS 1
#define TOTAL_ANALOG_SENSORS 1
#define TOTAL_INTERVALS 2
#include <DeviceReactor.h>

Device device;

byte statusLED, powerLED;

void setup() {
  // LEDs
  statusLED = device.newLED(13);
  powerLED = device.newLED(9);

  // Button
  device.newButton(2)
    .onPress(buttonPressed);

  // Analog Sensor (using a Potentiometer)
  device.newAnalogSensor(A0)
    .outputRange(0, 255)
    .onChange(adjustPower);

  // Timers
  device.led(statusLED).blink(1000);  // Status blinks
  device.every(5000, heartbeat);      // Heartbeat message
}

void buttonPressed() {
  device.led(powerLED).flip();
}

void adjustPower(int level) {
  device.led(powerLED).setLevel(level);
}

void heartbeat(byte msg) {
  Serial.println("System running...");
}

void loop() {
  device.update();
}
```

---

### Simple Game with Pause (Interval Pause/Resume)

```cpp
#define TOTAL_LEDS 1
#define TOTAL_BUTTONS 1
#define TOTAL_INTERVALS 1
#include <DeviceReactor.h>

Device device;
byte scoreLED;
IntervalHandle gameTimer;
bool isPaused = false;
byte score = 0;

void setup() {
  Serial.begin(9600);
  scoreLED = device.newLED(9);  // PWM pin

  // Button toggles pause state
  device.newButton(2).onPress(togglePause);

  // Game loop runs every 500ms
  gameTimer = device.every(500, gameLoop);
}

void togglePause() {
  isPaused = !isPaused;

  if (isPaused) {
    gameTimer.pause();
    Serial.println("Game Paused");
    device.led(scoreLED).blink(200);  // Visual feedback
  } else {
    gameTimer.resume();
    Serial.println("Game Resumed");
    device.led(scoreLED).setLevel(score).turnOn();
  }
}

void gameLoop(byte msg) {
  score++;
  if (score > 255) score = 0;

  device.led(scoreLED).setLevel(score).turnOn();
  Serial.print("Score: ");
  Serial.println(score);
}

void loop() {
  device.update();
}
```

---

## How It Works

### Handles vs. Names

DeviceReactor uses **handles** (byte indices) instead of string names:

```cpp
// Component creation returns a handle
byte led = device.newLED(9);  // Returns 0 (first LED)
byte btn = device.newButton(2);  // Returns 0 (first button)

// Access components by handle
device.led(led).turnOn();    // Fast array lookup: LEDs[0]
device.button(btn).onPress(cb);  // Fast array lookup: buttons[0]
```

### Fluent API Pattern

Methods return `*this` to enable chaining:

```cpp
// Traditional style
AnalogSensor& mySensor = device.newAnalogSensor(A0);
mySensor.outputRange(0, 100);
mySensor.smoothing(10);
mySensor.onChange(callback);

// Fluent style (same result)
device.newAnalogSensor(A0)
  .outputRange(0, 100)
  .smoothing(10)
  .onChange(callback);
```

### Zero-Cost Abstractions

Unused components consume zero memory:

```cpp
// Only define what you need
#define TOTAL_LEDS 2
// TOTAL_BUTTONS not defined = defaults to 0

// Button code is completely removed at compile time
// No arrays allocated, no code generated
```

---

## Advanced Usage

### Direct Component Access

```cpp
// Store handles for frequent access
byte led1 = device.newLED(9);
byte led2 = device.newLED(10);

// Access component properties directly (read-only)
if (device.led(led1).state == HIGH) {  // state: LOW or HIGH
  device.led(led2).turnOn();
}

// RGB LEDs expose R, G, B values (note: inverted if commonAnode=true)
byte red = device.led(rgbLED).R;

// Check LED capabilities
if (device.led(led1).isRGB) { /* ... */ }
if (device.led(led1).isDimmable) { /* ... */ }
```

**Warning:** Do not modify component properties (`pin`, `commonAnode`, `state`, etc.) after initialization.

### Custom Debounce Delay

```cpp
#define DEBOUNCE_DELAY 100  // milliseconds (default: 50)
#define TOTAL_BUTTONS 1

#include <DeviceReactor.h>
```

### RGB Common Cathode

```cpp
byte rgb = device.newLED(3, 5, 6);
device.led(rgb).commonAnode = false;  // Set to common cathode
device.led(rgb).setColor(255, 0, 0).turnOn();
```

### Interval Handle Storage

```cpp
IntervalHandle timer1;
IntervalHandle timer2;

void setup() {
  timer1 = device.every(1000, callback1);
  timer2 = device.every(2000, callback2);
}

void someFunction() {
  timer1.stop();  // Stop first timer
  timer2.stop();  // Stop second timer
}
```
---

## Troubleshooting

### "FATAL ERROR: Invalid handle. Halting."

**Cause:** Using an invalid handle (255 or out of range). This is a **fatal error** - the device halts permanently and must be reset.

**Fix:**
```cpp
// Check for invalid handle
byte led = device.newLED(9);
if (led != INVALID_HANDLE) {
  device.led(led).turnOn();
}
```

### "FATAL ERROR: Too many LEDs. Increase TOTAL_LEDS. Halting."

**Cause:** Created more components than defined in `TOTAL_LEDS`. This is a **fatal error** - the device halts permanently in `setup()` and must be reset.

**Fix:**
```cpp
// Increase the limit before including the library
#define TOTAL_LEDS 10  // Was 5, now 10
#include <DeviceReactor.h>
```

### LED not dimming

**Cause:** Pin doesn't support PWM

**Fix:**
```cpp
// Use PWM-capable pins: 3, 5, 6, 9, 10, 11 (on Uno)
byte led = device.newLED(9);  // ✓ PWM pin
// Not pin 13, 12, 8, etc. (digital only)
```

### Analog Sensor values erratic

**Cause:** Electrical noise, no smoothing

**Fix:**
```cpp
device.newAnalogSensor(A0)
  .smoothing(20)  // Increase smoothing samples
  .changeThreshold(10);  // Increase threshold
```

### Button triggering multiple times

**Cause:** Electrical bounce (already handled by library)

**Check:**
```cpp
// Increase debounce delay if needed
#define DEBOUNCE_DELAY 100  // Default is 50ms
```

---

## Exposed Macros

This library exposes several macros for configuration and use as constants.

### Configuration Macros

These macros should be defined **before** you `#include <DeviceReactor.h>` to configure the library's memory allocation and behavior.

| Macro | Default | Description |
|---|---|---|
| `TOTAL_LEDS` | `0` | Maximum number of LEDs you will create. |
| `TOTAL_BUTTONS` | `0` | Maximum number of buttons you will create. |
| `TOTAL_ANALOG_SENSORS` | `0` | Maximum number of analog sensors you will create. |
| `TOTAL_ROTARY_ENCODERS` | `0` | Maximum number of rotary encoders you will create. |
| `TOTAL_INTERVALS` | `0` | Maximum number of timers (`after`/`every`/`repeat`) you will create. |
| `DEBOUNCE_DELAY` | `50` | Sets the debounce delay in milliseconds for all buttons. |
| `ENCODER_DEBOUNCE_DELAY` | `5` | Sets the debounce delay in milliseconds for rotary encoder rotation events. |
| `DEVICE_REACTOR_DEBUG` | (undefined) | Define this to a serial port (e.g., `Serial`) to enable informational debug output. |

### Constants

These constants are defined by the library for use as arguments in component methods.

#### `INVALID_HANDLE`

Failed component creation returns `INVALID_HANDLE` (255). Check for this value to detect errors:

```cpp
byte led = device.newLED(9);
if (led == INVALID_HANDLE) {
  // Component creation failed (too many components)
}
```

#### Button Modes

Used with `device.newButton(pin, mode)`:

| Constant | Description |
|---|---|
| `BUTTON_INPUT_PULLUP` | Default mode. Uses internal pull-up resistor. Pressing connects the pin to GND. |
| `BUTTON_PRESS_LOW` | For use with an external pull-up resistor. Pressing connects the pin to GND. |
| `BUTTON_PRESS_HIGH` | For use with an external pull-down resistor. Pressing connects the pin to VCC. |

---

## Supported Boards

DeviceReactor works on any Arduino-compatible board:

- ✅ Arduino Uno, Nano, Mega
- ✅ Arduino Leonardo, Pro Mini
- ✅ ESP8266, ESP32 (in Arduino mode)
- ✅ Teensy
- ✅ STM32 (Arduino core)

**Requirements:** Arduino.h compatible environment

---

## Contributing

Issues and pull requests welcome at [GitHub repository URL]

---

## License

MIT License

Copyright (c) 2025 Jonathan Wyett

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## Author

**Jonathan Wyett**

Version 1.0.0 (2025)
