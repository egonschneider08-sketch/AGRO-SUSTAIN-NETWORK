#ifndef ESP_NOW_MANAGER_H
#define ESP_NOW_MANAGER_H

#include <esp_now.h>
#include <WiFi.h>

typedef void (*ReceiveCallback)(const uint8_t* mac, const uint8_t* data, int len);
typedef void (*SendCallback)(const uint8_t* mac, esp_now_send_status_t status);

class ESPNowManager {
public:
    bool begin();
    bool addPeer(const uint8_t* mac);
    bool send(const uint8_t* mac, const uint8_t* data, size_t len);
    void onReceive(ReceiveCallback cb);
    void onSend(SendCallback cb);
    void getMac(uint8_t* mac);

private:
    ReceiveCallback _recvCb = nullptr;
    SendCallback _sendCb = nullptr;

    static void recvCallback(const uint8_t* mac, const uint8_t* data, int len);
    static void sendCallback(const uint8_t* mac, esp_now_send_status_t status);
    static ESPNowManager* instance;
};

#endif