#include "Seeed_Arduino_mmWave.h"
#include <Adafruit_NeoPixel.h>
#include <hp_BH1750.h>
#include "esp_now.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "WiFi.h"
#include <string>

// 定义常量
#define ESPNOW_WIFI_CHANNEL 0
#define NO_PMK_KEY false
#define BAUD 115200
#define MAX_CHARACTERS_NUMBER 20

#ifdef ESP32
#  include <HardwareSerial.h>
HardwareSerial mmWaveSerial(0);
#else
#  define mmWaveSerial Serial1
#endif
const int MAX_INPUT_LENGTH = 32; 
const int ARRAY_SIZE = 6; 
const int scanTime = 5; 
const int MAX_ESP_NOW_MAC_LEN = 6;
const int pixelPin = D1;
// 按钮引脚定义
const int buttonPinA = 0;
const int buttonPinB = 1;
const int buttonPinC = 2;
const int buttonPinD = 5;
const unsigned long timeoutDuration = 10000;  // 10 seconds

// 校准范围
const float breathRateValidRange[] = {10.0, 30.0};  
const float heartRateValidRange[] = {40.0, 180.0};  
const float distanceValidRange[] = {0.0, 5.0};  

// BLE链表节点结构
struct BLEAddressNode {
  String address;
  BLEAddressNode* next;
};

// BLE链表头
BLEAddressNode* bleHead = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
BLEScan* pBLEScan = nullptr;
SEEED_MR60BHA2 mmWave;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, pixelPin, NEO_GRB + NEO_KHZ800);


typedef struct mmWaveDeviceESPNOW{
  char BLE_ADDRESS[MAX_CHARACTERS_NUMBER];
  uint8_t MAC_Address[MAX_CHARACTERS_NUMBER];
  bool mmWaveDevice_detection_Status;
  bool BLEDviec_Connect_Status;
}mmWaveDeviceESPNOW;

mmWaveDeviceESPNOW mmWaveDeviceESPNOW_A;

mmWaveDeviceESPNOW mmWaveDeviceESPNOW_B;

typedef struct global_variable{
  bool global_mmWaveDevice_detection_Status;
  bool global_BLEDviec_Connect_Status;
}global_variable;

global_variable global_variable_A;

typedef struct mmWaveDeviceData{
    float breath_rate;
    float heart_rate;
    float distance;
    unsigned long lastValidDataTime;
    int presenceStatus;
    float breathRateOffset;
    float heartRateOffset;
    float distanceOffset;
} mmWaveDeviceData;

mmWaveDeviceData mmWaveDeviceDataLocal;


struct MacNode {
    uint8_t macAddress[MAX_ESP_NOW_MAC_LEN]; 
    MacNode* next; 
};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void OnDataReceived(const esp_now_recv_info* info, const uint8_t* incomingDataBuffer, int len);
bool initializeEspNow(const String &macAddress);
void sendData();
void addMacNode(uint8_t mac[]);
void printMacList();
void deleteMacNode(int index);
void inputDeleteMacAddress();
void clearCache(char* information);
uint8_t* copyArray(char* input);
void inputMacAddress();
void setupCommunication();


MacNode* head = nullptr; 
esp_now_peer_info_t peerInfo;  
char inputBuffer[MAX_INPUT_LENGTH]; 
int inputIndex = 0; 
uint8_t macArray[MAX_ESP_NOW_MAC_LEN]; 

void mmWaveDeviceLocal_MACAddress_Requir() {
    WiFi.mode(WIFI_STA);
    WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
    uint8_t mac[MAX_CHARACTERS_NUMBER];
    
    while (!WiFi.STA.started()) {
        Serial.print(".");
        delay(100);
    }
    
    WiFi.macAddress(mac);
    Serial.println();
    
    Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


void OnDataReceived(const esp_now_recv_info* info, const uint8_t* incomingDataBuffer, int len) {
    memcpy(&mmWaveDeviceESPNOW_A, incomingDataBuffer, sizeof(mmWaveDeviceESPNOW_A));
    Serial.print("Received data from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", info->src_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.print(mmWaveDeviceESPNOW_B.mmWaveDevice_detection_Status);
    Serial.print(mmWaveDeviceESPNOW_B.BLEDviec_Connect_Status);
}


bool initializeEspNow(const String &macAddress) {
    WiFi.mode(WIFI_STA);  

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    esp_now_register_send_cb(OnDataSent);  
    esp_now_register_recv_cb(OnDataReceived); 

   
    int macIndex = 0;
    char *token = strtok((char *)macAddress.c_str(), ":");
    while (token != nullptr && macIndex < 6) {
        peerInfo.peer_addr[macIndex++] = strtol(token, nullptr, 16);
        token = strtok(nullptr, ":");
    }

    if (macIndex != 6) {
        Serial.println("Invalid MAC address format.");
        return false;
    }

    peerInfo.channel = 0;  
    peerInfo.encrypt = false;  

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return false;
    }

    return true;
}


void sendData() {

  mmWaveDeviceESPNOW_A.mmWaveDevice_detection_Status = detectPresence();
  mmWaveDeviceESPNOW_A.BLEDviec_Connect_Status = BLE_CONNECT();

  esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *)&mmWaveDeviceESPNOW_A, sizeof(mmWaveDeviceESPNOW_A));  

  if (result == ESP_OK) {
      Serial.println("Sent with success");
  } else {
      Serial.println("Error sending the data");
  }
}


