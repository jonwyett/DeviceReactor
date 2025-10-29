/******************************************************************************
  DeviceReactor
  Author: Jonathan Wyett
  Version: 1.0.0
  Date: 2025-01-27

  Modern event-driven hardware framework for Arduino with type-safe handles
  and fluent API design.
******************************************************************************/

#ifndef DEVICE_REACTOR_H
#define DEVICE_REACTOR_H

#include <Arduino.h>

/****** COMPONENT CONFIGURATION WITH DEFAULTS ********************************/
// Users can define these before including the library
// If not defined, defaults to 0 (component type disabled, zero memory used)

#ifndef TOTAL_LEDS
  #define TOTAL_LEDS 0
#endif

#ifndef TOTAL_BUTTONS
  #define TOTAL_BUTTONS 0
#endif

#ifndef TOTAL_ANALOG_SENSORS
  #define TOTAL_ANALOG_SENSORS 0
#endif

#ifndef TOTAL_ROTARY_ENCODERS
  #define TOTAL_ROTARY_ENCODERS 0
#endif

#ifndef TOTAL_INTERVALS
  #define TOTAL_INTERVALS 0
#endif

#ifndef DEBOUNCE_DELAY
  #define DEBOUNCE_DELAY 50
#endif

#ifndef ENCODER_DEBOUNCE_DELAY
  #define ENCODER_DEBOUNCE_DELAY 5
#endif

#ifndef MAX_ZONES_PER_SENSOR
  #define MAX_ZONES_PER_SENSOR 0
#endif

/****** END CONFIGURATION ****************************************************/

// Invalid handle constant
#define INVALID_HANDLE 255

/****** DEBUG MACROS ********************************************************/
// Usage: #define DEVICE_REACTOR_DEBUG Serial before including this library
#ifdef DEVICE_REACTOR_DEBUG
  #define DR_DEBUG_PRINT(x)   DEVICE_REACTOR_DEBUG.print(x)
  #define DR_DEBUG_PRINTLN(x) DEVICE_REACTOR_DEBUG.println(x)
#else
  #define DR_DEBUG_PRINT(x)
  #define DR_DEBUG_PRINTLN(x)
#endif
/****** END DEBUG MACROS ****************************************************/

// Callback type definitions
typedef void (*basicCallback)();
typedef void (*byteParamCallback)(byte);
typedef void (*intParamCallback)(int);

/****** BUTTON MODES *********************************************************/
#define BUTTON_PRESS_HIGH 0
#define BUTTON_PRESS_LOW 1
#define BUTTON_INPUT_PULLUP 2

/*****************************************************************************
 * INTERVAL CLASS
 *****************************************************************************/
#if TOTAL_INTERVALS > 0
  class Device;  // Forward declaration

  class IntervalHandle {
    public:
      IntervalHandle(Device* dev, byte idx) : device(dev), index(idx) {}

      IntervalHandle& withMessage(byte msg);
      void stop();
      void pause();
      void resume();

    private:
      Device* device;
      byte index;
      friend class Device;
  };

  class Interval {
    public:
      Interval() {
        // Initialize all slots to inactive and unpaused
        for (byte i = 0; i < TOTAL_INTERVALS; i++) {
          counts[i] = -1;
          paused[i] = false;
        }
      }

      byte add(byteParamCallback callback, unsigned long wait, int count, byte msg) {
        // Find an available slot for reuse
        byte slot = findSlot();

        if (slot >= TOTAL_INTERVALS) {
          Serial.println("FATAL ERROR: All interval slots used. Increase TOTAL_INTERVALS. Halting.");
          while(1);
        }

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("New interval on slot ");
          DR_DEBUG_PRINT(slot);
          DR_DEBUG_PRINT(" with msg=");
          DR_DEBUG_PRINTLN(msg);
        #endif

        // Fully initialize the slot to prevent stale data issues
        callbacks[slot] = callback;
        waits[slot] = wait;
        counts[slot] = count;
        msgs[slot] = msg;
        lastRuns[slot] = millis();
        paused[slot] = false;  // Ensure slot is not paused
        return slot;
      }

      void clear(byte index) {
        if (index < TOTAL_INTERVALS) {
          // Mark as inactive (stops it from running and makes slot available for reuse)
          // Note: We clear the callback pointer first to prevent race conditions if
          // clear() is called from within a callback during update() iteration
          callbacks[index] = nullptr;  // Clear callback to prevent stale function pointers
          counts[index] = -1;
          paused[index] = false;  // Reset paused state for slot reuse
          waits[index] = 0;
          msgs[index] = 0;
          lastRuns[index] = 0;
        }
      }

      void setMessage(byte index, byte msg) {
        // Bounds check and verify slot is active
        if (index >= TOTAL_INTERVALS) {
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINT("ERROR: Invalid interval index ");
            DR_DEBUG_PRINTLN(index);
          #endif
          return;
        }
        if (counts[index] >= 0) {
          msgs[index] = msg;
        }
      }

      void pause(byte index) {
        // Bounds check and verify slot is active
        if (index >= TOTAL_INTERVALS) {
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINT("ERROR: Invalid interval index ");
            DR_DEBUG_PRINTLN(index);
          #endif
          return;
        }
        if (counts[index] >= 0) {
          paused[index] = true;
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINT("Interval ");
            DR_DEBUG_PRINT(index);
            DR_DEBUG_PRINTLN(" paused");
          #endif
        }
      }

      void resume(byte index) {
        // Bounds check and verify slot is active
        if (index >= TOTAL_INTERVALS) {
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINT("ERROR: Invalid interval index ");
            DR_DEBUG_PRINTLN(index);
          #endif
          return;
        }
        if (counts[index] >= 0) {
          paused[index] = false;
          // Reset the timer to prevent immediate firing after resume
          lastRuns[index] = millis();
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINT("Interval ");
            DR_DEBUG_PRINT(index);
            DR_DEBUG_PRINTLN(" resumed");
          #endif
        }
      }

      void update() {
        for (byte i = 0; i < TOTAL_INTERVALS; i++) {
          // Only process active intervals (-1 is inactive) and skip paused intervals
          // Check callback is not null to prevent race conditions with clear()
          if (counts[i] >= 0 && !paused[i] && callbacks[i] != nullptr) {
            // Check if the time since the last run is >= the wait
            // Note: (millis() - lastRuns[i]) handles millis() rollover correctly
            if ((millis() - lastRuns[i]) >= waits[i]) {
              // Store callback locally in case clear() is called during execution
              byteParamCallback localCallback = callbacks[i];
              byte localMsg = msgs[i];

              // Decrement count AFTER callback to prevent double execution
              localCallback(localMsg); // Run the callback function
              lastRuns[i] = millis(); // Update the last run

              // Now update count state after callback (check callback still valid)
              if (counts[i] > 0 && callbacks[i] != nullptr) {
                counts[i]--; // Decrement the count
                // Check if the count has reached 0
                if (counts[i] == 0) {
                  counts[i] = -1; // Mark as inactive after final execution
                }
              }
            }
          }
        }
      }

      #ifdef DEVICE_REACTOR_DEBUG
        void printStatus() {
          DR_DEBUG_PRINTLN("INTERVAL DEBUG:");
          for (byte i = 0; i < TOTAL_INTERVALS; i++) {
            if (counts[i] >= 0) {
              DR_DEBUG_PRINT("(");
              DR_DEBUG_PRINT(i);
              DR_DEBUG_PRINT(") Count:");
              DR_DEBUG_PRINT(counts[i]);
              DR_DEBUG_PRINT(" Wait:");
              DR_DEBUG_PRINT(waits[i]);
              DR_DEBUG_PRINT(" Msg:");
              DR_DEBUG_PRINTLN(msgs[i]);
            }
          }
          DR_DEBUG_PRINTLN("-------------------------");
        }
      #endif

    private:
      byteParamCallback callbacks[TOTAL_INTERVALS];
      int counts[TOTAL_INTERVALS];  // -1 = inactive, 0 = infinite, >0 = remaining
      unsigned long waits[TOTAL_INTERVALS];
      unsigned long lastRuns[TOTAL_INTERVALS];
      byte msgs[TOTAL_INTERVALS];
      bool paused[TOTAL_INTERVALS];

      // Find an empty interval slot
      byte findSlot() {
        for (byte i = 0; i < TOTAL_INTERVALS; i++) {
          if (counts[i] == -1) {
            return i;
          }
        }
        // No empty slot found
        return INVALID_HANDLE;
      }
  };
