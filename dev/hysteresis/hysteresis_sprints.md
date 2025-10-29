### **Project: `AnalogSensor` Refactor - Sprint Plan**

**Objective:** To refactor the existing `AnalogSensor` class to implement the advanced Quantized Hysteresis and Zone-based state management systems. The new implementation will be more robust, flexible, and intuitive.

---

### **Sprint 1: Core Data Structures and State Management**

**Goal:** Establish the foundational data structures and internal state variables required for all new features. This sprint focuses on the "what" before the "how."

*   **Task 1.1: Refactor Internal State Variables.**
    *   Review all existing member variables in the `AnalogSensor` class.
    *   Add new member variables to support the upcoming features:
        *   `int hiResValue;` // To store the value after smoothing, clamping, and mapping, but before stability logic.
        *   `int currentReportedValue;` // The final, stable value exposed to the user.
        *   `byte currentZoneID;` // The ID of the currently active zone.
        *   `byte previousZoneID;` // The ID of the zone from the previous update cycle.
        *   `int hysteresis_amount;` // The 'H' value for Quantized Hysteresis.
    *   **Action:** Modify the `AnalogSensor` class definition in the header file.

*   **Task 1.2: Implement the `Zone` Structure.**
    *   Define a new `struct` inside the `AnalogSensor` class named `Zone`.
    *   The `struct` must contain the following members: `byte id;`, `int min_val;`, `int max_val;`.
    *   **Action:** Add the `struct Zone { ... };` definition to the `AnalogSensor` class in the header file.

*   **Task 1.3: Add Zone Storage.**
    *   Add a fixed-size array of `Zone` structs to the `AnalogSensor` class members: `Zone defined_zones[MAX_ZONES_PER_SENSOR];`.
    *   Add a member to track the number of zones added: `byte zone_count = 0;`.
    *   Define a new compile-time macro `MAX_ZONES_PER_SENSOR` (default to `5`) in the library's main header to control memory usage.
    *   **Action:** Modify the class definition and add the new macro.

---

### **Sprint 2: Implementing the Signal Processing Pipeline**

**Goal:** Implement the precise, ordered signal processing pipeline that converts a raw ADC reading into a clean `hiResValue`.

*   **Task 2.1: Re-order the `update()` Logic.**
    *   Refactor the existing `update()` method to follow this exact sequence:
        1.  `Read Raw ADC Value`
        2.  `Smooth the Raw Signal`
        3.  `Clamp to INPUT Range` (using `inputMin`, `inputMax`)
        4.  `Map to Output Range`
        5.  `Clamp to OUTPUT Range` (using `outputMin`, `outputMax`)
        6.  `Invert Mapped Value`
    *   The final result of this pipeline should be stored in the `hiResValue` member variable.
    *   **Action:** Rewrite the core logic of the `AnalogSensor::update()` method.

---

### **Sprint 3: Implementing the Stability Modes**

**Goal:** Implement the two mutually exclusive stability modes that operate on the `hiResValue`.

*   **Task 3.1: Implement Mode Selection Logic.**
    *   In the `update()` method, after `hiResValue` is calculated, implement the `if / else if / else` block to select the active stability mode.
    *   The precedence is:
        1.  If `quantizeStep > 0`, run **Quantized Hysteresis**.
        2.  Else if `changeThreshold > 1`, run **Change Threshold**.
        3.  Else, run **Raw Mode**.
    *   **Action:** Add the mode selection control flow to `AnalogSensor::update()`.

*   **Task 3.2: Implement Quantized Hysteresis (Q+H) Logic.**
    *   Inside the `if (quantizeStep > 0)` block, implement the full "Grid-Anchored" Quantized Hysteresis algorithm as defined in the design document.
    *   This logic will operate on `hiResValue` and, if a change is committed, will update `currentReportedValue`.
    *   It must correctly handle initialization on the first run.
    *   **Action:** Write the Quantized Hysteresis algorithm logic.

*   **Task 3.3: Implement Change Threshold (T) Logic.**
    *   Inside the `else if (changeThreshold > 1)` block, implement the "Floating Anchor" dead-zone logic.
    *   This logic will operate on `hiResValue` and, if a change is committed, will update `currentReportedValue`.
    *   **Action:** Write the Change Threshold algorithm logic.

