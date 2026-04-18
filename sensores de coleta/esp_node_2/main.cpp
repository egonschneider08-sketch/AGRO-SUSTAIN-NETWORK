#include <Arduino.h>
#include "config.h"
#include "data_types.h"
#include "sensor_modbus.h"
#include "esp_now_manager.h"

// ============================================
// OBJETOS GLOBAIS
// ============================================
ModbusSensor  sensor(DE_PIN, RE_PIN, &Serial2);
ESPNowManager network;

SensorData    myData;
uint8_t       nextMac[6] = NEXT_MAC;

unsigned long lastRead    = 0;
unsigned long lastSend    = 0;
bool          hasData     = false;

// ============================================
// FILA DE ENCAMINHAMENTO
// Evita chamar send() dentro do callback ESP-NOW
// ============================================
static uint8_t  forwardBuffer[sizeof(SensorData)];
static bool     forwardPending = false;

// ============================================
// CALLBACKS
// ============================================

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(SensorData)) {
        DEBUG_PRINTF("[RECV] Tamanho inválido: %d (esperado %zu)\n",
                     len, sizeof(SensorData));
        return;
    }

    // Valida node_id antes de encaminhar
    SensorData temp;
    memcpy(&temp, data, sizeof(SensorData));

    if (temp.node_id < 1 || temp.node_id > 4) {
        DEBUG_PRINTF("[RECV] node_id inválido: %d — descartado\n", temp.node_id);
        return;
    }

    // Não encaminha dados do próprio nó (loop prevention)
    if (temp.node_id == NODE_ID) {
        DEBUG_PRINTLN("[RECV] Pacote próprio — ignorado");
        return;
    }

    // CORRIGIDO: não chama send() aqui — agenda para o loop
    memcpy(forwardBuffer, data, sizeof(SensorData));
    forwardPending = true;

    DEBUG_PRINTF("[RECV] Agendado encaminhamento do nó %d\n", temp.node_id);
}

void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    DEBUG_PRINTLN(status == ESP_NOW_SEND_SUCCESS
                  ? "[SEND] ✅ Envio confirmado"
                  : "[SEND] ❌ Falha no envio");
}

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    DEBUG_PRINTF("\n[SETUP] 🌱 Nó Escravo — ID: %d\n", NODE_ID);
    DEBUG_PRINTF("[SETUP] Offset de envio: %lums | Intervalo: %lums\n",
                 SEND_OFFSET_MS, SEND_INTERVAL_MS);

    // ===== SENSOR MODBUS =====
    DEBUG_PRINTLN("[SETUP] Inicializando sensor Modbus...");
    if (!sensor.begin(9600)) {
        DEBUG_PRINTLN("[SETUP] ⚠️ Sensor Modbus não respondeu — verifique a fiação");
        // Não trava: continua para permitir roteamento de outros nós
    }

    // ===== ESP-NOW =====
    DEBUG_PRINTLN("[SETUP] Inicializando ESP-NOW...");
    if (!network.begin()) {
        DEBUG_PRINTLN("[SETUP] ❌ ERRO FATAL: ESP-NOW falhou");
        while (true) { delay(1000); }
    }

    DEBUG_PRINTF("[SETUP] Adicionando próximo nó: %02X:%02X:%02X:%02X:%02X:%02X — ",
                 nextMac[0], nextMac[1], nextMac[2],
                 nextMac[3], nextMac[4], nextMac[5]);
    DEBUG_PRINTLN(network.addPeer(nextMac) ? "✅" : "❌");

    network.onReceive(onDataReceived);
    network.onSend(onDataSent);

    // CORRIGIDO: inicializa lastSend com offset para escalonar envios
    // Nó 1: 0ms, Nó 2: 750ms, Nó 3: 1500ms, Nó 4: 2250ms
    lastSend = millis() - SEND_INTERVAL_MS + SEND_OFFSET_MS;
    lastRead = millis();

    uint8_t myMac[6];
    network.getMac(myMac);
    DEBUG_PRINTF("[SETUP] Meu MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 myMac[0], myMac[1], myMac[2],
                 myMac[3], myMac[4], myMac[5]);

    DEBUG_PRINTLN("[SETUP] ✅ Pronto!\n");
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
    unsigned long now = millis();

    // ===== LEITURA DO SENSOR =====
    if (now - lastRead >= READ_INTERVAL_MS) {
        lastRead = now;

        if (sensor.read(&myData)) {
            myData.node_id      = NODE_ID;
            myData.timestamp_ms = now;
            hasData = true;
            DEBUG_PRINTF("[SENSOR] ✅ Leitura OK | Umidade: %.1f%% | pH: %.2f\n",
                         myData.soil_moisture, myData.ph);
        } else {
            DEBUG_PRINTLN("[SENSOR] ❌ Falha na leitura Modbus");
        }
    }

    // ===== ENVIO DOS PRÓPRIOS DADOS =====
    if (hasData && (now - lastSend >= SEND_INTERVAL_MS)) {
        lastSend = now;

        // CORRIGIDO: copia para buffer antes de passar ao send
        uint8_t sendBuffer[sizeof(SensorData)];
        memcpy(sendBuffer, &myData, sizeof(SensorData));

        if (network.send(nextMac, sendBuffer, sizeof(SensorData))) {
            DEBUG_PRINTF("[SEND] ✅ Dados do nó %d enviados\n", NODE_ID);
        } else {
            DEBUG_PRINTLN("[SEND] ❌ Falha ao enviar — verificar peer");
        }
    }

    // ===== ENCAMINHAMENTO PENDENTE (agendado pelo callback) =====
    if (forwardPending) {
        forwardPending = false;

        if (network.send(nextMac, forwardBuffer, sizeof(SensorData))) {
            DEBUG_PRINTLN("[FWD] ✅ Encaminhado com sucesso");
        } else {
            DEBUG_PRINTLN("[FWD] ❌ Falha no encaminhamento");
        }
    }

    // CORRIGIDO: yield() em vez de delay(10)
    yield();
}