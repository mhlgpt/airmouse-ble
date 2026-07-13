#include <Arduino.h>
#include <Wire.h>
#include <MPU6500_WE.h>
#include <BleMouse.h>

// I2C Pins for MPU-6500
#define I2C_SDA 26
#define I2C_SCL 27
#define SENSOR_ADDR 0x68 

// ESP32 Native Touch Pins
#define PIN_LEFT_CLICK  32
#define PIN_RIGHT_CLICK 33
#define PIN_SCROLL_UP   13
#define PIN_SCROLL_DOWN 14
#define PIN_ACTIVATOR   12

// --- TOUCH SENSITIVITY THRESHOLD ---
// When untouched, touchRead usually returns ~50-70. 
// When touched, it drops below ~30. Adjust this if it's too sensitive or not sensitive enough!
const int TOUCH_THRESHOLD = 50;

MPU6500_WE mySensor = MPU6500_WE(&Wire, SENSOR_ADDR);
BleMouse bleMouse("Air Mouse", "MMA", 100);

// Click tracking
bool lastLeftState = false;
bool lastRightState = false;

// Scroll tracking
unsigned long lastScrollTime = 0;
const int scrollInterval = 100; 

// Anti-jitter Filter Variables
float smoothedZ = 0;
float smoothedX = 0;
const float filterAlpha = 0.2; 
const float deadzone = 3.5; 

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); 

  Serial.println("\nChecking for MPU-6500...");
  if (!mySensor.init()) {
    Serial.println("ERROR: Sensor failed to initialize.");
    while (1) { delay(100); } 
  }

  Serial.println("MPU-6500 detected! Calibrating (LEAVE FLAT FOR 2 SECONDS)...");
  mySensor.autoOffsets(); 
  
  Serial.println("Starting Bluetooth...");
  bleMouse.begin();
}

void loop() {
  if (bleMouse.isConnected()) {
    
    int moveX = 0;
    int moveY = 0;

    // ==========================================
    // 1. GYROSCOPE MOVEMENT (If Activator is touched)
    // ==========================================
    // touchRead returns a smaller number when your finger touches the pin
    if (touchRead(PIN_ACTIVATOR) < TOUCH_THRESHOLD) {
      xyzFloat gyroData = mySensor.getGyrValues();

      smoothedZ = (gyroData.z * filterAlpha) + (smoothedZ * (1.0 - filterAlpha));
      smoothedX = (gyroData.x * filterAlpha) + (smoothedX * (1.0 - filterAlpha));

      if (abs(smoothedZ) > deadzone) { moveX = (int)(smoothedZ / 2.0); }
      if (abs(smoothedX) > deadzone) { moveY = (int)(smoothedX / 2.0); }
      
    } else {
      smoothedZ = 0;
      smoothedX = 0;
    }

    // ==========================================
    // 2. NATIVE TOUCH SENSOR CLICKS 
    // ==========================================
    // Convert the analog touch reading into a simple true/false boolean
    bool currentLeftState = (touchRead(PIN_LEFT_CLICK) < TOUCH_THRESHOLD);
    bool currentRightState = (touchRead(PIN_RIGHT_CLICK) < TOUCH_THRESHOLD);

    if (currentLeftState == true && lastLeftState == false) {
      bleMouse.press(MOUSE_LEFT);
    } else if (currentLeftState == false && lastLeftState == true) {
      bleMouse.release(MOUSE_LEFT);
    }
    lastLeftState = currentLeftState; 

    if (currentRightState == true && lastRightState == false) {
      bleMouse.press(MOUSE_RIGHT);
    } else if (currentRightState == false && lastRightState == true) {
      bleMouse.release(MOUSE_RIGHT);
    }
    lastRightState = currentRightState; 

    // ==========================================
    // 3. NATIVE TOUCH SCROLLING
    // ==========================================
    if (millis() - lastScrollTime > scrollInterval) {
      int scrollAmount = 0;
      
      if (touchRead(PIN_SCROLL_UP) < TOUCH_THRESHOLD) {
        scrollAmount = 1;
      } else if (touchRead(PIN_SCROLL_DOWN) < TOUCH_THRESHOLD) {
        scrollAmount = -1;
      }

      if (moveX != 0 || moveY != 0 || scrollAmount != 0) {
        bleMouse.move(moveX, moveY, scrollAmount);
      }
      
      lastScrollTime = millis(); 
      
    } else {
      if (moveX != 0 || moveY != 0) {
        bleMouse.move(moveX, moveY, 0);
      }
    }
  }
  
  delay(20); // ~50Hz loop
}