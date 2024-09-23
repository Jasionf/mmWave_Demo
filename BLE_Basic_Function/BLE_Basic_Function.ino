#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

int scanTime = 5;  
BLEScan *pBLEScan;
BLEAdvertisedDevice *myDevice; 

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
  MyAdvertisedDeviceCallbacks(const String &address) : inputAddress(address) {}

  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    
    // Check the device address
    if (advertisedDevice.getAddress().toString() == inputAddress) {
      Serial.println("Found my device! Connecting...");
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      pBLEScan->stop(); 
    }
  }

private:
  const String &inputAddress; 

void BLE_SCAN_CONFIG() {
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();  
  pBLEScan->setActiveScan(true);  
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  
}

bool isValidMacAddress(const String& address) {
  return address.length() == 17 && address.indexOf(':') > -1;
}

void BLE_CONNECT() {
  if (myDevice != nullptr) {
    Serial.printf("Connecting to device: %s\n", myDevice->toString().c_str());
    
    BLEClient *pClient = BLEDevice::createClient();
    unsigned long connectStartTime = millis();  
    
    while (millis() - connectStartTime < 10000) {
      if (pClient->connect(myDevice)) {
        Serial.println("Connected to device!");
        
  
        pClient->disconnect();
        Serial.println("Disconnected from device.");
        delete pClient;  
        return;  
      }
    }
    
    Serial.println("Connection timed out! Please input the address again.");
    myDevice = nullptr;  
    delete pClient;  
  } else {
    Serial.println("No device found to connect.");
  }
}

void BLE_SCAN_DRIVER(const String &inputAddress) {
  Serial.println("Starting BLE scan...");
  MyAdvertisedDeviceCallbacks callbacks(inputAddress);
  pBLEScan->setAdvertisedDeviceCallbacks(&callbacks);
  
  pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(pBLEScan->getResults()->getCount());
  Serial.println("Scan done!");
  pBLEScan->clearResults(); 
  delay(2000);
}

void scanAndConnect(const String &address) {
  if (!isValidMacAddress(address)) {
    Serial.println("Invalid address format. Please input again in format E0:6D:17:7B:7D:A7:");
    return;
  }
  
  BLE_SCAN_CONFIG();
  BLE_SCAN_DRIVER(address);
  BLE_CONNECT();
}

void Input_BLEAddress(){
    if (Serial.available() > 0) {
    String inputAddress = Serial.readStringUntil('\n');  
    inputAddress.trim();  
  
    
    scanAndConnect(inputAddress);
  }
}

void setup(){
  Serial.begin(115200);
}


void loop(){
  Input_BLEAddress();
  delay(500);
}
