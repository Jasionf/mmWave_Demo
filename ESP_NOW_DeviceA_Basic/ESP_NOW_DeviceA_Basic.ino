// #include <Arduino.h>
// #include <esp_now.h>
// #include <WiFi.h>




// // 发送的数据结构
// typedef struct struct_message {
//     char a[32];
//     int b;
//     float c;
//     bool d;
// } struct_message;

// struct_message myData;
// esp_now_peer_info_t peerInfo;

// // 发送数据回调函数
// void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
//     Serial.print("Last Packet Send Status: ");
//     Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
// }


// //接收数据回调函数
// void OnDataReceived(const esp_now_recv_info* info, const uint8_t* incomingDataBuffer, int len) {
//   memcpy(&incomingData, incomingDataBuffer, sizeof(incomingData));
//   Serial.print("Received data from: ");
//   for (int i = 0; i < 6; i++) {
//       Serial.printf("%02X", info->src_addr[i]);
//       if (i < 5) Serial.print(":");
//   }
//   Serial.print(" | a: ");
//   Serial.print(incomingData.a);
//   Serial.print(", b: ");
//   Serial.print(incomingData.b);
//   Serial.print(", c: ");
//   Serial.print(incomingData.c);
//   Serial.print(", d: ");
//   Serial.println(incomingData.d);
// }

// // 初始化 ESP-NOW 并添加对等设备
// bool initializeAndAddPeer(const String &macAddress) {
//     WiFi.mode(WIFI_STA);  // 设置为 Station 模式

//     if (esp_now_init() != ESP_OK) {
//         Serial.println("Error initializing ESP-NOW");
//         return false;
//     }

//     esp_now_register_send_cb(OnDataSent);  // 注册发送数据的回调函数

//     // 解析输入的 MAC 地址
//     int macIndex = 0;
//     char *token = strtok((char *)macAddress.c_str(), ":");
//     while (token != nullptr && macIndex < 6) {
//         peerInfo.peer_addr[macIndex++] = strtol(token, nullptr, 16);//
//         token = strtok(nullptr, ":");
//     }

//     if (macIndex != 6) {
//         Serial.println("Invalid MAC address format.");
//         return false;
//     }

//     peerInfo.channel = 0;  // 使用当前打开的通道
//     peerInfo.encrypt = false;  // 未加密

//     if (esp_now_add_peer(&peerInfo) != ESP_OK) {
//         Serial.println("Failed to add peer");
//         return false;
//     }

//     return true;
// }

// // 发送数据
// void sendData() {
//     strcpy(myData.a, "THIS IS A CHAR");
//     myData.b = random(1, 20);
//     myData.c = 1.2;
//     myData.d = false;

//     esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));  // 发送

//     if (result == ESP_OK) {
//         Serial.println("Sent with success");
//     } else {
//         Serial.println("Error sending the data");
//     }
// }

// // 初始化和发送的主函数
// void initializeAndSend(const String &macAddress) {
//     if (initializeAndAddPeer(macAddress)) {
//         Serial.print("Connected to: ");
//         for (int i = 0; i < 6; i++) {
//             Serial.printf("%02X", peerInfo.peer_addr[i]);
//             if (i < 5) Serial.print(":");
//         }
//         Serial.println();
        
//         // 发送数据
//         sendData();
//     }
// }

// // 获取用户输入的 MAC 地址
// String getMacAddressInput() {
//     Serial.println("Enter the MAC address of the target ESP32 (format: XX:XX:XX:XX:XX:XX):");
//     String macInput;

//     while (true) {
//         if (Serial.available()) {
//             macInput = Serial.readStringUntil('\n');  
//             macInput.trim(); 
//             return macInput;  // 返回获取到的 MAC 地址
//         }
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     String macInput = getMacAddressInput();  // 获取用户输入的 MAC 地址
//     initializeAndSend(macInput);  // 初始化并发送数据
// }

// void loop() {
//     // 在这里可以继续发送数据或其他逻辑
//     delay(5000);
// }

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// 定义常量
#define ESPNOW_WIFI_CHANNEL 0
#define NO_PMK_KEY false
#define BAUD 115200
#define MAX_CHARACTERS_NUMBER 20

// 发送和接收的数据结构
typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

struct_message myData;         // 发送数据
struct_message incomingData;   // 接收数据
esp_now_peer_info_t peerInfo;  // 对等设备信息

// 发送数据回调函数
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// 接收数据回调函数
void OnDataReceived(const esp_now_recv_info* info, const uint8_t* incomingDataBuffer, int len) {
    memcpy(&incomingData, incomingDataBuffer, sizeof(incomingData));
    Serial.print("Received data from: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", info->src_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.print(" | a: ");
    Serial.print(incomingData.a);
    Serial.print(", b: ");
    Serial.print(incomingData.b);
    Serial.print(", c: ");
    Serial.print(incomingData.c);
    Serial.print(", d: ");
    Serial.println(incomingData.d);
}

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
    strcpy(myData.a, "THIS IS A CHAR");
    myData.b = random(1, 20);
    myData.c = 1.2;
    myData.d = false;

    esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));  // 发送

    if (result == ESP_OK) {
        Serial.println("Sent with success");
    } else {
        Serial.println("Error sending the data");
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
    String macInput = getMacAddressInput();  // 获取用户输入的 MAC 地址
    if (initializeEspNow(macInput)) {  // 初始化并添加对等设备
        Serial.print("Connected to: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", peerInfo.peer_addr[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
    }
}

void setup() {
    Serial.begin(115200);
    mmWaveDeviceLocal_MACAddress_Requir();
    setupCommunication();  // 初始化通信
    esp_now_register_recv_cb(OnDataReceived);  // 注册接收数据的回调函数
    Serial.println("Ready to receive data...");
}

void loop() {
    sendData();  // 发送数据
    delay(5000); // 每5秒发送一次数据
}
