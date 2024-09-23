
#include <Arduino.h>
#include "Seeed_Arduino_mmWave.h"
#include <Adafruit_NeoPixel.h>

// Set up serial communication depending on the board type
#ifdef ESP32
#  include <HardwareSerial.h>
HardwareSerial mmWaveSerial(0);
#else
#  define mmWaveSerial Serial1
#endif

SEEED_MR60BHA2 mmWave;

// Pin definition for NeoPixel
const int pixelPin = D1;

// Create NeoPixel object
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, pixelPin, NEO_GRB + NEO_KHZ800);

// Structure to hold mmWave device data
typedef struct mmWaveDeviceData{
    float breath_rate;
    float heart_rate;
    float distance;
    unsigned long lastValidDataTime;
    int presenceStatus;
} mmWaveDeviceData;

mmWaveDeviceData mmWaveDeviceDataLocal;

// Define timeout duration
const unsigned long timeoutDuration = 10000;  // 10 seconds


void mmWaveDeviceSensorDriver(){
  mmWave.begin(&mmWaveSerial);
  pixels.begin();
  pixels.clear();
  pixels.show();
}

int detectPresence() {
    // Update the mmWave data
    if (mmWave.update(100)) {
        bool validData = false;

        // Check breath rate
        if (mmWave.getBreathRate(mmWaveDeviceDataLocal.breath_rate) && mmWaveDeviceDataLocal.breath_rate > 0) {
            Serial.printf("breath_rate: %.2f\n", mmWaveDeviceDataLocal.breath_rate);
            validData = true;
        }

        // Check heart rate
        if (mmWave.getHeartRate(mmWaveDeviceDataLocal.heart_rate) && mmWaveDeviceDataLocal.heart_rate > 0) {
            Serial.printf("heart_rate: %.2f\n", mmWaveDeviceDataLocal.heart_rate);
            validData = true;
        }

        // Check distance
        if (mmWave.getDistance(mmWaveDeviceDataLocal.distance) && mmWaveDeviceDataLocal.distance > 0) {
            Serial.printf("distance: %.2f\n", mmWaveDeviceDataLocal.distance);
            validData = true;
        }

        // Control LED and return signal based on sensor readings
        if (validData) {
            // Update the last valid data time
            mmWaveDeviceDataLocal.lastValidDataTime = millis();
            // Light blue LED if valid data is present
            pixels.setPixelColor(0, pixels.Color(0, 0, 125));  // Dim blue
            pixels.show();
            return 1;  // Presence detected
        } else {
            // Check if 10 seconds have passed since the last valid data
            if (millis() - mmWaveDeviceDataLocal.lastValidDataTime >= timeoutDuration) {
                // Light red LED if no valid data for 10 seconds
                pixels.setPixelColor(0, pixels.Color(125, 0, 0));  // Dim red
                pixels.show();
                return 0;  // No presence detected
            }
        }
    }
    return -1;  // Data not updated yet
}

void detection_signal(){
  mmWaveDeviceDataLocal.presenceStatus = detectPresence();
  if (mmWaveDeviceDataLocal.presenceStatus == 1) {
      Serial.println("Presence detected.");
  } else if (mmWaveDeviceDataLocal.presenceStatus == 0) {
      Serial.println("No presence detected.");
  }
}

void setup(){
  Serial.begin(115200);
  mmWaveDeviceSensorDriver();
}

void loop(){
  detection_signal();
}