void addMacNode(uint8_t mac[]) {
    MacNode* newNode = new MacNode; 
    memcpy(newNode->macAddress, mac, MAX_ESP_NOW_MAC_LEN); 
    newNode->next = head; 
    head = newNode; 
}


void printMacList() {
    MacNode* current = head; 
    int index = 0;
    while (current != nullptr) {
        Serial.print("The list MAC address is: ");
        Serial.print("Index ");
        Serial.print(index++);
        Serial.print(": ");
        for (int i = 0; i < MAX_ESP_NOW_MAC_LEN; i++) {
            Serial.print("0x");
            Serial.print(current->macAddress[i], HEX);
            if (i < MAX_ESP_NOW_MAC_LEN - 1) {
                Serial.print(":");
            }
        }
        Serial.println();
        current = current->next; 
    }
}


void deleteMacNode(int index) {
    if (head == nullptr) {
        Serial.println("链表为空，无法删除。");
        return;
    }

    MacNode* current = head;
    MacNode* previous = nullptr;

    for (int i = 0; i < index; i++) {
        if (current == nullptr) {
            Serial.println("无效的序列号。");
            return;
        }
        previous = current;
        current = current->next;
    }


    if (previous == nullptr) {
        head = current->next;
    } else {
        previous->next = current->next; 
    }
    delete current; 
    Serial.println("节点已删除。");
}

void inputDeleteMacAddress() {
    printMacList();
    Serial.print("请输入要删除的节点序列号: ");
    while (true) {
        if (Serial.available() > 0) {
            int index = Serial.parseInt(); 
            deleteMacNode(index); 
            return;
        }
    }
}


void clearCache(char* information) {
    memset(information, 0, MAX_INPUT_LENGTH);
    inputIndex = 0; 
}