#endif

/*****************************************************************************
 * ANALOG SENSOR CLASS
 *****************************************************************************/
#if TOTAL_ANALOG_SENSORS > 0
  class AnalogSensor {
    public:
      // Sprint 5: Preset enum for common configurations
      enum Preset {
        RAW_DATA,              // No processing, raw 0-1023 values
        POT_FOR_LED,           // Smooth, continuous 0-255 for analogWrite
        POT_FOR_SERVO,         // Smooth, continuous 0-180 for servo control
        POT_FOR_PERCENTAGE,    // Stable 0-100 in steps of 5
        SWITCH_5_POSITION      // Heavy 5-position switch (0-4)
      };

      // Sprint 5: Preset configuration structure
      struct PresetConfig {
        byte smoothing_samples;
        int output_min;
        int output_max;
        int quantize_step;      // Q value
        int hysteresis_amount;  // H value
        int change_threshold;   // T value for non-quantized mode
      };

      // Zone structure for zone-based state management
      struct Zone {
        byte id;
        int min_val;
        int max_val;
      };

      // Sprint 5: Static preset configurations array
      static const PresetConfig preset_configs[];

      byte pin;

      void init(byte newPin) {
        if (initialized) {
          Serial.println("WARNING: AnalogSensor already initialized. Ignoring.");
          return;
        }
        pin = newPin;
        initialized = true;
        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Setup Analog Sensor on pin ");
          DR_DEBUG_PRINTLN(pin);
        #endif
      }

      AnalogSensor& inputRange(int newMin, int newMax) {
        inputMin = newMin;
        inputMax = newMax;
        return *this;
      }

      AnalogSensor& outputRange(int newMin, int newMax) {
        outputMin = newMin;
        outputMax = newMax;
        return *this;
      }

      AnalogSensor& invert() {
        inverted = true;
        return *this;
      }

      // Sprint 5: Overloaded quantize() methods
      AnalogSensor& quantize(int step) {
        quantizeStep = step;
        // Automatically calculate H = Q / 4 for balanced feel
        hysteresis_amount = step / 4;
        return *this;
      }

      AnalogSensor& quantize(int step, int hysteresis) {
        quantizeStep = step;
        hysteresis_amount = hysteresis;
        return *this;
      }

      AnalogSensor& changeThreshold(int threshold) {
        delta = threshold;
        return *this;
      }

      AnalogSensor& smoothing(byte samples) {
        avgSamples = samples < 1 ? 1 : samples;
        return *this;
      }

      AnalogSensor& onChange(intParamCallback callback) {
        hasChangeFunc = true;
        changed = callback;
        return *this;
      }

      // Sprint 2: Zone configuration methods
      AnalogSensor& addZone(byte id, int min_value, int max_value) {
        if (zone_count >= MAX_ZONES_PER_SENSOR) {
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINTLN("ERROR: Maximum zones reached for this sensor");
          #endif
          return *this;
        }

        // Validate zone boundaries
        if (min_value > max_value) {
          #ifdef DEVICE_REACTOR_DEBUG
            DR_DEBUG_PRINTLN("ERROR: Zone min_value must be <= max_value");
          #endif
          return *this;
        }

        defined_zones[zone_count].id = id;
        defined_zones[zone_count].min_val = min_value;
        defined_zones[zone_count].max_val = max_value;
        zone_count++;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Defined zone ID ");
          DR_DEBUG_PRINT(id);
          DR_DEBUG_PRINT(" [");
          DR_DEBUG_PRINT(min_value);
          DR_DEBUG_PRINT(", ");
          DR_DEBUG_PRINT(max_value);
          DR_DEBUG_PRINTLN("]");
        #endif

        return *this;
      }

      AnalogSensor& clearZones() {
        zone_count = 0;
        currentZoneID = INVALID_HANDLE;
        previousZoneID = INVALID_HANDLE;
        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINTLN("Cleared all zones");
        #endif
        return *this;
      }

      // Sprint 5: High-level preset configuration
      AnalogSensor& configure(Preset preset) {
        // Cast enum to index for array lookup
        int index = static_cast<int>(preset);
        const PresetConfig& config = preset_configs[index];

        // Apply common settings
        smoothing(config.smoothing_samples);
        outputRange(config.output_min, config.output_max);

        // Apply stability mode based on recipe
        if (config.quantize_step > 0) {
          // Quantization mode enabled
          // If H is 0 in table, use auto-calculated default (Q/4)
          int h = (config.hysteresis_amount > 0)
                    ? config.hysteresis_amount
                    : config.quantize_step / 4;
          quantize(config.quantize_step, h);
        } else {
          // Continuous mode with change threshold
          changeThreshold(config.change_threshold);
        }

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Applied preset configuration: ");
          DR_DEBUG_PRINTLN(index);
        #endif

        return *this;
      }

      // Sprint 4: Zone change event callback
      AnalogSensor& onZoneChange(byteParamCallback callback) {
        hasZoneChangeFunc = true;
        zoneChanged = callback;
        return *this;
      }

      int value() {
        // Perform initial read on first call with configured settings
        if (!hasInitialRead) {
          performInitialRead();
        }
        return currentValue;
      }

      void update() {
        // Perform initial read if not done yet (in case update() called before value())
        if (!hasInitialRead) {
          performInitialRead();
          return;  // Don't fire onChange on initial read
        }

        // Read raw ADC value
        int rawValue = analogRead(pin);

        // Apply smoothing
        if (avgSamples > 1) {
          average(rawValue);
        } else {
          accumulatedSum = rawValue;
          avgCount = 1;  // Single sample, ready to process
        }

        // Only process when averaging is complete (avgCount == avgSamples)
        if (avgCount >= avgSamples) {
          // Calculate the average value from accumulated sum
          int avgValue = accumulatedSum / avgSamples;

          // Clamp to INPUT range first
          if (avgValue < inputMin) avgValue = inputMin;
          if (avgValue > inputMax) avgValue = inputMax;

          // Map from input range to output range
          hiResValue = map(avgValue, inputMin, inputMax, outputMin, outputMax);

          // Clamp to OUTPUT range
          if (hiResValue < outputMin) hiResValue = outputMin;
          if (hiResValue > outputMax) hiResValue = outputMax;

          // Apply inversion if enabled
          if (inverted) {
            hiResValue = outputMax + outputMin - hiResValue;
          }

          // Sprint 3: Mode selection logic for stability
          bool valueChanged = false;
          int newValue = hiResValue;

          if (quantizeStep > 0) {
            // Mode 1: Quantized Hysteresis (Q+H)
            // Calculate the potential quantized value using proper rounding
            int V_potential = round((float)hiResValue / quantizeStep) * quantizeStep;

            // Clamp V_potential to output range
            if (V_potential < outputMin) V_potential = outputMin;
            if (V_potential > outputMax) V_potential = outputMax;

            // Check if we've crossed a quantization boundary
            if (V_potential != currentReportedValue) {
              if (V_potential > currentReportedValue) {
                // Value is increasing - check upper trigger
                int trigger_up = (currentReportedValue + quantizeStep / 2) + hysteresis_amount;
                if (hiResValue >= trigger_up) {
                  newValue = V_potential;
                  valueChanged = true;
                }
              } else {
                // Value is decreasing - check lower trigger
                int trigger_down = (currentReportedValue - quantizeStep / 2) - hysteresis_amount;
                if (hiResValue <= trigger_down) {
                  newValue = V_potential;
                  valueChanged = true;
                }
              }
            }
          } else if (delta > 1) {
            // Mode 2: Change Threshold (Floating Anchor)
            if (abs(hiResValue - currentReportedValue) >= delta) {
              newValue = hiResValue;
              valueChanged = true;
            }
          } else {
            // Mode 3: Raw Mode (delta == 1, no quantization)
            if (hiResValue != currentReportedValue) {
              newValue = hiResValue;
              valueChanged = true;
            }
          }

          // Update state and fire onChange if value changed
          if (valueChanged) {
            currentReportedValue = newValue;
            currentValue = newValue;  // Keep currentValue in sync for backward compatibility
            previousValue = newValue; // Keep previousValue in sync

            #ifdef DEVICE_REACTOR_DEBUG
              DR_DEBUG_PRINT("Analog sensor value changed to ");
              DR_DEBUG_PRINTLN(currentReportedValue);
            #endif

            if (hasChangeFunc) {
              changed(currentReportedValue);
            }
          }

          // Sprint 4: Zone detection and event firing
          if (zone_count > 0) {
            // Find which zone the currentReportedValue falls into
            byte detectedZoneID = findZoneForValue(currentReportedValue);

            // Update currentZoneID
            currentZoneID = detectedZoneID;

            // Fire onZoneChange if zone changed
            if (currentZoneID != previousZoneID) {
              previousZoneID = currentZoneID;

              #ifdef DEVICE_REACTOR_DEBUG
                DR_DEBUG_PRINT("Zone changed to ");
                DR_DEBUG_PRINTLN(currentZoneID);
              #endif

              if (hasZoneChangeFunc) {
                zoneChanged(currentZoneID);
              }
            }
          }

          // Reset accumulator and counter for next cycle
          accumulatedSum = 0;
          avgCount = 0;
        }
      }

    private:
      bool initialized = false;
      bool hasInitialRead = false;

      // Input range (raw ADC values)
      int inputMin = 0;
      int inputMax = 1023;

      // Output range (mapped values)
      int outputMin = 0;
      int outputMax = 1023;
      bool inverted = false;
      int quantizeStep = 0;  // 0 = disabled, >0 = snap to grid

      // Change detection
      int delta = 1;
      int currentValue = 0;
      int previousValue = 0;

      // Smoothing
      byte avgSamples = 1;
      byte avgCount = 0;
      unsigned long accumulatedSum = 0;  // Use unsigned long to prevent overflow

      // Callback
      bool hasChangeFunc = false;
      intParamCallback changed;

      // Sprint 1: New state variables for Quantized Hysteresis and Zones
      int hiResValue = 0;              // Value after smoothing, clamping, and mapping
      int currentReportedValue = 0;    // Final stable value exposed to the user
      int hysteresis_amount = 0;        // The 'H' value for Quantized Hysteresis

      // Zone management
      Zone defined_zones[MAX_ZONES_PER_SENSOR];
      byte zone_count = 0;
      byte currentZoneID = INVALID_HANDLE;
      byte previousZoneID = INVALID_HANDLE;

      // Sprint 4: Zone change callback
      bool hasZoneChangeFunc = false;
      byteParamCallback zoneChanged;

      void average(int newReading) {
        accumulatedSum += newReading;
        avgCount++;
        // avgCount will be checked in update() to determine if ready to process
      }

      // Sprint 2: Helper method to find zone for a given value
      byte findZoneForValue(int val) {
        for (byte i = 0; i < zone_count; i++) {
          if (val >= defined_zones[i].min_val && val <= defined_zones[i].max_val) {
            return defined_zones[i].id;
          }
        }
        return INVALID_HANDLE;  // No zone found
      }

      void performInitialRead() {
        // Read raw ADC value with all configured settings applied
        int rawValue = analogRead(pin);

        // Pre-fill smoothing buffer with first reading for consistent behavior
        if (avgSamples > 1) {
          accumulatedSum = (unsigned long)rawValue * avgSamples;
          avgCount = avgSamples;
        } else {
          accumulatedSum = rawValue;
          avgCount = 1;
        }

        // Calculate average (same as update logic)
        int avgValue = accumulatedSum / avgSamples;

        // Clamp to INPUT range
        if (avgValue < inputMin) avgValue = inputMin;
        if (avgValue > inputMax) avgValue = inputMax;

        // Map from input range to output range
        hiResValue = map(avgValue, inputMin, inputMax, outputMin, outputMax);

        // Clamp to OUTPUT range
        if (hiResValue < outputMin) hiResValue = outputMin;
        if (hiResValue > outputMax) hiResValue = outputMax;

        // Apply inversion if enabled
        if (inverted) {
          hiResValue = outputMax + outputMin - hiResValue;
        }

        // Sprint 3: Initialize currentReportedValue based on mode
        if (quantizeStep > 0) {
          // Quantize to nearest grid point using proper rounding
          currentReportedValue = round((float)hiResValue / quantizeStep) * quantizeStep;
          // Clamp to output range
          if (currentReportedValue < outputMin) currentReportedValue = outputMin;
          if (currentReportedValue > outputMax) currentReportedValue = outputMax;
        } else {
          // For Change Threshold or Raw mode, use hiResValue directly
          currentReportedValue = hiResValue;
        }

        // Keep legacy variables in sync for backward compatibility
        currentValue = currentReportedValue;
        previousValue = currentReportedValue;

        // Initialize zone state to prevent onZoneChange firing on first update
        if (zone_count > 0) {
          byte initialZone = findZoneForValue(currentReportedValue);
          currentZoneID = initialZone;
          previousZoneID = initialZone;
        }

        // Reset accumulator for next update cycle
        accumulatedSum = 0;
        avgCount = 0;

        hasInitialRead = true;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Initial analog sensor value: ");
          DR_DEBUG_PRINTLN(currentReportedValue);
        #endif
      }
  };

  // Sprint 5: Define preset configuration data
  // This array holds the configuration data for each preset.
  // The order MUST EXACTLY MATCH the order of the Preset enum.
  const AnalogSensor::PresetConfig AnalogSensor::preset_configs[] = {
    // RAW_DATA: No processing, raw 0-1023 values
    { /*smoothing*/ 1, /*out_min*/ 0, /*out_max*/ 1023, /*Q*/ 0, /*H*/ 0, /*T*/ 1 },

    // POT_FOR_LED: Smooth, continuous 0-255 for analogWrite
    { /*smoothing*/ 8, /*out_min*/ 0, /*out_max*/ 255,  /*Q*/ 0, /*H*/ 0, /*T*/ 2 },

    // POT_FOR_SERVO: Smooth, continuous 0-180 for servo control
    { /*smoothing*/ 8, /*out_min*/ 0, /*out_max*/ 180,  /*Q*/ 0, /*H*/ 0, /*T*/ 2 },

    // POT_FOR_PERCENTAGE: Stable 0-100 in steps of 5 (T is irrelevant, Q > 0)
    { /*smoothing*/ 10, /*out_min*/ 0, /*out_max*/ 100, /*Q*/ 5, /*H*/ 1, /*T*/ 1 },

    // SWITCH_5_POSITION: Heavy 5-position switch with strong hysteresis
    { /*smoothing*/ 12, /*out_min*/ 0, /*out_max*/ 4,   /*Q*/ 1, /*H*/ 1, /*T*/ 1 }
  };
