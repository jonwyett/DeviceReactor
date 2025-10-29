/*
 * DeviceReactor - Button Toggle
 *
 * Press a button to toggle an LED on/off.
 * Demonstrates handle-based component references.
 *
 * Hardware:
 * - LED on pin 13 (built-in)
 * - Button on pin 2 (using INPUT_PULLUP, no external resistor needed)
 */

#define TOTAL_LEDS 1
#define TOTAL_BUTTONS 1

#include <DeviceReactor.h>

Device device;

byte led;
byte btn;

void setup() {
  Serial.begin(9600);
  Serial.println("DeviceReactor - Button Toggle");

  // Create components and store handles (unified API)
  led = device.newLED(13);
  btn = device.newButton(2);  // Returns handle, defaults to INPUT_PULLUP

  // Configure button with fluent callback setup
  device.button(btn).onPress(buttonPressed);

  Serial.println("Press the button to toggle the LED");
}

void buttonPressed() {
  device.led(led).flip();
  Serial.println("Button pressed - LED toggled");
}

void loop() {
  device.update();
}
