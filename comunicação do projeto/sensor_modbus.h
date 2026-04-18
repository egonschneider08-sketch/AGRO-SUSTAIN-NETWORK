#ifndef SENSOR_MODBUS_H
#define SENSOR_MODBUS_H

#include <HardwareSerial.h>
#include "data_types.h"

#define MODBUS_SLAVE_ADDR  1
#define MODBUS_NUM_REGS   16   // 8 floats × 2 registradores cada

class ModbusSensor {
public:
    ModbusSensor(int dePin, int rePin, HardwareSerial* serial);

    bool begin(uint32_t baudrate = 9600);
    bool read(SensorData* data);

private:
    int             _dePin;
    int             _rePin;
    HardwareSerial* _serial;

    void  setRS485Transmit(bool transmit);
    float readFloat(const uint16_t* registers, int regIndex);
    // sendCommand removido — não implementado e não usado
};

#endif