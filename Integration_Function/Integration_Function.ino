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
  bool mmWaveDevice_detection_Status;//是否检测到人
  bool BLEDviec_Connect_Status;//是否连接了蓝牙设备
}mmWaveDeviceESPNOW;

mmWaveDeviceESPNOW mmWaveDeviceESPNOW_A;

mmWaveDeviceESPNOW mmWaveDeviceESPNOW_B;

typedef struct global_variable{
  bool global_mmWaveDevice_detection_Status;//是否检测到人
  bool global_BLEDviec_Connect_Status;//是否连接了蓝牙设备,如果有一个设备连接到蓝牙地址，则全部都开灯
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

// 链表节点结构
struct MacNode {
    uint8_t macAddress[MAX_ESP_NOW_MAC_LEN]; // 存储MAC地址
    MacNode* next; // 指向下一个节点
};

// 函数声明
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


// 全局变量
MacNode* head = nullptr; // 链表头指针
esp_now_peer_info_t peerInfo;  // 对等设备信息
char inputBuffer[MAX_INPUT_LENGTH]; // 输入缓冲区
int inputIndex = 0; // 输入索引
uint8_t macArray[MAX_ESP_NOW_MAC_LEN]; // 存储转换后的MAC地址

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
    
    // 打印 MAC 地址为 XX:XX:XX:XX:XX:XX 格式
    Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// 发送数据回调函数
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// 接收数据回调函数
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

// 初始化 ESP-NOW 并添加对等设备
bool initializeEspNow(const String &macAddress) {
    WiFi.mode(WIFI_STA);  // 设置为 Station 模式

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return false;
    }

    esp_now_register_send_cb(OnDataSent);  // 注册发送数据的回调函数
    esp_now_register_recv_cb(OnDataReceived); // 注册接收数据的回调函数

    // 解析输入的 MAC 地址
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

    peerInfo.channel = 0;  // 使用当前打开的通道
    peerInfo.encrypt = false;  // 未加密

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return false;
    }

    return true;
}

// 发送数据
void sendData() {

  mmWaveDeviceESPNOW_A.mmWaveDevice_detection_Status = detectPresence();//发送毫米波雷达结果true代表有人
  mmWaveDeviceESPNOW_A.BLEDviec_Connect_Status = BLE_CONNECT();//发送蓝牙连接结果，true代表连接成功

  esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *)&mmWaveDeviceESPNOW_A, sizeof(mmWaveDeviceESPNOW_A));  // 发送

  if (result == ESP_OK) {
      Serial.println("Sent with success");
  } else {
      Serial.println("Error sending the data");
  }
}

// 添加 MAC 地址节点
void addMacNode(uint8_t mac[]) {
    MacNode* newNode = new MacNode; // 创建新节点
    memcpy(newNode->macAddress, mac, MAX_ESP_NOW_MAC_LEN); // 复制MAC地址
    newNode->next = head; // 新节点指向当前头节点
    head = newNode; // 更新头节点
}

// 打印链表中的 MAC 地址
void printMacList() {
    MacNode* current = head; // 从头节点开始
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
        current = current->next; // 移动到下一个节点
    }
}

// 根据序列号删除节点
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
            return; // 序列号无效
        }
        previous = current;
        current = current->next; // 移动到下一个节点
    }

    // 删除节点
    if (previous == nullptr) {
        // 删除头节点
        head = current->next;
    } else {
        previous->next = current->next; // 跳过当前节点
    }
    delete current; // 释放内存
    Serial.println("节点已删除。");
}

// 输入序列号并删除节点
void inputDeleteMacAddress() {
    printMacList(); // 打印当前链表
    Serial.print("请输入要删除的节点序列号: ");
    while (true) {
        if (Serial.available() > 0) {
            int index = Serial.parseInt(); // 读取序列号
            deleteMacNode(index); // 删除节点
            return; // 退出函数
        }
    }
}

// 清理缓存
void clearCache(char* information) {
    memset(information, 0, MAX_INPUT_LENGTH); // 将整个缓冲区清零
    inputIndex = 0; // 重置索引
}