*   **Task 3.4: Implement Raw Mode and `onChange` Event.**
    *   Ensure that when a change is committed in *any* of the three modes, the `onChange` callback is fired with the new `currentReportedValue`.
    *   Update the `previousValue` member (or equivalent) to the new `currentReportedValue` after the event fires.
    *   **Action:** Finalize the event firing logic at the end of the stability mode block.

---

### **Sprint 4: Implementing the Zone State Machine**

**Goal:** Implement the Zone system, which acts as a second-layer state machine operating on the final `currentReportedValue`.

*   **Task 4.1: Implement the `.addZone()` Setter.**
    *   Create the public method `AnalogSensor& addZone(byte id, int min_value, int max_value)`.
    *   It should add a new `Zone` struct to the `defined_zones` array and increment `zone_count`.
    *   Include a safety check against `MAX_ZONES_PER_SENSOR`.
    *   **Action:** Implement the `addZone` method.

*   **Task 4.2: Implement the Zone Detection Logic.**
    *   At the end of the `update()` loop (after the stability modes have run), add the Zone detection logic.
    *   This logic will iterate through the `defined_zones` array.
    *   It will check which zone the `currentReportedValue` falls into and update the `currentZoneID` member. If it falls into no defined zones, `currentZoneID` should be set to a default value (e.g., 0 or 255).
    *   **Action:** Add the Zone lookup loop to `AnalogSensor::update()`.

*   **Task 4.3: Implement the `.onZoneChange()` Event.**
    *   Create the public method `AnalogSensor& onZoneChange(void (*callback)(byte))`.
    *   Store the callback pointer.
    *   In the `update()` loop, after `currentZoneID` has been determined, compare it to `previousZoneID`.
    *   If they differ, fire the `onZoneChange` callback with `currentZoneID`, and then update `previousZoneID = currentZoneID`.
    *   **Action:** Implement the `onZoneChange` setter and event firing logic.

---

### **Sprint 5: API Refinement and Presets**

**Goal:** Finalize the public API to be intuitive and powerful, including the high-level preset system.

*   **Task 5.1: Refactor `.quantize()` to be Overloaded.**
    *   Modify the public API to have two versions of the `.quantize()` method:
        1.  `quantize(int step)`: Sets `Q` and automatically calculates `H = Q / 4`.
        2.  `quantize(int step, int hysteresis)`: Sets `Q` and `H` explicitly.
    *   Remove the separate `.hysteresis()` setter.
    *   **Action:** Update the header file and implementation for the `quantize` method.

*   **Task 5.2: Implement the Preset Data Structures.**
    *   Define the `enum Preset` inside the `AnalogSensor` class.
    *   Define the `struct PresetConfig`.
    *   Declare the `static const PresetConfig preset_configs[]` array in the header file.
    *   Define and initialize the `preset_configs` array in the `.cpp` file with the agreed-upon recipes.
    *   **Action:** Implement the data-driven structures.

*   **Task 5.3: Implement the `.configure()` Method.**
    *   Create the public method `AnalogSensor& configure(Preset preset)`.
    *   Implement the data-driven logic that looks up the recipe in the `preset_configs` array and calls the appropriate low-level setters (`.smoothing`, `.outputRange`, `.quantize`, `.changeThreshold`).
    *   Ensure the logic correctly chooses between `.quantize()` and `.changeThreshold()` based on the recipe's `Q` value.
    *   **Action:** Implement the `configure` method.

---

### **Sprint 6: Documentation and Final Review**

**Goal:** Update all user-facing documentation to reflect the new, powerful API.

*   **Task 6.1: Update `README.md`.**
    *   Completely rewrite the "Analog Sensors" section of the `README.md`.
    *   Clearly explain the two stability modes (`changeThreshold` vs. `quantize`).
    *   Provide clear examples for both overloaded `quantize` methods.
    *   Add a new major section explaining the Zone System with a compelling example (e.g., the thermostat).
    *   Add a new section explaining the `.configure()` presets and how to use them.
    *   **Action:** Author the new documentation.

*   **Task 6.2: Final Code Review.**
    *   Review the entire refactored class for consistency, correctness, and adherence to the design document.
    *   Ensure all member variables are initialized correctly.
    *   Verify the order of operations in the signal processing pipeline.
    *   Check that the stability mode precedence (`Q+H` wins over `T`) is correctly implemented.
    *   **Action:** Perform a full code audit.