#endif

/*****************************************************************************
 * BUTTON CLASS
 *****************************************************************************/
#if TOTAL_BUTTONS > 0 || TOTAL_ROTARY_ENCODERS > 0
  class Button {
    public:
      byte pin;
      byte pressMode = BUTTON_INPUT_PULLUP;

      void init(byte newPin, byte newMode = BUTTON_INPUT_PULLUP) {
        if (initialized) {
          Serial.println("WARNING: Button already initialized. Ignoring.");
          return;
        }
        pin = newPin;
        pressMode = newMode;

        if (pressMode == BUTTON_INPUT_PULLUP) {
          pinMode(pin, INPUT_PULLUP);
        } else {
          pinMode(pin, INPUT);
        }

        // Read the actual pin state to avoid missing first state change
        state = digitalRead(pin);
        oldState = state;
        initialized = true;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Setup button on pin ");
          DR_DEBUG_PRINT(pin);
          DR_DEBUG_PRINT(" initial state: ");
          DR_DEBUG_PRINTLN(state);
        #endif
      }

      Button& onPress(basicCallback callback) {
        hasPressFunc = true;
        pressed = callback;
        return *this;
      }

      Button& onRelease(basicCallback callback) {
        hasReleaseFunc = true;
        released = callback;
        return *this;
      }

      void update() {
        checkForPress();
      }

    protected:
      bool initialized = false;
      byte state = HIGH;
      byte oldState = HIGH;
      bool hasPressFunc = false;
      bool hasReleaseFunc = false;
      basicCallback pressed;
      basicCallback released;
      unsigned long lastDebounceTime = 0;

      void checkForPress() {
        // Get state
        state = digitalRead(pin);

        if (state != oldState) {
          // Note: (millis() - lastDebounceTime) handles millis() rollover correctly
          if ((millis() - lastDebounceTime) >= DEBOUNCE_DELAY) {
            oldState = state;
            lastDebounceTime = millis();

            #ifdef DEVICE_REACTOR_DEBUG
              DR_DEBUG_PRINT("Button state changed to ");
              DR_DEBUG_PRINTLN(state);
            #endif

            bool isPressed = false;
            if (pressMode == BUTTON_PRESS_HIGH) {
              isPressed = (state == HIGH);
            } else {
              isPressed = (state == LOW);
            }

            if (isPressed && hasPressFunc) {
              pressed();
            } else if (!isPressed && hasReleaseFunc) {
              released();
            }
          } else {
            #ifdef DEVICE_REACTOR_DEBUG
              DR_DEBUG_PRINTLN("BUTTON DEBOUNCE PROTECTION");
            #endif
          }
        }
      }
  };
