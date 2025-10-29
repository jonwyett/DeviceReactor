/*
 * DeviceReactor - Potentiometer Dimmer
 *
 * Use a potentiometer to control LED brightness.
 * Demonstrates the new inputRange/outputRange API and fluent configuration.
 *
 * Hardware:
 * - LED on pin 9 (must be PWM-capable pin)
 * - Potentiometer on A0:
 *   - Left pin: GND
 *   - Middle pin: A0
 *   - Right pin: 5V
 */

#define TOTAL_LEDS 1
#define TOTAL_ANALOG_SENSORS 1

#include <DeviceReactor.h>

Device device;

byte led;
byte potentiometer;

void setup() {
  Serial.begin(9600);
  Serial.println("DeviceReactor - Potentiometer Dimmer");

  led = device.newLED(9);  // Must be PWM pin for dimming
  potentiometer = device.newAnalogSensor(A0); // Returns handle

  // Configure potentiometer (analog sensor) with fluent API
  device.analogSensor(potentiometer)
    .outputRange(0, 255)      // Map to LED brightness range (0-255)
    .smoothing(10)            // Average 10 readings for stable output
    .changeThreshold(1)       // Fire on any change for smooth dimming
    .onChange(potChanged);

  device.led(led).turnOn();   // Turn on LED at startup

  Serial.println("Turn the potentiometer to adjust LED brightness");
}

void potChanged(int brightness) {
  Serial.print("Brightness: ");
  Serial.println(brightness);

  device.led(led).setLevel(brightness);
}

void loop() {
  device.update();
}
