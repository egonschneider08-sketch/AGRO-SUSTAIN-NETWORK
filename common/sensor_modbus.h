#ifndef SENSOR_MODBUS_H
#define SENSOR_MODBUS_H

#include <HardwareSerial.h>
#include "data_types.h"

class ModbusSensor {
public:
    ModbusSensor(int dePin, int rePin, HardwareSerial* serial);

    bool begin(uint32_t baudrate = 9600);
    bool read(SensorData* data);

private:
    int _dePin;
    int _rePin;
    HardwareSerial* _serial;

    void setRS485Transmit(bool transmit);
    bool sendCommand(uint8_t slaveAddr, uint8_t funcCode, uint16_t startReg, uint16_t numRegs, uint8_t* response);
    float readFloat(uint8_t* buffer, int index);
};

#endif