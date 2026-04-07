#include <Arduino.h>
#include "config.h"

//Inclui os headers da pasta common
#include "data_types.h"
#include "sensor_modbus.h"
#include "esp_now_manager.h"
// Objetos
ModbusSensor sensor(DE_RE_PIN, DE_RE_PIN, &Serial2);
ESPNowManager network;
SensorData myData;

uint8_t nextMac[6] = NEXT_MAC;
unsigned long lastRead = 0;
unsigned long lastSend = 0;
bool hasData = false;

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len == sizeof(SensorData)) {
        // Encaminha para o próximo nó
        network.send(nextMac, data, len);
    }
}

void setup() {
    Serial.begin(115200);

    sensor.begin(9600);

    network.begin();
    network.addPeer(nextMac);
    network.onReceive(onDataReceived);
}

void loop() {
    unsigned long now = millis();

    // Lê sensor a cada 10s
    if (now - lastRead >= 10000) {
        if (sensor.read(&myData)) {
            myData.node_id = NODE_ID;
            myData.timestamp_ms = now;
            hasData = true;
        }
        lastRead = now;
    }

    // Envia a cada 3s com offset
    if (hasData && (now - lastSend >= SEND_INTERVAL_MS)) {
        network.send(nextMac, (uint8_t*)&myData, sizeof(myData));
        lastSend = now;
    }

    delay(10);
}