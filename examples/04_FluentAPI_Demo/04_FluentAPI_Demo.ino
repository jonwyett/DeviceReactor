/*
 * DeviceReactor - Fluent API Demo
 *
 * Demonstrates method chaining for clean, readable code.
 * Shows how fluent API makes component configuration self-documenting.
 *
 * Hardware:
 * - Red LED on pin 9 (PWM)
 * - Green LED on pin 10 (PWM)
 * - Button on pin 2
 */

#define TOTAL_LEDS 2
#define TOTAL_BUTTONS 1
#define TOTAL_INTERVALS 1

#include <DeviceReactor.h>

Device device;

byte redLED, greenLED;
byte btn;

void setup() {
  Serial.begin(9600);
  Serial.println("DeviceReactor - Fluent API Demo");

  // Create components and store handles
  redLED = device.newLED(9);
  greenLED = device.newLED(10);
  btn = device.newButton(2);

  // Configure button with chained callbacks (fluent API)
  device.button(btn)
    .onPress(buttonPressed)
    .onRelease(buttonReleased);

  // Start with green LED pulsing (infinite loop, 1 second cycle)
  device.led(greenLED).pulse(1000, 0, 0, 255);  // pulse(delay, count, low, high)

  Serial.println("Press the button to see method chaining in action");
}

void buttonPressed() {
  Serial.println("Button pressed");

  // Chain multiple LED operations together
  device.led(redLED)
    .setLevel(200)
    .turnOn();

  // Use interval with message passing
  // The interval will call turnOffLED with redLED handle as the message
  device.after(500, turnOffLED).withMessage(redLED);
}

void buttonReleased() {
  Serial.println("Button released");
}

void turnOffLED(byte ledHandle) {
  device.led(ledHandle).turnOff();
  Serial.println("Red LED turned off by timer");
}

void loop() {
  device.update();
}