uint8_t* copyArray(char* input) {

    memset(macArray, 0, sizeof(macArray));

    int bytesCopied = 0;
    char* token = strtok(input, ","); 

    while (token != NULL && bytesCopied < MAX_ESP_NOW_MAC_LEN) {
        macArray[bytesCopied++] = strtol(token, NULL, 0);
        token = strtok(NULL, ","); 
    }

    Serial.print("MAC Array: ");
    for (int i = 0; i < bytesCopied; i++) {
        Serial.print("0x");
        Serial.print(macArray[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    addMacNode(macArray); 
    printMacList(); 
    return macArray;
}

void inputMacAddress() {
    clearCache(inputBuffer);
    inputIndex = 0; 
    
    Serial.println("Enter the MAC address of the target ESP32 (format: XX:XX:XX:XX:XX:XX):");

    while (true) {
        if (Serial.available() > 0) {
            char incomingByte = Serial.read(); 

 
            if (incomingByte == '\n') {
                inputBuffer[inputIndex] = '\0';
                Serial.print("Your entered MAC address is: ");
                Serial.println(inputBuffer); 
                Serial.println("Are you sure (y/n)?");
入
                while (true) {
                    if (Serial.available() > 0) {
                        char confirmation = Serial.read();
                        if (confirmation == 'y') {
                            Serial.println("Data is saved successfully.");
                            copyArray(inputBuffer);
                            clearCache(inputBuffer);

                      
                            if (initializeEspNow(inputBuffer)) {  
                                Serial.print("Connected to: ");
                                for (int i = 0; i < 6; i++) {
                                    Serial.printf("%02X", peerInfo.peer_addr[i]);
                                    if (i < 5) Serial.print(":");
                                }
                                Serial.println();
                            } else {
                                Serial.println("Failed to initialize ESP-NOW.");
                            }
                            return; 
                        } else if (confirmation == 'n') {
                            Serial.println("Please re-enter your mmWave device MAC address:");
                            inputIndex = 0; 
                            inputBuffer[0] = '\0'; 
                            break;
                        }
                    }
                }
            } else {
                if (inputIndex < sizeof(inputBuffer) - 1) {
                    inputBuffer[inputIndex++] = incomingByte; 
                }
            }
        }
    }
}

String getMacAddressInput() {
    Serial.println("Enter the MAC address of the target ESP32 (format: XX:XX:XX:XX:XX:XX):");
    String macInput;

    while (true) {
        if (Serial.available()) {
            macInput = Serial.readStringUntil('\n');  
            macInput.trim(); 
            return macInput; 
        }
    }
}


void setupCommunication() {
    String macInput = getMacAddressInput();
    if (initializeEspNow(macInput)) { 
        Serial.print("Connected to: ");
        for (int i = 0; i < 6; i++) {
          Serial.printf("%02X", peerInfo.peer_addr[i]);
          if (i < 5) Serial.print(":");
        }
        Serial.println();
    }
}


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    MyAdvertisedDeviceCallbacks(const String &address) : inputAddress(address) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
        if (advertisedDevice.getAddress().toString() == inputAddress) {
            Serial.println("Found my device! Connecting...");
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            pBLEScan->stop(); 
        }
    }

private:
    const String &inputAddress;  
};


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


bool BLE_CONNECT() {
    if (myDevice != nullptr) {
        Serial.printf("Connecting to device: %s\n", myDevice->toString().c_str());
        
        BLEClient *pClient = BLEDevice::createClient();
        unsigned long connectStartTime = millis();  
        while (millis() - connectStartTime < 10000) {
            if (pClient->connect(myDevice)) {
                Serial.println("Connected to device!");
                return true;
                pClient->disconnect();  
                Serial.println("Disconnected from device.");
                delete pClient; 
            }
        }
        
        Serial.println("Connection timed out! Please input the address again.");
        myDevice = nullptr; 
        delete pClient;  
    } else {
        Serial.println("No device found to connect.");
        return false;
    }
}


void BLE_SCAN_DRIVER(const String &inputAddress) {
    Serial.println("Starting BLE scan...");
    MyAdvertisedDeviceCallbacks callbacks(inputAddress);
    pBLEScan->setAdvertisedDeviceCallbacks(&callbacks);
    
    pBLEScan->start(5, false);  
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


void printBLEAddresses() {
    Serial.println("Saved BLE Addresses:");
    BLEAddressNode* current = bleHead;
    int index = 0;
    while (current != nullptr) {
        Serial.printf("Index %d: %s\n", index++, current->address.c_str());
        current = current->next;
    }
}


void deleteBLEAddress(int index) {
    if (bleHead == nullptr) {
        Serial.println("No addresses to delete.");
        return;
    }
    BLEAddressNode* current = bleHead;
    BLEAddressNode* previous = nullptr;

    for (int i = 0; i < index; i++) {
        if (current == nullptr) {
            Serial.println("Index out of bounds.");
            return;
        }
        previous = current;
        current = current->next;
    }
    if (previous == nullptr) {
        bleHead = current->next; 
    } else {
        previous->next = current->next;  
    }
    
    Serial.printf("Deleted address at index %d: %s\n", index, current->address.c_str());
    delete current; 
}


void insertBLEAddressSorted(const String &address) {
    BLEAddressNode* newNode = new BLEAddressNode();
    newNode->address = address;
    newNode->next = nullptr;

    if (bleHead == nullptr || bleHead->address > address) {
        newNode->next = bleHead;
        bleHead = newNode;
    } else {
        BLEAddressNode* current = bleHead;
        while (current->next != nullptr && current->next->address < address) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
}


void Input_BLEAddress() {
    if (Serial.available() > 0) {
        String inputAddress = Serial.readStringUntil('\n'); 
        inputAddress.trim();  
        
        if (inputAddress.equalsIgnoreCase("check")) {
            printBLEAddresses();  
        } else if (inputAddress.equalsIgnoreCase("delete")) {
            printBLEAddresses();
            Serial.println("Enter index to delete:");
            while (!Serial.available());
            int index = Serial.parseInt();  
            deleteBLEAddress(index);  
        } else {
            scanAndConnect(inputAddress); 
            insertBLEAddressSorted(inputAddress);  
            Serial.printf("Address %s added to the list.\n", inputAddress.c_str());
        }
    }
}


void BUTTON_SIGNA() {
    int buttonStateA = digitalRead(buttonPinA);
    if (buttonStateA == 1) {
      mmWaveDeviceLocal_MACAddress_Requir();
      delay(200); 
    }
}

void BUTTON_SIGNB() {
    int buttonStateB = digitalRead(buttonPinB);
    if (buttonStateB == 1) {
      setupCommunication();
      delay(200); 
    }
}

void BUTTON_SIGNC() {
    int buttonStateC = digitalRead(buttonPinC);
    if (buttonStateC == 1) {
      Serial.println("Please input the Bluetooth address in format E0:6D:17:7B:7D:A7:");
      Input_BLEAddress();
      delay(200);
    }
}

void BUTTON_SIGND() {
    int buttonStateD = digitalRead(buttonPinD);
    if (buttonStateD == 1) {
      Serial.println("Please input the Bluetooth address in format E0:6D:17:7B:7D:A7:");
      Input_BLEAddress();
      delay(200);
    }
}


void mmWaveDeviceSensorDriver() {
    mmWave.begin(&mmWaveSerial);
    pixels.begin();
    pixels.clear();
    pixels.show();
        // 初始化偏移量
    mmWaveDeviceDataLocal.breathRateOffset = 0.0;
    mmWaveDeviceDataLocal.heartRateOffset = 0.0;
    mmWaveDeviceDataLocal.distanceOffset = 0.0;
}

float getUserBreathRateOffset() {
    Serial.println("Enter breath rate offset:");
    while (!Serial.available());
    return Serial.parseFloat(); 
}

float getUserHeartRateOffset() {
    Serial.println("Enter heart rate offset:");
    while (!Serial.available());
    return Serial.parseFloat();
}

float getUserDistanceOffset() {
    Serial.println("Enter distance offset:");
    while (!Serial.available());
    return Serial.parseFloat();
}

void calibrateData() {
   
    mmWaveDeviceDataLocal.breathRateOffset = getUserBreathRateOffset();
    mmWaveDeviceDataLocal.heartRateOffset = getUserHeartRateOffset();  
    mmWaveDeviceDataLocal.distanceOffset = getUserDistanceOffset();  
}


int detectPresence() {
  
    if (mmWave.update(100)) {
        bool validData = false;

      
        if (mmWave.getBreathRate(mmWaveDeviceDataLocal.breath_rate) && mmWaveDeviceDataLocal.breath_rate > 0) {
            mmWaveDeviceDataLocal.breath_rate += mmWaveDeviceDataLocal.breathRateOffset;  
            if (mmWaveDeviceDataLocal.breath_rate >= breathRateValidRange[0] && mmWaveDeviceDataLocal.breath_rate <= breathRateValidRange[1]) {
                Serial.printf("breath_rate: %.2f\n", mmWaveDeviceDataLocal.breath_rate);
                validData = true;
            }
        }

        
        if (mmWave.getHeartRate(mmWaveDeviceDataLocal.heart_rate) && mmWaveDeviceDataLocal.heart_rate > 0) {
            mmWaveDeviceDataLocal.heart_rate += mmWaveDeviceDataLocal.heartRateOffset;  
            if (mmWaveDeviceDataLocal.heart_rate >= heartRateValidRange[0] && mmWaveDeviceDataLocal.heart_rate <= heartRateValidRange[1]) {
                Serial.printf("heart_rate: %.2f\n", mmWaveDeviceDataLocal.heart_rate);
                validData = true;
            }
        }

        
        if (mmWave.getDistance(mmWaveDeviceDataLocal.distance) && mmWaveDeviceDataLocal.distance > 0) {
            mmWaveDeviceDataLocal.distance += mmWaveDeviceDataLocal.distanceOffset;  
            if (mmWaveDeviceDataLocal.distance >= distanceValidRange[0] && mmWaveDeviceDataLocal.distance <= distanceValidRange[1]) {
                Serial.printf("distance: %.2f\n", mmWaveDeviceDataLocal.distance);
                validData = true;
            }
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

// 检测信号
void detection_signal() {
    mmWaveDeviceDataLocal.presenceStatus = detectPresence();
    if (mmWaveDeviceDataLocal.presenceStatus == 1) {
        Serial.println("Presence detected.");
    } else if (mmWaveDeviceDataLocal.presenceStatus == 0) {
        Serial.println("No presence detected.");
    }
}

// 主设置和循环
void setup() {
    Serial.begin(115200);
    pinMode(buttonPinA, INPUT);
    pinMode(buttonPinB, INPUT);
    pinMode(buttonPinC, INPUT);
    // pinMode(buttonPinD, INPUT);
    BUTTON_SIGNA();
    // setupCommunication();  
    // mmWaveDeviceSensorDriver();
    Serial.println("Setup complete. Please input Bluetooth address.");
}

void loop() {
    sendData(); 
    Input_BLEAddress();  
    BUTTON_SIGNA();
    BUTTON_SIGNB();
    BUTTON_SIGNC();
    // BUTTON_SIGND();
    // detection_signal();  
    delay(5000); 
}
