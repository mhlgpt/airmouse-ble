#include <Wire.h>
#include <BleMouse.h>

BleMouse bleMouse("AirMouse", "ESP32", 100);

const int MPU_ADDR = 0x68;
const int SCREEN_W = 1920;
const int SCREEN_H = 1080;
const float SENSITIVITY = 1200.0;
const float SMOOTHING = 0.85;
const float DEADZONE = 0.30;

long gyroOffsetX = 0, gyroOffsetY = 0, gyroOffsetZ = 0;
float smoothVX = 0, smoothVY = 0;
float remX = 0, remY = 0;

// --- NATIVE TOUCH PINS ---
const int PIN_SCROLL_UP   = 13;
const int PIN_SCROLL_DOWN = 14;
const int PIN_ACTIVATOR   = 12;
const int PIN_LEFT_CLICK  = 32;
const int PIN_RIGHT_CLICK = 33;

// Touch threshold (adjust if too sensitive or not sensitive enough)
const int TOUCH_THRESHOLD = 30;

bool leftState = false, rightState = false;

// Scroll timing variables
unsigned long lastScrollTime = 0;
const int scrollInterval = 100;

void readGyro(int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  gx = (int16_t)(Wire.read() << 8 | Wire.read());
  gy = (int16_t)(Wire.read() << 8 | Wire.read());
  gz = (int16_t)(Wire.read() << 8 | Wire.read());
}

void calibrateSensor() {
  long sx = 0, sy = 0, sz = 0;
  int16_t gx, gy, gz;
  for (int i = 0; i < 300; i++) {
    readGyro(gx, gy, gz);
    sx += gx; sy += gy; sz += gz;
    delay(2);
  }
  gyroOffsetX = sx / 300;
  gyroOffsetY = sy / 300;
  gyroOffsetZ = sz / 300;
  smoothVX = smoothVY = 0;
  remX = remY = 0;
}

void moveCursorToCenter() {
  for (int i = 0; i < 60; i++) {
    bleMouse.move(-127, -127);
    delay(8);
  }
  delay(100);
  int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
  while (cx > 0 || cy > 0) {
    int mx = min(cx, 100), my = min(cy, 100);
    bleMouse.move(mx, my);
    cx -= mx; cy -= my;
    delay(8);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Assuming you are still using default I2C pins for this specific script
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  delay(500);
  
  Serial.println("Calibrating on startup... leave flat!");
  calibrateSensor();
  Serial.println("Starting Bluetooth...");
  bleMouse.begin();
}

void loop() {
  if (bleMouse.isConnected()) {
    int mx = 0, my = 0;
    
    // ==========================================
    // 1. GYRO MOVEMENT (Controlled by Activator)
    // ==========================================
    if (touchRead(PIN_ACTIVATOR) < TOUCH_THRESHOLD) {
      int16_t GyX, GyY, GyZ;
      readGyro(GyX, GyY, GyZ);
      float rotX = GyX - gyroOffsetX;
      float rotY = GyY - gyroOffsetY;
      float rotZ = GyZ - gyroOffsetZ;
      
      float vx_raw = (rotX - rotZ) / SENSITIVITY;
      float vy_raw = rotY / SENSITIVITY;
      
      smoothVX = smoothVX * SMOOTHING + vx_raw * (1.0 - SMOOTHING);
      smoothVY = smoothVY * SMOOTHING + vy_raw * (1.0 - SMOOTHING);
      
      float ox = (abs(smoothVX) < DEADZONE) ? 0 : smoothVX;
      float oy = (abs(smoothVY) < DEADZONE) ? 0 : smoothVY;
      
      if (ox == 0 && oy == 0) {
        remX = remY = 0;
      } else {
        remX += ox; remY += oy;
        mx = (int)remX; 
        my = (int)remY;
        remX -= mx; 
        remY -= my;
      }
    } else {
      // If activator is released, reset smoothers to prevent jumping when touched again
      smoothVX = smoothVY = 0;
      remX = remY = 0;
    }

    // ==========================================
    // 2. SCROLLING LOGIC
    // ==========================================
    int scrollAmount = 0;
    if (millis() - lastScrollTime > scrollInterval) {
      if (touchRead(PIN_SCROLL_UP) < TOUCH_THRESHOLD) {
        scrollAmount = 1;
      } else if (touchRead(PIN_SCROLL_DOWN) < TOUCH_THRESHOLD) {
        scrollAmount = -1;
      }
      if (scrollAmount != 0) {
        lastScrollTime = millis();
      }
    }

    // Send the combined movement and scroll command
    if (mx != 0 || my != 0 || scrollAmount != 0) {
      bleMouse.move(constrain(mx, -127, 127), constrain(my, -127, 127), scrollAmount);
    }

    // ==========================================
    // 3. CLICKING LOGIC
    // ==========================================
    bool currentLeftState = (touchRead(PIN_LEFT_CLICK) < TOUCH_THRESHOLD);
    if (currentLeftState) {
      if (!leftState) { 
        bleMouse.press(MOUSE_LEFT); 
        leftState = true; 
      }
    } else if (leftState) { 
      bleMouse.release(MOUSE_LEFT); 
      leftState = false; 
    }

    bool currentRightState = (touchRead(PIN_RIGHT_CLICK) < TOUCH_THRESHOLD);
    if (currentRightState) {
      if (!rightState) { 
        bleMouse.press(MOUSE_RIGHT); 
        rightState = true; 
      }
    } else if (rightState) { 
      bleMouse.release(MOUSE_RIGHT); 
      rightState = false; 
    }
  }
  
  delay(10);
}