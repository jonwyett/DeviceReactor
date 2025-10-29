### Universal Feature 1: The RadioGroup

This is your primary example and the perfect place to start. It manages the state for a group of "radio buttons" where only one can be active.

*   **The Concept:** Track which of a set of buttons was the last one pressed.
*   **Why it's Universal:** It makes no assumptions about feedback. The user might have one LED per button, a single RGB LED, an LCD display, or no visual feedback at all. The `RadioGroup` doesn't care; it just manages the "active index" state.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    #include <DeviceReactor.h>
    #include <DeviceConductor.h>

    Device device;
    DeviceConductor conductor(&device);

    byte rgbLED;
    byte btnRed, btnGreen, btnBlue;
    RadioGroup colorSelector;

    // This is the user's callback. It gets the index of the newly active button.
    void onColorSelectionChanged(byte newIndex, byte previousIndex) {
      Serial.print("Group selection changed to index: ");
      Serial.println(newIndex);

      // USER'S IMPLEMENTATION: Use the index to control a single RGB LED
      if (newIndex == 0) device.led(rgbLED).setColor(255, 0, 0);   // Red
      if (newIndex == 1) device.led(rgbLED).setColor(0, 255, 0);   // Green
      if (newIndex == 2) device.led(rgbLED).setColor(0, 0, 255);   // Blue
    }

    void setup() {
      // 1. Create physical components with DeviceReactor
      rgbLED = device.newLED(3, 5, 6);
      btnRed = device.newButton(2);
      btnGreen = device.newButton(4);
      btnBlue = device.newButton(7);

      // 2. Create the logical RadioGroup with DeviceConductor
      byte buttonHandles[] = {btnRed, btnGreen, btnBlue};
      colorSelector = conductor.newRadioGroup(buttonHandles, 3) // Handles, count
                               .onChange(onColorSelectionChanged)
                               .initialState(0); // Optionally set the first button as active
    }

    void loop() {
      device.update();
      // conductor.update() is likely not needed if it hooks into DeviceReactor's
      // events directly during creation.
    }
    ```
    This design is perfect. It provides the core radio-button logic without dictating any hardware response.

### Universal Feature 2: The StatefulToggle

Let's fix the toggle. Instead of linking a button to an LED, it should just manage a boolean `active` state.

*   **The Concept:** A logical boolean flag that is flipped each time a button is pressed.
*   **Why it's Universal:** The user decides what "active" and "inactive" mean. It could be turning an LED on/off, starting/stopping a motor, or enabling/disabling a software feature.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    StatefulToggle powerToggle;
    byte powerButton, statusLED;

    void onPowerActivate() {
      Serial.println("System Activated.");
      device.led(statusLED).turnOn();
      // startMotor();
    }

    void onPowerDeactivate() {
      Serial.println("System Deactivated.");
      device.led(statusLED).turnOff();
      // stopMotor();
    }

    void setup() {
      powerButton = device.newButton(2);
      statusLED = device.newLED(13);

      powerToggle = conductor.newStatefulToggle(powerButton)
                             .onActivate(onPowerActivate)
                             .onDeactivate(onPowerDe Vẽ);
    }
    ```
    Again, this provides the state-flipping logic cleanly and leaves the implementation entirely to the developer via callbacks.

### Universal Feature 3: The Sequencer

Many projects need to cycle through a predefined list of settings, patterns, or modes. A sequencer helper would be invaluable.

*   **The Concept:** A component that tracks a current step/index within a defined range and can be advanced (and optionally reversed) by button presses.
*   **Why it's Universal:** It provides a "current step" number. The developer can use this number as an index into an array of colors, sound files, motor speeds, animation patterns, etc.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    Sequencer patternSelector;
    byte nextButton, prevButton, patternLED;

    // An array of brightness values defined in the user's sketch
    byte patterns[] = { 25, 50, 100, 150, 200, 255 };

    void onPatternChanged(byte newStepIndex) {
      Serial.print("Sequence step changed to: ");
      Serial.println(newStepIndex);

      // USER'S IMPLEMENTATION: Use the index to look up a value
      byte newBrightness = patterns[newStepIndex];
      device.led(patternLED).setLevel(newBrightness).turnOn();
    }

    void setup() {
      nextButton = device.newButton(2);
      prevButton = device.newButton(3);
      patternLED = device.newLED(9); // PWM pin

      patternSelector = conductor.newSequencer()
                                 .steps(6) // Total number of steps
                                 .next(nextButton)
                                 .previous(prevButton) // Optional
                                 .wraps(true) // Optional: Does it wrap from last to first?
                                 .onChange(onPatternChanged);
    }
    ```

### Universal Feature 4: The ValueAdjuster (Counter)

This is a common requirement: using buttons to increment/decrement a value within a specific range.

*   **The Concept:** Manage a numerical value that can be changed by buttons, respecting min/max boundaries.
*   **Why it's Universal:** It abstracts the tedious boilerplate of `value++`, `if (value > max) value = max;` from the main loop. The user can map the resulting value to anything: PWM brightness, a timer delay, a servo position, etc.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    ValueAdjuster brightnessControl;
    byte upButton, downButton, outputLED;

    void onBrightnessChanged(int newValue) { // Use int for wider range
      Serial.print("Value changed to: ");
      Serial.println(newValue);
      device.led(outputLED).setLevel(newValue);
    }

    void setup() {
      upButton = device.newButton(2);
      downButton = device.newButton(3);
      outputLED = device.newLED(9);

      brightnessControl = conductor.newValueAdjuster()
                                   .increment(upButton)
                                   .decrement(downButton)
                                   .range(0, 255) // Min, Max
                                   .step(5)       // Optional step value
                                   .onChange(onBrightnessChanged);
    }
    ```