#endif

/*****************************************************************************
 * ROTARY ENCODER CLASS
 *****************************************************************************/
#if TOTAL_ROTARY_ENCODERS > 0
  class RotaryEncoder : public Button {
    public:
      byte DT, CLK;

      void init(byte swPin, byte dtPin, byte clkPin) {
        if (initialized) {
          Serial.println("WARNING: RotaryEncoder already initialized. Ignoring.");
          return;
        }
        pin = swPin;
        DT = dtPin;
        CLK = clkPin;

        pinMode(pin, INPUT_PULLUP);
        pinMode(DT, INPUT_PULLUP);
        pinMode(CLK, INPUT_PULLUP);

        // Initialize button state to match actual pin state
        state = digitalRead(pin);
        oldState = state;

        lastStateCLK = digitalRead(CLK);
        initialized = true;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Setup Rotary Encoder SW:");
          DR_DEBUG_PRINT(pin);
          DR_DEBUG_PRINT(" DT:");
          DR_DEBUG_PRINT(DT);
          DR_DEBUG_PRINT(" CLK:");
          DR_DEBUG_PRINT(CLK);
          DR_DEBUG_PRINT(" initial button state: ");
          DR_DEBUG_PRINTLN(state);
        #endif
      }

      RotaryEncoder& onClockwise(basicCallback callback) {
        hasCWFunc = true;
        CW = callback;
        return *this;
      }

      RotaryEncoder& onCounterClockwise(basicCallback callback) {
        hasCCWFunc = true;
        CCW = callback;
        return *this;
      }

      void update() {
        checkForPress();

        currentStateCLK = digitalRead(CLK);

        // If last and current state of CLK are different, then pulse occurred
        // React to only 1 state change to avoid double count
        if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
          // Apply debouncing to rotation events (shorter delay than button press)
          // Note: (millis() - lastRotationTime) handles millis() rollover correctly
          if ((millis() - lastRotationTime) >= ENCODER_DEBOUNCE_DELAY) {
            lastRotationTime = millis();

            // If the DT state is different than the CLK state then
            // the encoder is rotating CCW
            if (digitalRead(DT) != currentStateCLK) {
              if (hasCCWFunc) {
                CCW();
              }
              #ifdef DEVICE_REACTOR_DEBUG
                DR_DEBUG_PRINTLN("Encoder CCW");
              #endif
            } else {  // Encoder is rotating CW
              if (hasCWFunc) {
                CW();
              }
              #ifdef DEVICE_REACTOR_DEBUG
                DR_DEBUG_PRINTLN("Encoder CW");
              #endif
            }
          } else {
            #ifdef DEVICE_REACTOR_DEBUG
              DR_DEBUG_PRINTLN("ENCODER ROTATION DEBOUNCE PROTECTION");
            #endif
          }
        }

        // Remember last CLK state
        lastStateCLK = currentStateCLK;
      }

    private:
      byte currentStateCLK;
      byte lastStateCLK;
      unsigned long lastRotationTime = 0;
      bool hasCWFunc = false;
      bool hasCCWFunc = false;
      basicCallback CW;
      basicCallback CCW;
  };
