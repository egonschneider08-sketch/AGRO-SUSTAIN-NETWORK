#ifndef LORA_SENDER_H
#define LORA_SENDER_H

#include <LoRa.h>
#include "data_types.h"
#include "config.h"   // ADICIONADO: para os macros DEBUG_*

class LoraSender {
public:
    LoraSender(int ssPin, int rstPin, int dio0Pin);

    bool   begin(long frequency);
    bool   sendSensorData(const SensorData* data);
    bool   sendMultiSensorData(const SensorData* dataArray, int count);
    bool   isActive() const;
    String getChipVersion();

private:
    int  _ssPin;
    int  _rstPin;
    int  _dio0Pin;
    bool _initialized;

    // Tamanho máximo de payload LoRa (SX1276 = 255 bytes)
    static constexpr size_t LORA_MAX_PAYLOAD = 255;
};

#endif