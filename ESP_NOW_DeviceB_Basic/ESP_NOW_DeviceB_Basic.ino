#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>


#define ESPNOW_WIFI_CHANNEL 0
#define NO_PMK_KEY false
#define BAUD 115200
#define MAX_CHARACTERS_NUMBER 20


typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

struct_message myData;        
struct_message incomingData;   
esp_now_peer_info_t peerInfo;  


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}


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
    
    
    Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
    strcpy(myData.a, "THIS IS A CHAR");
    myData.b = random(1, 20);
    myData.c = 1.2;
    myData.d = false;

    esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *)&myData, sizeof(myData));  

    if (result == ESP_OK) {
        Serial.println("Sent with success");
    } else {
        Serial.println("Error sending the data");
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

void setup() {
    Serial.begin(115200);
    mmWaveDeviceLocal_MACAddress_Requir();
    setupCommunication();  
    esp_now_register_recv_cb(OnDataReceived);  
    Serial.println("Ready to receive data...");
}

void loop() {
    sendData();  
    delay(5000); 
}