// 将输入的数据转换为 uint8_t 格式并添加到链表
uint8_t* copyArray(char* input) {
    // 清空 macArray
    memset(macArray, 0, sizeof(macArray));

    int bytesCopied = 0;
    char* token = strtok(input, ","); // 使用 strtok 分割用户输入的字符串

    while (token != NULL && bytesCopied < MAX_ESP_NOW_MAC_LEN) {
        macArray[bytesCopied++] = strtol(token, NULL, 0); // 转换为 uint8_t 并存储
        token = strtok(NULL, ","); 
    }

    Serial.print("MAC Array: ");
    for (int i = 0; i < bytesCopied; i++) {
        Serial.print("0x");
        Serial.print(macArray[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    addMacNode(macArray); // 添加到链表中
    printMacList(); // 打印链表
    return macArray;
}

void inputMacAddress() {
    clearCache(inputBuffer);
    inputIndex = 0; // 重置输入索引
    
    Serial.println("Enter the MAC address of the target ESP32 (format: XX:XX:XX:XX:XX:XX):");

    while (true) {
        if (Serial.available() > 0) {
            char incomingByte = Serial.read(); 

            // 检查是否接收到换行符
            if (incomingByte == '\n') {
                inputBuffer[inputIndex] = '\0';
                Serial.print("Your entered MAC address is: ");
                Serial.println(inputBuffer); 
                Serial.println("Are you sure (y/n)?");

                // 处理确认输入
                while (true) {
                    if (Serial.available() > 0) {
                        char confirmation = Serial.read();
                        if (confirmation == 'y') {
                            Serial.println("Data is saved successfully.");
                            copyArray(inputBuffer);
                            clearCache(inputBuffer);

                            // 调用初始化 ESP-NOW
                            if (initializeEspNow(inputBuffer)) {  // 初始化并添加对等设备
                                Serial.print("Connected to: ");
                                for (int i = 0; i < 6; i++) {
                                    Serial.printf("%02X", peerInfo.peer_addr[i]);
                                    if (i < 5) Serial.print(":");
                                }
                                Serial.println();
                            } else {
                                Serial.println("Failed to initialize ESP-NOW.");
                            }
                            return; // 结束输入函数
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
// 获取用户输入的 MAC 地址
String getMacAddressInput() {
    Serial.println("Enter the MAC address of the target ESP32 (format: XX:XX:XX:XX:XX:XX):");
    String macInput;

    while (true) {
        if (Serial.available()) {
            macInput = Serial.readStringUntil('\n');  
            macInput.trim(); 
            return macInput;  // 返回获取到的 MAC 地址
        }
    }
}

// 主初始化和发送函数
void setupCommunication() {
    String macInput = getMacAddressInput(); // 这里可以根据需要设置一个默认的 MAC 地址
    if (initializeEspNow(macInput)) {  // 初始化并添加对等设备
        Serial.print("Connected to: ");
        for (int i = 0; i < 6; i++) {
          Serial.printf("%02X", peerInfo.peer_addr[i]);
          if (i < 5) Serial.print(":");
        }
        Serial.println();
    }
}

// BLE 扫描回调类
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    MyAdvertisedDeviceCallbacks(const String &address) : inputAddress(address) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
        if (advertisedDevice.getAddress().toString() == inputAddress) {
            Serial.println("Found my device! Connecting...");
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            pBLEScan->stop();  // 停止扫描
        }
    }

private:
    const String &inputAddress;  // 使用常量引用
};

// BLE扫描配置
void BLE_SCAN_CONFIG() {
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();  // 创建新扫描
    pBLEScan->setActiveScan(true);  // 主动扫描
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  // 小于或等于 setInterval 值
}

// 检查 MAC 地址格式
bool isValidMacAddress(const String& address) {
    return address.length() == 17 && address.indexOf(':') > -1;
}

// 连接到 BLE 设备
bool BLE_CONNECT() {
    if (myDevice != nullptr) {
        Serial.printf("Connecting to device: %s\n", myDevice->toString().c_str());
        
        BLEClient *pClient = BLEDevice::createClient();
        unsigned long connectStartTime = millis();  // 记录连接开始时间
        
        while (millis() - connectStartTime < 10000) {
            if (pClient->connect(myDevice)) {
                Serial.println("Connected to device!");
                return true;
                pClient->disconnect();  // 断开连接
                Serial.println("Disconnected from device.");
                delete pClient;  // 释放客户端
            }
        }
        
        Serial.println("Connection timed out! Please input the address again.");
        myDevice = nullptr;  // 重置设备
        delete pClient;  // 确保客户端被释放
    } else {
        Serial.println("No device found to connect.");
        return false;
    }
}

// 执行 BLE 扫描和连接
void BLE_SCAN_DRIVER(const String &inputAddress) {
    Serial.println("Starting BLE scan...");
    MyAdvertisedDeviceCallbacks callbacks(inputAddress);
    pBLEScan->setAdvertisedDeviceCallbacks(&callbacks);
    
    pBLEScan->start(5, false);  // 扫描 5 秒
    Serial.print("Devices found: ");
    Serial.println(pBLEScan->getResults()->getCount());
    Serial.println("Scan done!");
    pBLEScan->clearResults();  // 清除结果
    delay(2000);
}

// 扫描并连接到设备
void scanAndConnect(const String &address) {
    if (!isValidMacAddress(address)) {
        Serial.println("Invalid address format. Please input again in format E0:6D:17:7B:7D:A7:");
        return;
    }
    
    BLE_SCAN_CONFIG();
    BLE_SCAN_DRIVER(address);
    BLE_CONNECT();
}

// 打印保存的 BLE 地址
void printBLEAddresses() {
    Serial.println("Saved BLE Addresses:");
    BLEAddressNode* current = bleHead;
    int index = 0;
    while (current != nullptr) {
        Serial.printf("Index %d: %s\n", index++, current->address.c_str());
        current = current->next;
    }
}

// 删除指定索引的 BLE 地址
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
        bleHead = current->next;  // 删除头节点
    } else {
        previous->next = current->next;  // 删除中间或尾节点
    }
    
    Serial.printf("Deleted address at index %d: %s\n", index, current->address.c_str());
    delete current;  // 释放内存
}

// 按字母顺序插入 BLE 地址
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

// 输入 BLE 地址
void Input_BLEAddress() {
    if (Serial.available() > 0) {
        String inputAddress = Serial.readStringUntil('\n');  // 读取用户输入的地址
        inputAddress.trim();  // 去除多余空格
        
        if (inputAddress.equalsIgnoreCase("check")) {
            printBLEAddresses();  // 打印链表中的地址
        } else if (inputAddress.equalsIgnoreCase("delete")) {
            printBLEAddresses();  // 打印链表中的地址以供用户选择
            Serial.println("Enter index to delete:");
            while (!Serial.available());  // 等待输入
            int index = Serial.parseInt();  // 读取索引
            deleteBLEAddress(index);  // 删除对应地址
        } else {
            scanAndConnect(inputAddress);  // 执行扫描和连接
            insertBLEAddressSorted(inputAddress);  // 将有效地址添加到链表
            Serial.printf("Address %s added to the list.\n", inputAddress.c_str());
        }
    }
}

// 按钮信号处理
void BUTTON_SIGNA() {
    int buttonStateA = digitalRead(buttonPinA);
    if (buttonStateA == 1) {
      mmWaveDeviceLocal_MACAddress_Requir();//输入MAC地址，自动ESP-NOW组网
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
      Input_BLEAddress();//输入BLE地址，绑定设备
      delay(200);
    }
}

void BUTTON_SIGND() {
    int buttonStateD = digitalRead(buttonPinD);
    if (buttonStateD == 1) {
      Serial.println("Please input the Bluetooth address in format E0:6D:17:7B:7D:A7:");
      Input_BLEAddress();//输入BLE地址，绑定设备
      delay(200);
    }
}

// mmWave 设备传感器驱动
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
    while (!Serial.available()); // 等待用户输入
    return Serial.parseFloat(); // 读取并返回偏移量
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
    // 这里可以通过用户输入或其他方式获取校准值
    mmWaveDeviceDataLocal.breathRateOffset = getUserBreathRateOffset();  // 用户输入呼吸率偏移
    mmWaveDeviceDataLocal.heartRateOffset = getUserHeartRateOffset();    // 用户输入心率偏移
    mmWaveDeviceDataLocal.distanceOffset = getUserDistanceOffset();      // 用户输入距离偏移
}


int detectPresence() {
    // Update the mmWave data
    if (mmWave.update(100)) {
        bool validData = false;

        // Check breath rate
        if (mmWave.getBreathRate(mmWaveDeviceDataLocal.breath_rate) && mmWaveDeviceDataLocal.breath_rate > 0) {
            mmWaveDeviceDataLocal.breath_rate += mmWaveDeviceDataLocal.breathRateOffset;  // 应用偏移
            if (mmWaveDeviceDataLocal.breath_rate >= breathRateValidRange[0] && mmWaveDeviceDataLocal.breath_rate <= breathRateValidRange[1]) {
                Serial.printf("breath_rate: %.2f\n", mmWaveDeviceDataLocal.breath_rate);
                validData = true;
            }
        }

        // Check heart rate
        if (mmWave.getHeartRate(mmWaveDeviceDataLocal.heart_rate) && mmWaveDeviceDataLocal.heart_rate > 0) {
            mmWaveDeviceDataLocal.heart_rate += mmWaveDeviceDataLocal.heartRateOffset;  // 应用偏移
            if (mmWaveDeviceDataLocal.heart_rate >= heartRateValidRange[0] && mmWaveDeviceDataLocal.heart_rate <= heartRateValidRange[1]) {
                Serial.printf("heart_rate: %.2f\n", mmWaveDeviceDataLocal.heart_rate);
                validData = true;
            }
        }

        // Check distance
        if (mmWave.getDistance(mmWaveDeviceDataLocal.distance) && mmWaveDeviceDataLocal.distance > 0) {
            mmWaveDeviceDataLocal.distance += mmWaveDeviceDataLocal.distanceOffset;  // 应用偏移
            if (mmWaveDeviceDataLocal.distance >= distanceValidRange[0] && mmWaveDeviceDataLocal.distance <= distanceValidRange[1]) {
                Serial.printf("distance: %.2f\n", mmWaveDeviceDataLocal.distance);
                validData = true;
            }
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
    BUTTON_SIGNA();//获取MAC地址
    // setupCommunication();  // 初始化通信
    // 初始化 mmWave 传感器
    // mmWaveDeviceSensorDriver();
    Serial.println("Setup complete. Please input Bluetooth address.");
}

void loop() {
    sendData();  // 发送数据
    Input_BLEAddress();  // 持续监听用户输入
    BUTTON_SIGNA();//获取MAC地址
    BUTTON_SIGNB();//添加MAC地址
    BUTTON_SIGNC();//连接BLE
    // BUTTON_SIGND();
    // detection_signal();  // 检测信号
    delay(5000); // 每5秒发送一次数据
}