#endif

/*****************************************************************************
 * LED CLASS
 *****************************************************************************/
#if TOTAL_LEDS > 0
  class LED {
    public:
      byte pin, pinG, pinB;
      byte R = 0, G = 0, B = 0;  // Initialize RGB values to prevent undefined behavior
      bool isRGB = false;
      bool isDimmable = false;
      bool commonAnode = true;

      // Regular LED initialization
      void init(byte newPin) {
        if (initialized) {
          Serial.println("WARNING: LED already initialized. Ignoring.");
          return;
        }
        pin = newPin;
        pinMode(pin, OUTPUT);
        setState(LOW);
        initialized = true;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Setup LED on pin ");
          DR_DEBUG_PRINTLN(pin);
        #endif
      }

      // RGB LED initialization
      void init(byte newPinR, byte newPinG, byte newPinB) {
        if (initialized) {
          Serial.println("WARNING: LED already initialized. Ignoring.");
          return;
        }
        pin = newPinR;
        pinMode(pin, OUTPUT);
        pinG = newPinG;
        pinMode(pinG, OUTPUT);
        pinB = newPinB;
        pinMode(pinB, OUTPUT);
        isRGB = true;
        setState(LOW);
        initialized = true;

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("Setup RGB LED on pins ");
          DR_DEBUG_PRINT(pin);
          DR_DEBUG_PRINT(",");
          DR_DEBUG_PRINT(pinG);
          DR_DEBUG_PRINT(",");
          DR_DEBUG_PRINTLN(pinB);
        #endif
      }

      void turnOn() {
        setState(HIGH);
      }

      void turnOff() {
        clearBlink();
        clearPulse();
        setState(LOW);
      }

      void flip() {
        if (state == HIGH) {
          setState(LOW);
        } else {
          setState(HIGH);
        }
      }

      LED& setLevel(byte newLevel) {
        isDimmable = true;
        level = newLevel;

        // If the LED is already on, update the brightness
        if (state == HIGH) {
          setState(HIGH);
        }

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("LED level: ");
          DR_DEBUG_PRINTLN(level);
        #endif

        return *this;
      }

      LED& setColor(byte newR, byte newG, byte newB) {
        // Store raw RGB values
        byte rawR = newR;
        byte rawG = newG;
        byte rawB = newB;

        // Apply common anode inversion if needed
        if (commonAnode) {
          R = 255 - rawR;
          G = 255 - rawG;
          B = 255 - rawB;
        } else {
          R = rawR;
          G = rawG;
          B = rawB;
        }

        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINT("RGB Color: ");
          DR_DEBUG_PRINT(rawR);
          DR_DEBUG_PRINT(",");
          DR_DEBUG_PRINT(rawG);
          DR_DEBUG_PRINT(",");
          DR_DEBUG_PRINTLN(rawB);
        #endif

        // Only update the hardware if the LED is already on
        if (state == HIGH) {
          setState(HIGH);
        }

        return *this;
      }

      LED& setCommonAnode(bool isCommonAnode) {
        commonAnode = isCommonAnode;
        return *this;
      }

      LED& pulse(unsigned long pDelay, unsigned int count, byte low, byte high) {
        initPulse(pDelay, count, low, high);
        return *this;
      }

      LED& fadeIn(unsigned long duration) {
        return fadeIn(duration, 255);
      }

      LED& fadeIn(unsigned long duration, byte targetLevel) {
        // Fade from 0 to targetLevel over duration ms (one-shot fade)
        initFade(duration, 0, targetLevel);
        return *this;
      }

      LED& fadeOut(unsigned long duration) {
        // Only fade out if LED is actually on, otherwise do nothing
        if (state == LOW) {
          return *this;  // Already off, no fade needed
        }

        // Fade from current level to 0 over duration ms
        // Determine start level: use current level if dimmable, otherwise 255
        byte startLevel = (isDimmable && level > 0) ? level : 255;

        initFade(duration, startLevel, 0);
        return *this;
      }

      LED& blink(unsigned long delay) {
        initBlink(delay, 0);
        return *this;
      }

      LED& blink(unsigned long delay, unsigned int count) {
        initBlink(delay, count);
        return *this;
      }

      void update() {
        runBlink();
        runPulse();
      }

    private:
      bool initialized = false;
      byte state = LOW;
      byte level = 255;  // Default to full brightness for intuitive turnOn() behavior
      unsigned int blinkCount = 0;
      unsigned int blinkMax = 0;
      unsigned long lastBlinkTime = 0;
      unsigned long blinkDelay = 0;
      bool blinking = false;

      unsigned int pulseCount = 0;
      unsigned int pulseMax = 0;
      byte pulseLow = 0;
      byte pulseHigh = 255;
      unsigned long pulseDuration = 0;  // Half-period duration in ms
      bool pulseUp = true;
      unsigned long lastPulseTime = 0;
      bool pulsing = false;

      void setState(byte newState) {
        state = newState;

        if (isRGB) {
          if (state == LOW) {
            byte val = 0;
            if (commonAnode) {
              val = 255;
            }
            analogWrite(pin, val);
            analogWrite(pinG, val);
            analogWrite(pinB, val);
          } else {
            analogWrite(pin, R);
            analogWrite(pinG, G);
            analogWrite(pinB, B);
          }
        } else if (isDimmable) {
          analogWrite(pin, (state == HIGH) ? level : LOW);
        } else {
          digitalWrite(pin, state);
        }
      }

      void initPulse(unsigned long pDelay, unsigned int count, byte low, byte high) {
        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINTLN("Initiate Pulse");
        #endif

        clearBlink();
        clearPulse();

        pulseLow = low;
        pulseHigh = high;
        pulseMax = count;
        // Guard against zero delay to prevent excessive updates
        unsigned long safePDelay = (pDelay < 2) ? 2 : pDelay;
        pulseDuration = safePDelay / 2;  // Half-period (time to go from low to high or vice versa)
        lastPulseTime = millis();
        pulsing = true;

        setLevel(pulseLow);
        setState(HIGH);
      }

      void initFade(unsigned long duration, byte start, byte end) {
        #ifdef DEVICE_REACTOR_DEBUG
          DR_DEBUG_PRINTLN("Initiate Fade");
        #endif

        clearBlink();
        clearPulse();

        pulseLow = start;
        pulseHigh = end;
        pulseMax = 0;  // Signifies a one-way fade
        // Guard against zero duration to prevent division issues and excessive updates
        pulseDuration = (duration < 1) ? 1 : duration;  // Use full duration (not halved like pulse)
        lastPulseTime = millis();
        pulsing = true;
        pulseUp = (end > start);  // Determine direction

        setLevel(start);
        setState(HIGH);
      }

      void clearPulse() {
        pulseCount = 0;
        pulseMax = 0;
        pulseLow = 0;
        pulseHigh = 0;
        pulseDuration = 0;
        pulseUp = true;
        pulsing = false;
      }

      void runPulse() {
        if (pulsing) {
          unsigned long elapsed = millis() - lastPulseTime;
          byte desiredLevel;

          if (pulseUp) {
            // Calculate level using integer math: low + (range * elapsed / duration)
            if (elapsed >= pulseDuration) {
              // Reached the high point
              level = pulseHigh;
              setState(HIGH);
              if (pulseMax == 0) {
                // One-way fade (fadeIn), stop here
                clearPulse();
                return;
              }
              pulseUp = false;
              lastPulseTime = millis();
            } else {
              // Map elapsed time to level using integer math
              // Cast to unsigned long BEFORE multiplication to prevent overflow
              unsigned int range = pulseHigh - pulseLow;
              desiredLevel = pulseLow + ((unsigned long)range * elapsed) / pulseDuration;
              if (level != desiredLevel) {
                level = desiredLevel;
                setState(HIGH);
              }
            }
          } else {
            // Fading down
            if (elapsed >= pulseDuration) {
              // Reached the low point
              if (pulseMax == 0) {
                // One-way fade (fadeOut), stop here
                level = pulseLow;
                setState(HIGH);
                clearPulse();
                setState(LOW);  // Turn off at end of fadeOut
                return;
              }
              lastPulseTime = millis();
              pulseUp = true;
              pulseCount++;
            } else {
              // Map elapsed time to level (going down) using integer math
              // Cast to unsigned long BEFORE multiplication to prevent overflow
              unsigned int range = pulseHigh - pulseLow;
              desiredLevel = pulseHigh - ((unsigned long)range * elapsed) / pulseDuration;
              if (level != desiredLevel) {
                level = desiredLevel;
                setState(HIGH);
              }
            }
          }

          // We've pulsed enough
          if (pulseCount >= pulseMax && pulseMax > 0) {
            clearPulse();
            setState(LOW);
          }
        }
      }

      void initBlink(unsigned long bDelay, unsigned int count) {
        clearPulse();
        blinkCount = 0;
        // For finite blinks: ensure LED ends OFF after count blinks
        // If currently OFF, need count*2 flips. If currently ON, need count*2-1 flips to end OFF
        if (count > 0) {
          blinkMax = (state == HIGH) ? (count * 2 - 1) : (count * 2);
        } else {
          blinkMax = 0;  // Infinite blink
        }
        // Guard against zero delay to prevent excessive toggling
        unsigned long safeBDelay = (bDelay < 1) ? 1 : bDelay;
        blinkDelay = safeBDelay / 2;  // Since we need to flip
        lastBlinkTime = millis();  // Wait for first interval before starting
        blinking = true;
      }

      void clearBlink() {
        blinkCount = 0;
        blinkMax = 0;
        lastBlinkTime = 0;
        blinkDelay = 0;
        blinking = false;
      }

      void runBlink() {
        if (blinking) {
          // Note: (millis() - lastBlinkTime) handles millis() rollover correctly
          if (millis() - lastBlinkTime >= blinkDelay) {
            lastBlinkTime = millis();
            blinkCount++;
            flip();

            if (blinkCount >= blinkMax && blinkMax > 0) {
              turnOff();
              blinking = false;
            }
          }
        }
      }
  };
