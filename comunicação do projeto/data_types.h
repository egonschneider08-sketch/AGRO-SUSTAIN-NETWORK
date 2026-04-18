#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <stdint.h>

#define ESP_NOW_MAX_PAYLOAD 250

#pragma pack(push, 1)
struct SensorData {
    uint8_t  node_id;           // 1 a 5
    float    soil_moisture;     // %
    float    soil_temperature;  // °C
    float    air_humidity;      // %
    float    ec;                // µS/cm
    float    ph;
    float    nitrogen;          // mg/kg
    float    phosphorus;        // mg/kg
    float    potassium;         // mg/kg
    uint32_t timestamp_ms;
};
#pragma pack(pop)

// Garante em tempo de compilação que a struct cabe no ESP-NOW
static_assert(sizeof(SensorData) <= ESP_NOW_MAX_PAYLOAD,
              "SensorData excede o limite de payload do ESP-NOW (250 bytes)");

#endif