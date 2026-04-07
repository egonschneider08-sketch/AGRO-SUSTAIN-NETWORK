#include "sensor_modbus.h"
#include <ModbusMaster.h>

static ModbusMaster modbus;  // Biblioteca auxiliar

ModbusSensor::ModbusSensor(int dePin, int rePin, HardwareSerial* serial) {
    _dePin = dePin;
    _rePin = rePin;
    _serial = serial;
}

bool ModbusSensor::begin(uint32_t baudrate) {
    pinMode(_dePin, OUTPUT);
    pinMode(_rePin, OUTPUT);
    digitalWrite(_dePin, LOW);
    digitalWrite(_rePin, LOW);

    _serial->begin(baudrate);
    modbus.begin(MODBUS_SLAVE_ADDR, *_serial);
    modbus.preTransmission([this]() { setRS485Transmit(true); });
    modbus.postTransmission([this]() { setRS485Transmit(false); });

    return true;
}

void ModbusSensor::setRS485Transmit(bool transmit) {
    digitalWrite(_dePin, transmit ? HIGH : LOW);
    digitalWrite(_rePin, transmit ? HIGH : LOW);
}

bool ModbusSensor::read(SensorData* data) {
    uint8_t result;
    uint16_t registers[20];

    // Lê 10 registradores (valores em float)
    result = modbus.readHoldingRegisters(0x00, 10);

    if (result != modbus.ku8MBSuccess) {
        return false;
    }

    for (int i = 0; i < 10; i++) {
        registers[i] = modbus.getResponseBuffer(i);
    }

    // Converte para float (assumindo formato IEEE754 little-endian)
    data->soil_moisture     = readFloat((uint8_t*)registers, 0);
    data->soil_temperature  = readFloat((uint8_t*)registers, 2);
    data->air_humidity      = readFloat((uint8_t*)registers, 4);
    data->ec                = readFloat((uint8_t*)registers, 6);
    data->ph                = readFloat((uint8_t*)registers, 8);
    data->nitrogen          = readFloat((uint8_t*)registers, 10);
    data->phosphorus        = readFloat((uint8_t*)registers, 12);
    data->potassium         = readFloat((uint8_t*)registers, 14);

    return true;
}

float ModbusSensor::readFloat(uint8_t* buffer, int index) {
    union {
        uint8_t b[4];
        float f;
    } conv;

    conv.b[0] = buffer[index + 1];
    conv.b[1] = buffer[index];
    conv.b[2] = buffer[index + 3];
    conv.b[3] = buffer[index + 2];

    return conv.f;
}