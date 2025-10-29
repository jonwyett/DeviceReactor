/*
 * DeviceReactor - Full Feature Demo
 *
 * Comprehensive example showing all library features:
 * - Handle-based component management
 * - Fluent API configuration
 * - Modern interval system (after/every)
 * - RGB LEDs, dimming, pulsing
 * - Rotary encoder
 *
 * Hardware Setup:
 * - Status LED: Pin 13 (built-in)
 * - Dimmable LED: Pin 9 (PWM)
 * - RGB LED: Pins 3 (R), 5 (G), 6 (B) - all PWM
 * - Button 1: Pin 2
 * - Button 2: Pin 4
 * - Potentiometer: A0
 * - Rotary Encoder: Pin 7 (SW), 8 (DT), 11 (CLK)
 */

#define DEVICE_REACTOR_DEBUG Serial  // Enable debug output

#define TOTAL_LEDS 3
#define TOTAL_BUTTONS 2
#define TOTAL_ANALOG_SENSORS 1
#define TOTAL_ROTARY_ENCODERS 1
#define TOTAL_INTERVALS 3

#include <DeviceReactor.h>

Device device;

// Component handles
byte statusLED, dimLED, rgbLED;
byte btn1, btn2;
byte potentiometer;
byte encoder;

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("========================================");
  Serial.println("DeviceReactor v3.0 - Full Feature Demo");
  Serial.println("========================================\n");

  // --- LEDs ---
  statusLED = device.newLED(13);  // Built-in LED
  dimLED = device.newLED(9);      // PWM pin for dimming
  rgbLED = device.newLED(3, 5, 6); // RGB LED (all PWM pins)

  // --- Buttons ---
  btn1 = device.newButton(2);
  device.button(btn1).onPress(btn1Pressed);

  btn2 = device.newButton(4);
  device.button(btn2)
    .onPress(btn2Pressed)
    .onRelease(btn2Released);

  // --- Potentiometer (Analog Sensor) ---
  potentiometer = device.newAnalogSensor(A0);
  device.analogSensor(potentiometer)
    .outputRange(0, 100)
    .smoothing(10)
    .changeThreshold(3)
    .onChange(volumeChanged);

  // --- Rotary Encoder ---
  encoder = device.newRotaryEncoder(7, 8, 11);  // SW, DT, CLK
  device.rotaryEncoder(encoder)
    .onClockwise(encoderCW)
    .onCounterClockwise(encoderCCW)
    .onPress(encoderPressed);

  // --- Intervals ---
  device.every(2000, heartbeat);  // Runs forever every 2 seconds

  // Start with status LED blinking
  device.led(statusLED).blink(1000);

  Serial.println("Setup complete. Interact with components!");
  Serial.println("- Button 1: Toggle status LED");
  Serial.println("- Button 2: RGB blink 3 times");
  Serial.println("- Pot: Adjust dim LED brightness");
  Serial.println("- Encoder: Rotate to change RGB hue, press for random color\n");
}

void btn1Pressed() {
  Serial.println("Button 1: Pressed - Toggle status LED");
  device.led(statusLED).flip();
}

void btn2Pressed() {
  Serial.println("Button 2: Pressed - RGB blink 3 times");
  device.led(rgbLED)
    .setColor(255, 0, 0)  // Red
    .blink(200, 3);
}

void btn2Released() {
  Serial.println("Button 2: Released");
}

void volumeChanged(int value) {
  Serial.print("Volume: ");
  Serial.println(value);

  // Map 0-100 to LED brightness 0-255
  byte brightness = map(value, 0, 100, 0, 255);
  device.led(dimLED).setLevel(brightness).turnOn();
}

void encoderCW() {
  Serial.println("Encoder: Clockwise");
  // Increase RGB hue
  static byte hue = 0;
  hue += 10;
  device.led(rgbLED).setColor(hue, 255 - hue, 128).turnOn();
}

void encoderCCW() {
  Serial.println("Encoder: Counter-clockwise");
  // Decrease RGB hue
  static byte hue = 0;
  hue -= 10;
  device.led(rgbLED).setColor(hue, 255 - hue, 128).turnOn();
}

void encoderPressed() {
  Serial.println("Encoder: Button pressed - Random color!");
  device.led(rgbLED)
    .setColor(random(256), random(256), random(256))
    .turnOn();
}

void heartbeat(byte msg) {
  Serial.println("♥ Heartbeat - System running");
}

void loop() {
  device.update();
}
