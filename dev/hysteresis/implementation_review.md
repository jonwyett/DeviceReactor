### **Code Review: `AnalogSensor` Refactor**


#### **Critical Bug Fixes (Must be fixed)**

**1. Integer Division in Quantization Logic**

*   **Location:** `update()` and `performInitialRead()` methods.
*   **Problem:** The line `int V_potential = ((hiResValue + quantizeStep / 2) / quantizeStep) * quantizeStep;` uses integer division. This performs truncation, not mathematical rounding. For example, if `quantizeStep` is `10`, `10 / 2` is `5`. But if `quantizeStep` is `5`, `5 / 2` becomes `2`, not `2.5`. This will cause quantization to be skewed towards lower values.
*   **Solution:** Perform the calculation using floating-point math and the `round()` function to ensure correct mathematical rounding.
*   **Recommendation:**
    ```cpp
    // Replace this:
    int V_potential = ((hiResValue + quantizeStep / 2) / quantizeStep) * quantizeStep;

    // With this:
    int V_potential = round((float)hiResValue / quantizeStep) * quantizeStep;
    ```    This needs to be fixed in both `update()` and `performInitialRead()`.

**2. Legacy `currentValue` is Not Being Updated Correctly**

*   **Location:** `update()` method.
*   **Problem:** The code `currentValue = newValue;` is inside the `if (valueChanged)` block. This means the publicly accessible `.value()` method will only return an updated value when an `onChange` event fires. This breaks the "Commit-on-Threshold" model and leads to the "stale value" problem we discussed. The `.value()` method should always reflect the current stable state, even if no event was fired.
*   **Solution:** The `currentReportedValue` is the source of truth and should be updated every cycle where the value is stable, even if no event is fired. The logic is slightly incorrect. `currentReportedValue` should not change unless a trigger is met.
*   **Let's reconsider the logic flow:** The current implementation correctly updates `currentReportedValue` only when `valueChanged` is true. This preserves the "Commit-on-Threshold" model. However, the legacy `currentValue` (from `.value()`) is tied to it. This is a subtle but acceptable design choice, as it aligns `onChange` and `.value()` perfectly. **Let's mark this as "Acceptable Behavior" but note that it prioritizes Stability over Accuracy.** The user must understand that `.value()` will not track the `hiResValue` in real-time.

---

#### **High Priority Improvements (Highly Recommended)**

**1. Zone Detection Default Value**

*   **Location:** `findZoneForValue()` method.
*   **Problem:** The method returns `0` if no zone is found. This is problematic because `0` could be a valid, user-defined zone ID. This creates ambiguity: did the sensor enter Zone 0, or did it enter no zone at all?
*   **Solution:** Use a dedicated constant to represent the "no zone" state. `DeviceReactor` already has `INVALID_HANDLE` (255), which is perfect for this.
*   **Recommendation:**
    *   In `findZoneForValue()`, change `return 0;` to `return INVALID_HANDLE;`.
    *   In `update()`, when initializing `detectedZoneID`, initialize it to `INVALID_HANDLE`.
    *   In `performInitialRead()`, initialize `currentZoneID` and `previousZoneID` to `INVALID_HANDLE`.
    *   This makes the system unambiguous.

**2. Initialization of Zone State**

*   **Location:** `performInitialRead()` method.
*   **Problem:** The initial zone is not calculated and set during the initial read. `currentZoneID` and `previousZoneID` will remain `0` until the first `update()` loop. This means if the sensor starts in Zone 3, the `onZoneChange` will fire on the first `update`, which is unexpected behavior (similar to `onChange`).
*   **Solution:** After `currentReportedValue` is calculated in `performInitialRead()`, immediately determine its zone and set *both* `currentZoneID` and `previousZoneID` to that zone's ID.
*   **Recommendation:**
    ```cpp
    // In performInitialRead(), after currentReportedValue is set:
    if (zone_count > 0) {
        byte initialZone = findZoneForValue(currentReportedValue);
        currentZoneID = initialZone;
        previousZoneID = initialZone; // Set both to prevent firing on first update
    }
    ```