#endif

/*****************************************************************************
 * DEVICE CLASS
 *****************************************************************************/
class Device {
  public:
    /****** LEDS *************************************************************/
    #if TOTAL_LEDS > 0
      LED LEDs[TOTAL_LEDS];

      byte newLED(byte pin) {
        if (totalSetupLEDs >= TOTAL_LEDS) {
          Serial.println("FATAL ERROR: Too many LEDs. Increase TOTAL_LEDS. Halting.");
          while(1);
        }
        LEDs[totalSetupLEDs].init(pin);
        return totalSetupLEDs++;
      }

      byte newLED(byte pinR, byte pinG, byte pinB) {
        if (totalSetupLEDs >= TOTAL_LEDS) {
          Serial.println("FATAL ERROR: Too many LEDs. Increase TOTAL_LEDS. Halting.");
          while(1);
        }
        LEDs[totalSetupLEDs].init(pinR, pinG, pinB);
        return totalSetupLEDs++;
      }

      LED& led(byte handle) {
        if (handle >= totalSetupLEDs || handle == INVALID_HANDLE) {
          Serial.println("FATAL ERROR: Invalid LED handle. Halting.");
          while(1);  // Halt execution
        }
        return LEDs[handle];
      }
    #endif

    /****** BUTTONS **********************************************************/
    #if TOTAL_BUTTONS > 0
      Button buttons[TOTAL_BUTTONS];