---


### Universal Feature 5: The ValueAdjuster (Expanded for Encoders)

We previously designed this for buttons, but a rotary encoder is its natural soulmate. The existing API can be beautifully extended to support both, making it even more universal.

*   **The Concept:** Manage a numerical value that can be changed by buttons OR an encoder, respecting boundaries and step sizes.
*   **Why it's Universal:** It's the quintessential control for settings like volume, brightness, speed, or menu navigation. By accepting either button or encoder inputs, it adapts to the user's available hardware.
*   **Proposed "DeviceConductor" API (with Encoder support):**

    ```cpp
    ValueAdjuster volumeControl;
    byte volumeEncoder;
    byte muteButton; // Encoder's built-in button

    void onVolumeChanged(int newValue) {
      Serial.print("Volume is now: ");
      Serial.println(newValue);
      // User maps this value to an audio library, PWM output, etc.
    }

    void onMuteToggled() {
      Serial.println("Mute toggled!");
    }

    void setup() {
      // Create encoder with DeviceReactor
      volumeEncoder = device.newRotaryEncoder(2, 3, 4) // SW, DT, CLK
                              .onPress(onMuteToggled); // Use DeviceReactor for simple events

      // Create the logical adjuster with DeviceConductor
      volumeControl = conductor.newValueAdjuster()
                               .controlledBy(volumeEncoder) // <-- New method for encoders!
                               .range(0, 100)
                               .initialValue(50)
                               .onChange(onVolumeChanged);
    }
    ```
    Notice the elegance here: the `ValueAdjuster` handles the counting and clamping logic from the encoder's rotation events. The simple press event is still handled directly by `DeviceReactor`. The two libraries work in perfect harmony.

### Universal Feature 6: The AnalogDetent

A common use for a potentiometer is not as a smooth analog control, but as a multi-position selector switch. This helper adds "snap points" or detents to an analog input.

*   **The Concept:** Divide an analog sensor's range into a number of discrete zones. Report when the sensor's value enters a new zone.
*   **Why it's Universal:** It turns any cheap potentiometer or photoresistor into a mode selector, a menu navigator, or a discrete level setter (e.g., Low/Medium/High fan speed).
*   **Proposed "DeviceConductor" API:**

    ```cpp
    AnalogDetent modeSelector;
    byte modePot;
    byte modeDisplayLED;

    const char* modes[] = {"HEAT", "COOL", "FAN", "AUTO"};

    // Callback receives the index of the new zone (detent)
    void onModeChanged(byte newIndex) {
      Serial.print("Mode changed to: ");
      Serial.println(modes[newIndex]);
      // User could update an LCD, change an LED color, etc.
      device.led(modeDisplayLED).blink(100, newIndex + 1); // Blink 1, 2, 3, or 4 times
    }

    void setup() {
      byte potHandle = device.newAnalogSensor(A0).smoothing(5);
      modeDisplayLED = device.newLED(13);

      modeSelector = conductor.newAnalogDetent(potHandle)
                                .zones(4) // Creates 4 equal zones (0-255, 256-511, etc.)
                                .onChange(onModeChanged);
    }
    ```

### Universal Feature 7: The DPad

Your suggestion is a classic and fits perfectly. It groups four buttons into a single logical unit for directional input.

