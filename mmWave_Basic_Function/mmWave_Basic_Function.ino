
#include <Arduino.h>
#include "Seeed_Arduino_mmWave.h"
#include <Adafruit_NeoPixel.h>


#ifdef ESP32
#  include <HardwareSerial.h>
HardwareSerial mmWaveSerial(0);
#else
#  define mmWaveSerial Serial1
#endif

SEEED_MR60BHA2 mmWave;


const int pixelPin = D1;


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, pixelPin, NEO_GRB + NEO_KHZ800);


typedef struct mmWaveDeviceData{
    float breath_rate;
    float heart_rate;
    float distance;
    unsigned long lastValidDataTime;
    int presenceStatus;
} mmWaveDeviceData;

mmWaveDeviceData mmWaveDeviceDataLocal;


const unsigned long timeoutDuration = 10000; 


void mmWaveDeviceSensorDriver(){
  mmWave.begin(&mmWaveSerial);
  pixels.begin();
  pixels.clear();
  pixels.show();
}

int detectPresence() {

    if (mmWave.update(100)) {
        bool validData = false;


        if (mmWave.getBreathRate(mmWaveDeviceDataLocal.breath_rate) && mmWaveDeviceDataLocal.breath_rate > 0) {
            Serial.printf("breath_rate: %.2f\n", mmWaveDeviceDataLocal.breath_rate);
            validData = true;
        }


        if (mmWave.getHeartRate(mmWaveDeviceDataLocal.heart_rate) && mmWaveDeviceDataLocal.heart_rate > 0) {
            Serial.printf("heart_rate: %.2f\n", mmWaveDeviceDataLocal.heart_rate);
            validData = true;
        }


        if (mmWave.getDistance(mmWaveDeviceDataLocal.distance) && mmWaveDeviceDataLocal.distance > 0) {
            Serial.printf("distance: %.2f\n", mmWaveDeviceDataLocal.distance);
            validData = true;
        }


        if (validData) {

            mmWaveDeviceDataLocal.lastValidDataTime = millis();

            pixels.setPixelColor(0, pixels.Color(0, 0, 125));  
            pixels.show();
            return 1;  
        } else {
            if (millis() - mmWaveDeviceDataLocal.lastValidDataTime >= timeoutDuration) {
                pixels.setPixelColor(0, pixels.Color(125, 0, 0)); 
                pixels.show();
                return 0; 
            }
        }
    }
    return -1;  
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