      byte newButton(byte pin, byte mode = BUTTON_INPUT_PULLUP) {
        if (totalSetupButtons >= TOTAL_BUTTONS) {
          Serial.println("FATAL ERROR: Too many buttons. Increase TOTAL_BUTTONS. Halting.");
          while(1);
        }
        buttons[totalSetupButtons].init(pin, mode);
        return totalSetupButtons++;
      }

      Button& button(byte handle) {
        if (handle >= totalSetupButtons || handle == INVALID_HANDLE) {
          Serial.println("FATAL ERROR: Invalid button handle. Halting.");
          while(1);  // Halt execution
        }
        return buttons[handle];
      }
    #endif

    /****** ANALOG SENSORS ***************************************************/
    #if TOTAL_ANALOG_SENSORS > 0
      AnalogSensor analogSensors[TOTAL_ANALOG_SENSORS];

      byte newAnalogSensor(byte pin) {
        if (totalSetupAnalogSensors >= TOTAL_ANALOG_SENSORS) {
          Serial.println("FATAL ERROR: Too many analog sensors. Increase TOTAL_ANALOG_SENSORS. Halting.");
          while(1);
        }
        analogSensors[totalSetupAnalogSensors].init(pin);
        return totalSetupAnalogSensors++;
      }

      AnalogSensor& analogSensor(byte handle) {
        if (handle >= totalSetupAnalogSensors || handle == INVALID_HANDLE) {
          Serial.println("FATAL ERROR: Invalid analog sensor handle. Halting.");
          while(1);  // Halt execution
        }
        return analogSensors[handle];
      }
    #endif

