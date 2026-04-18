#include "lora_sender.h"

LoraSender::LoraSender(int ssPin, int rstPin, int dio0Pin)
    : _ssPin(ssPin), _rstPin(rstPin), _dio0Pin(dio0Pin), _initialized(false) {}

bool LoraSender::begin(long frequency) {
    DEBUG_PRINTF("[LORA] Inicializando em %.0f MHz...\n", frequency / 1000000.0);

    LoRa.setPins(_ssPin, _rstPin, _dio0Pin);

    if (!LoRa.begin(frequency)) {
        DEBUG_PRINTLN("[LORA] ❌ Falha na inicialização!");
        _initialized = false;
        return false;
    }

    LoRa.setSpreadingFactor(12);
    LoRa.setCodingRate4(8);
    LoRa.setTxPower(20);
    LoRa.setSignalBandwidth(125E3);

    _initialized = true;
    DEBUG_PRINTLN("[LORA] ✅ Inicializado com sucesso!");
    return true;
}

bool LoraSender::sendSensorData(const SensorData* data) {
    if (!_initialized || data == nullptr) {
        DEBUG_PRINTLN("[LORA] ❌ Módulo não inicializado ou dados nulos");
        return false;
    }

    DEBUG_PRINTF("[LORA] Enviando dados do nó %d...\n", data->node_id);

    LoRa.beginPacket();
    LoRa.write((const uint8_t*)data, sizeof(SensorData));

    // CORRIGIDO: endPacket() retorna 1 (sucesso) ou 0 (falha)
    bool ok = (LoRa.endPacket() == 1);
    DEBUG_PRINTLN(ok ? "[LORA] ✅ Enviado" : "[LORA] ❌ Falha no envio");
    return ok;
}

bool LoraSender::sendMultiSensorData(const SensorData* dataArray, int count) {
    if (!_initialized || dataArray == nullptr) {
        DEBUG_PRINTLN("[LORA] ❌ Módulo não inicializado ou dados nulos");
        return false;
    }

    if (count <= 0 || count > 5) {
        DEBUG_PRINTLN("[LORA] ❌ Número inválido de sensores");
        return false;
    }

    // ADICIONADO: verificação de tamanho antes de enviar
    size_t totalSize = 1 + (count * sizeof(SensorData)); // 1 byte count + dados
    if (totalSize > LORA_MAX_PAYLOAD) {
        DEBUG_PRINTF("[LORA] ❌ Payload muito grande: %zu bytes (max %zu)\n",
                     totalSize, LORA_MAX_PAYLOAD);
        return false;
    }

    DEBUG_PRINTF("[LORA] Enviando %d sensores (%zu bytes)...\n", count, totalSize);

    LoRa.beginPacket();
    LoRa.write((uint8_t)count);
    for (int i = 0; i < count; i++) {
        LoRa.write((const uint8_t*)&dataArray[i], sizeof(SensorData));
    }

    bool ok = (LoRa.endPacket() == 1);
    DEBUG_PRINTLN(ok ? "[LORA] ✅ Enviado com sucesso" : "[LORA] ❌ Falha no envio");
    return ok;
}

bool LoraSender::isActive() const {
    return _initialized;
}

String LoraSender::getChipVersion() {
    // CORRIGIDO: protege contra chamada antes de begin()
    if (!_initialized) return "N/A";
    uint8_t version = LoRa.readRegister(0x42);
    return String(version, HEX);
}