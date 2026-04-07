#include "lora_sender.h"

LoraSender::LoraSender(int ssPin, int rstPin, int dio0Pin) {
    _ssPin = ssPin;
    _rstPin = rstPin;
    _dio0Pin = dio0Pin;
    _initialized = false;
}

bool LoraSender::begin(long frequency) {
    DEBUG_PRINTF("[LORA] Inicializando na frequência %.0f MHz...\n", frequency / 1000000.0);

    // Configura os pinos
    LoRa.setPins(_ssPin, _rstPin, _dio0Pin);

    // Inicializa o LoRa
    if (!LoRa.begin(frequency)) {
        DEBUG_PRINTLN("[LORA] ❌ Falha na inicialização!");
        _initialized = false;
        return false;
    }

    // Configurações opcionais para melhor alcance
    LoRa.setSpreadingFactor(12);      // SF12 = maior alcance
    LoRa.setCodingRate4(8);           // CR4/8
    LoRa.setTxPower(20);              // Potência máxima (20 dBm)
    LoRa.setSignalBandwidth(125E3);   // 125 kHz

    _initialized = true;
    DEBUG_PRINTLN("[LORA] ✅ Inicializado com sucesso!");

    return true;
}

bool LoraSender::sendSensorData(const SensorData* data) {
    if (!_initialized) {
        DEBUG_PRINTLN("[LORA] ❌ Módulo não inicializado");
        return false;
    }

    DEBUG_PRINTF("[LORA] Enviando dados do nó %d...\n", data->node_id);

    // Inicia o pacote
    LoRa.beginPacket();

    // Escreve os dados byte a byte
    LoRa.write((uint8_t*)data, sizeof(SensorData));

    // Finaliza e envia
    size_t packetSize = LoRa.endPacket();

    if (packetSize == 0) {
        DEBUG_PRINTLN("[LORA] ❌ Falha no envio");
        return false;
    }

    DEBUG_PRINTF("[LORA] ✅ Enviado %d bytes\n", packetSize);
    return true;
}

bool LoraSender::sendMultiSensorData(const SensorData* dataArray, int count) {
    if (!_initialized) {
        DEBUG_PRINTLN("[LORA] ❌ Módulo não inicializado");
        return false;
    }

    if (count <= 0 || count > 5) {
        DEBUG_PRINTLN("[LORA] ❌ Número inválido de sensores");
        return false;
    }

    DEBUG_PRINTF("[LORA] Enviando dados de %d sensores...\n", count);

    // Inicia o pacote
    LoRa.beginPacket();

    // Primeiro envia o número de sensores
    LoRa.write((uint8_t)count);

    // Depois envia os dados de cada sensor
    for (int i = 0; i < count; i++) {
        LoRa.write((uint8_t*)&dataArray[i], sizeof(SensorData));
    }

    // Finaliza e envia
    size_t packetSize = LoRa.endPacket();

    if (packetSize == 0) {
        DEBUG_PRINTLN("[LORA] ❌ Falha no envio");
        return false;
    }

    DEBUG_PRINTF("[LORA] ✅ Enviado %d bytes\n", packetSize);
    return true;
}

bool LoraSender::isActive() {
    return _initialized;
}

String LoraSender::getChipVersion() {
    // Tenta ler um registrador para identificar o chip
    uint8_t version = LoRa.readRegister(0x42);  // Registro de versão do SX1276
    return String(version, HEX);
}