    /****** ROTARY ENCODERS **************************************************/
    #if TOTAL_ROTARY_ENCODERS > 0
      RotaryEncoder rotaryEncoders[TOTAL_ROTARY_ENCODERS];

      byte newRotaryEncoder(byte swPin, byte dtPin, byte clkPin) {
        if (totalSetupRotaryEncoders >= TOTAL_ROTARY_ENCODERS) {
          Serial.println("FATAL ERROR: Too many rotary encoders. Increase TOTAL_ROTARY_ENCODERS. Halting.");
          while(1);
        }
        rotaryEncoders[totalSetupRotaryEncoders].init(swPin, dtPin, clkPin);
        return totalSetupRotaryEncoders++;
      }

      RotaryEncoder& rotaryEncoder(byte handle) {
        if (handle >= totalSetupRotaryEncoders || handle == INVALID_HANDLE) {
          Serial.println("FATAL ERROR: Invalid rotary encoder handle. Halting.");
          while(1);  // Halt execution
        }
        return rotaryEncoders[handle];
      }
    #endif

    /****** INTERVALS ********************************************************/
    #if TOTAL_INTERVALS > 0
      Interval intervals;

      IntervalHandle after(unsigned long delayMs, byteParamCallback callback) {
        byte idx = intervals.add(callback, delayMs, 1, 0);  // Run once
        return IntervalHandle(this, idx);
      }

      IntervalHandle every(unsigned long delayMs, byteParamCallback callback) {
        byte idx = intervals.add(callback, delayMs, 0, 0);  // Run forever
        return IntervalHandle(this, idx);
      }

      IntervalHandle repeat(unsigned long delayMs, unsigned int count, byteParamCallback callback) {
        byte idx = intervals.add(callback, delayMs, count, 0);  // Run count times
        return IntervalHandle(this, idx);
      }

      void _setIntervalMessage(byte index, byte msg) {
        intervals.setMessage(index, msg);
      }

      void _clearInterval(byte index) {
        intervals.clear(index);
      }

      void _pauseInterval(byte index) {
        intervals.pause(index);
      }

      void _resumeInterval(byte index) {
        intervals.resume(index);
      }
    #endif

    /****** UPDATE ***********************************************************/
    void update() {
      #if TOTAL_LEDS > 0
        for (byte i = 0; i < totalSetupLEDs; i++) {
          LEDs[i].update();
        }
      #endif

      #if TOTAL_BUTTONS > 0
        for (byte i = 0; i < totalSetupButtons; i++) {
          buttons[i].update();
        }
      #endif

      #if TOTAL_ROTARY_ENCODERS > 0
        for (byte i = 0; i < totalSetupRotaryEncoders; i++) {
          rotaryEncoders[i].update();
        }
      #endif

      #if TOTAL_ANALOG_SENSORS > 0
        for (byte i = 0; i < totalSetupAnalogSensors; i++) {
          analogSensors[i].update();
        }
      #endif

      #if TOTAL_INTERVALS > 0
        intervals.update();
      #endif
    }

  private:

    #if TOTAL_LEDS > 0
      byte totalSetupLEDs = 0;
    #endif

    #if TOTAL_BUTTONS > 0
      byte totalSetupButtons = 0;
    #endif

    #if TOTAL_ROTARY_ENCODERS > 0
      byte totalSetupRotaryEncoders = 0;
    #endif

    #if TOTAL_ANALOG_SENSORS > 0
      byte totalSetupAnalogSensors = 0;
    #endif
};

/*****************************************************************************
 * INTERVALHANDLE IMPLEMENTATION
 *****************************************************************************/
#if TOTAL_INTERVALS > 0
  inline IntervalHandle& IntervalHandle::withMessage(byte msg) {
    device->_setIntervalMessage(index, msg);
    return *this;
  }

  inline void IntervalHandle::stop() {
    device->_clearInterval(index);
  }

  inline void IntervalHandle::pause() {
    device->_pauseInterval(index);
  }

  inline void IntervalHandle::resume() {
    device->_resumeInterval(index);
  }
#endif

#endif // DEVICE_REACTOR_H