*   **The Concept:** Consolidate four button press events into a single, directional callback.
*   **Why it's Universal:** Essential for menu navigation, controlling robotics, games, or any 2D positioning task.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    enum Direction { NONE, UP, DOWN, LEFT, RIGHT };
    DPad menuNav;

    // A single callback handles all directions
    void onDirectionPressed(Direction dir) {
      switch(dir) {
        case UP:    Serial.println("NAV: UP");    break;
        case DOWN:  Serial.println("NAV: DOWN");  break;
        case LEFT:  Serial.println("NAV: LEFT");  break;
        case RIGHT: Serial.println("NAV: RIGHT"); break;
      }
    }

    void setup() {
      byte btnU = device.newButton(2);
      byte btnD = device.newButton(3);
      byte btnL = device.newButton(4);
      byte btnR = device.newButton(5);

      menuNav = conductor.newDPad(btnU, btnD, btnL, btnR)
                         .onPress(onDirectionPressed);
    }
    ```

### Universal Feature 8: The InteractionDetector (Long Press & Double Click)

This is a huge quality-of-life improvement. Simple presses are easy, but timing-based interactions like long-press and double-click are tedious to implement correctly.

*   **The Concept:** A wrapper for a button that can distinguish between a normal press, a long press, and a double-click.
*   **Why it's Universal:** These are fundamental UI interactions. A long-press is often used for a secondary function (e.g., enter settings menu), while a double-click might perform a special action (e.g., select all).
*   **Proposed "DeviceConductor" API:**

    ```cpp
    InteractionDetector mainButton;

    void onSimpleClick() {
      Serial.println("Action: Select Item");
    }

    void onLongPress() {
      Serial.println("Action: Open Context Menu");
    }

    void onDoubleClick() {
      Serial.println("Action: Delete Item");
    }

    void setup() {
      byte btnHandle = device.newButton(2);

      mainButton = conductor.newInteractionDetector(btnHandle)
                              .onClick(onSimpleClick)
                              .onLongPress(onLongPress, 750) // Optional custom delay (ms)
                              .onDoubleClick(onDoubleClick);
    }
    ```

### Universal Feature 9: The Modifier

Inspired by keyboard "Shift" or "Ctrl" keys, this helper allows one button to change the function of other buttons while it's being held down.

*   **The Concept:** Create a set of alternative actions for components that are only active when a modifier button is held.
*   **Why it's Universal:** It dramatically increases the number of functions you can assign to a limited set of physical buttons, which is a constant challenge in hardware projects.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    ValueAdjuster adjuster;
    Modifier shift;

    void onAdjust(int val) { Serial.print("Fine adjust: "); Serial.println(val); }
    void onShiftAdjust(int val) { Serial.print("Coarse adjust: "); Serial.println(val); }

    void setup() {
      byte adjEncoder = device.newRotaryEncoder(2, 3, 4);
      byte shiftButton = device.newButton(5);

      // 1. Setup the default behavior
      adjuster = conductor.newValueAdjuster()
                          .controlledBy(adjEncoder)
                          .range(0, 255)
                          .step(1)
                          .onChange(onAdjust);

      // 2. Create the modifier and link it to the shift button
      shift = conductor.newModifier(shiftButton);

      // 3. Define the modified behavior for the ValueAdjuster
      shift.whenActive(adjuster)
           .step(10) // Changes the adjuster's step value to 10
           .onChange(onShiftAdjust); // Optionally use a different callback
    }
    ```
    This is a more advanced example, but it shows the power. When the shift button is held, the `Modifier` temporarily reconfigures the `ValueAdjuster` to use a step of 10. When released, it reverts to a step of 1. This is incredibly powerful and entirely managed by the logic helpers.

---

#### The HysteresisController (Thermostat Logic)

Your `.onHigh()` and `.onLow()` idea is great for single events. But a very common real-world problem is preventing a system from rapidly oscillating when a sensor value hovers around a threshold. For example, a heater turning on and off every second. The solution is **hysteresis**: turning it ON at one threshold and OFF at a *different* threshold.

This is complex stateful logic and a perfect job for a helper.

*   **The Concept:** Manage an ON/OFF state based on a sensor crossing two different thresholds (an upper and a lower bound).
*   **Why it's Universal:** This is the core logic for any thermostat, humidity controller, pressure regulator, or automatic pump system.
*   **Proposed "DeviceConductor" API:**

    ```cpp
    HysteresisController temperatureControl;

    void onHeaterActivate() {
      Serial.println("Temperature too low. Turning heater ON.");
      // digitalWrite(HEATER_RELAY, HIGH);
    }

    void onHeaterDeactivate() {
      Serial.println("Temperature is sufficient. Turning heater OFF.");
      // digitalWrite(HEATER_RELAY, LOW);
    }

    void setup() {
      byte tempSensorHandle = device.newAnalogSensor(A0).outputRange(0, 100); // Mapped to Celsius

      temperatureControl = conductor.newHysteresisController(tempSensorHandle)
                                      .activateWhenBelow(20) // Turn heater ON when temp drops to 20°C
                                      .deactivateWhenAbove(22) // Turn heater OFF only when temp rises to 22°C
                                      .onActivate(onHeaterActivate)
                                      .onDeactivate(onHeaterDeactivate);
    }
    ```
This helper solves a non-trivial, real-world problem with a clean, declarative API, perfectly illustrating the power of the `DeviceConductor` concept.


