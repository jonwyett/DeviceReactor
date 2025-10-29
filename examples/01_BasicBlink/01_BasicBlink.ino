/*
 * DeviceReactor - Basic Blink
 *
 * The simplest example: blink an LED.
 * Demonstrates the new handle-based API.
 */

#define TOTAL_LEDS 1

#include <DeviceReactor.h>

Device device;

void setup() {
  Serial.begin(9600);
  Serial.println("DeviceReactor Basic Blink");

  // Create LED and start blinking (500ms interval)
  byte led = device.newLED(13);
  device.led(led).blink(500);

  Serial.println("LED should be blinking now.");
}

void loop() {
  device.update();
}
