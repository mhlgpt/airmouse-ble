#include <Arduino.h>
#include <Wire.h>
#include <MPU6500_WE.h>
#include <BleMouse.h>

// I2C Pins for MPU-6500
#define I2C_SDA 26
#define I2C_SCL 27
#define SENSOR_ADDR 0x68 

// HW-763 Touch Sensor Pins (UPDATED)
#define PIN_LEFT_CLICK  13
#define PIN_RIGHT_CLICK 14
#define PIN_SCROLL_UP   25
#define PIN_SCROLL_DOWN 33
#define PIN_ACTIVATOR   32

MPU6500_WE mySensor = MPU6500_WE(&Wire, SENSOR_ADDR);
BleMouse bleMouse("ESP32 Air Mouse", "YourName", 100);

// Click tracking
bool lastLeftState = LOW;
bool lastRightState = LOW;

// Scroll tracking
unsigned long lastScrollTime = 0;
const int scrollInterval = 100; 

// --- ANTI-JITTER FILTER VARIABLES ---
float smoothedZ = 0;
float smoothedX = 0;
// alpha determines smoothness. 0.1 is very smooth (but slight lag). 0.5 is very fast (but more jitter).
const float filterAlpha = 0.2; 
const float deadzone = 3.5; // Increased slightly to ignore hand tremors

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize Touch Pins
  pinMode(PIN_LEFT_CLICK, INPUT);
  pinMode(PIN_RIGHT_CLICK, INPUT);
  pinMode(PIN_SCROLL_UP, INPUT);
  pinMode(PIN_SCROLL_DOWN, INPUT);
  pinMode(PIN_ACTIVATOR, INPUT);

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
    // 1. GYROSCOPE MOVEMENT (Only if Activator is touched)
    // ==========================================
    if (digitalRead(PIN_ACTIVATOR) == HIGH) {
      xyzFloat gyroData = mySensor.getGyrValues();

      // Apply Low-Pass Filter to smooth out the raw data
      smoothedZ = (gyroData.z * filterAlpha) + (smoothedZ * (1.0 - filterAlpha));
      smoothedX = (gyroData.x * filterAlpha) + (smoothedX * (1.0 - filterAlpha));

      // Map Z-axis (twist) to horizontal movement
      if (abs(smoothedZ) > deadzone) { 
        moveX = (int)(smoothedZ / 2.0); 
      }
      
      // Map X-axis (tilt) to vertical movement
      if (abs(smoothedX) > deadzone) { 
        moveY = (int)(smoothedX / 2.0); 
      }
    } else {
      // If activator is released, instantly reset the filter to prevent drift when re-engaged
      smoothedZ = 0;
      smoothedX = 0;
    }

    // ==========================================
    // 2. TOUCH SENSOR CLICKS 
    // ==========================================
    bool currentLeftState = digitalRead(PIN_LEFT_CLICK);
    bool currentRightState = digitalRead(PIN_RIGHT_CLICK);

    if (currentLeftState == HIGH && lastLeftState == LOW) {
      bleMouse.press(MOUSE_LEFT);
    } else if (currentLeftState == LOW && lastLeftState == HIGH) {
      bleMouse.release(MOUSE_LEFT);
    }
    lastLeftState = currentLeftState; 

    if (currentRightState == HIGH && lastRightState == LOW) {
      bleMouse.press(MOUSE_RIGHT);
    } else if (currentRightState == LOW && lastRightState == HIGH) {
      bleMouse.release(MOUSE_RIGHT);
    }
    lastRightState = currentRightState; 

    // ==========================================
    // 3. TOUCH SENSOR SCROLLING
    // ==========================================
    if (millis() - lastScrollTime > scrollInterval) {
      int scrollAmount = 0;
      
      if (digitalRead(PIN_SCROLL_UP) == HIGH) {
        scrollAmount = 1;
      } else if (digitalRead(PIN_SCROLL_DOWN) == HIGH) {
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