#ifndef LORA_SENDER_H
#define LORA_SENDER_H

#include <LoRa.h>
#include "data_types.h"

class LoraSender {
public:
    LoraSender(int ssPin, int rstPin, int dio0Pin);

    // Inicializa o módulo LoRa
    bool begin(long frequency);

    // Envia dados de um único sensor
    bool sendSensorData(const SensorData* data);

    // Envia dados de múltiplos sensores (array de 5)
    bool sendMultiSensorData(const SensorData* dataArray, int count);

    // Verifica se o módulo está ativo
    bool isActive();

    // Obtém a versão do chip (debug)
    String getChipVersion();

private:
    int _ssPin;
    int _rstPin;
    int _dio0Pin;
    bool _initialized;
};

#endif // LORA_SENDER_H