#include "sensor_modbus.h"
#include <ModbusMaster.h>

ModbusSensor::ModbusSensor(int dePin, int rePin, HardwareSerial* serial)
    : _dePin(dePin), _rePin(rePin), _serial(serial) {}

// Instância local ao objeto via ponteiro — evita global estático
static ModbusMaster modbus;

bool ModbusSensor::begin(uint32_t baudrate) {
    if (_serial == nullptr) return false;

    pinMode(_dePin, OUTPUT);
    pinMode(_rePin, OUTPUT);
    setRS485Transmit(false);  // Estado inicial definido explicitamente

    _serial->begin(baudrate);
    modbus.begin(MODBUS_SLAVE_ADDR, *_serial);

    modbus.preTransmission([this]()  { setRS485Transmit(true);  });
    modbus.postTransmission([this]() { setRS485Transmit(false); });

    return true;
}

void ModbusSensor::setRS485Transmit(bool transmit) {
    digitalWrite(_dePin, transmit ? HIGH : LOW);
    digitalWrite(_rePin, transmit ? HIGH : LOW);
}

bool ModbusSensor::read(SensorData* data) {
    if (data == nullptr) return false;

    // CORRIGIDO: 8 floats × 2 registradores = 16 registradores necessários
    uint8_t result = modbus.readHoldingRegisters(0x00, MODBUS_NUM_REGS);

    if (result != ModbusMaster::ku8MBSuccess) {
        return false;
    }

    uint16_t registers[MODBUS_NUM_REGS];
    for (int i = 0; i < MODBUS_NUM_REGS; i++) {
        registers[i] = modbus.getResponseBuffer(i);
    }

    // CORRIGIDO: índices agora são de registrador (não de byte)
    data->soil_moisture    = readFloat(registers, 0);
    data->soil_temperature = readFloat(registers, 2);
    data->air_humidity     = readFloat(registers, 4);
    data->ec               = readFloat(registers, 6);
    data->ph               = readFloat(registers, 8);
    data->nitrogen         = readFloat(registers, 10);
    data->phosphorus       = readFloat(registers, 12);
    data->potassium        = readFloat(registers, 14);

    return true;
}

// CORRIGIDO: recebe uint16_t* e índice de registrador — sem cast perigoso
float ModbusSensor::readFloat(const uint16_t* registers, int regIndex) {
    union {
        uint8_t b[4];
        float   f;
    } conv;

    uint16_t hi = registers[regIndex];
    uint16_t lo = registers[regIndex + 1];

    // Formato IEEE754 big-endian (word-swapped) — padrão Modbus
    conv.b[0] = (lo >> 8) & 0xFF;
    conv.b[1] = (lo)      & 0xFF;
    conv.b[2] = (hi >> 8) & 0xFF;
    conv.b[3] = (hi)      & 0xFF;

    return conv.f;
}

//comunicação com sensores via Modbus