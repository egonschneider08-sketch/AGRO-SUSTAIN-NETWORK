#include "esp_now_manager.h"

ESPNowManager* ESPNowManager::instance = nullptr;

bool ESPNowManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) return false;

    instance = this;
    esp_now_register_recv_cb(recvCallback);
    esp_now_register_send_cb(sendCallback);

    return true;
}

bool ESPNowManager::addPeer(const uint8_t* mac) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool ESPNowManager::send(const uint8_t* mac, const uint8_t* data, size_t len) {
    return esp_now_send(mac, data, len) == ESP_OK;
}

void ESPNowManager::onReceive(ReceiveCallback cb) {
    _recvCb = cb;
}

void ESPNowManager::onSend(SendCallback cb) {
    _sendCb = cb;
}

void ESPNowManager::getMac(uint8_t* mac) {
    WiFi.macAddress(mac);
}

void ESPNowManager::recvCallback(const uint8_t* mac, const uint8_t* data, int len) {
    if (instance && instance->_recvCb) {
        instance->_recvCb(mac, data, len);
    }
}

void ESPNowManager::sendCallback(const uint8_t* mac, esp_now_send_status_t status) {
    if (instance && instance->_sendCb) {
        instance->_sendCb(mac, status);
    }
}