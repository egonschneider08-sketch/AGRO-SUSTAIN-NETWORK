#include <Arduino.h>
#include "config.h"
#include "data_types.h"
#include "esp_now_manager.h"
#include "lora_sender.h"

// ============================================
// OBJETOS GLOBAIS
// ============================================
ESPNowManager network;
LoraSender lora(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

// Armazena dados recebidos de cada nó (índice 0 = nó 1, ..., índice 3 = nó 4)
SensorData   allData[NODE_COUNT];
bool         dataReceived[NODE_COUNT] = {false};
int          receivedCount = 0; // Contador explícito — evita soma de bool

// MACs dos nós escravos (1 a 4)
uint8_t nodeMacs[NODE_COUNT][6] = {
    NODE_1_MAC, NODE_2_MAC, NODE_3_MAC, NODE_4_MAC
};

unsigned long lastLoraSend = 0;

// ============================================
// CALLBACKS DO ESP-NOW
// ============================================

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(SensorData)) {
        DEBUG_PRINTF("[RECV] Tamanho inesperado: %d bytes (esperado %zu)\n",
                     len, sizeof(SensorData));
        return;
    }

    // CORRIGIDO: memcpy em vez de cast direto — garante alinhamento
    SensorData received;
    memcpy(&received, data, sizeof(SensorData));

    // Nós escravos são 1 a 4 (índice 0 a 3)
    if (received.node_id >= 1 && received.node_id <= NODE_COUNT) {
        int idx = received.node_id - 1;

        // Só incrementa o contador se ainda não havia recebido deste nó
        if (!dataReceived[idx]) {
            receivedCount++;
        }

        allData[idx]      = received;
        dataReceived[idx] = true;

        DEBUG_PRINTF("[RECV] ✅ Nó %d | Umidade: %.1f%% | pH: %.2f | (%d/%d)\n",
                     received.node_id, received.soil_moisture,
                     received.ph, receivedCount, NODE_COUNT);
    } else {
        DEBUG_PRINTF("[RECV] ⚠️ Node ID inválido: %d\n", received.node_id);
    }
}

void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        DEBUG_PRINTLN("[SEND] ✅ Confirmação de envio recebida");
    } else {
        DEBUG_PRINTLN("[SEND] ❌ Falha no envio");
    }
}

// ============================================
// FUNÇÕES AUXILIARES
// ============================================

bool allNodesDataReceived() {
    return receivedCount >= NODE_COUNT;
}

void resetReceivedFlags() {
    memset(dataReceived, 0, sizeof(dataReceived));
    receivedCount = 0;
    DEBUG_PRINTLN("[FLAGS] Flags de recebimento resetadas");
}

void printSummary() {
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("📊 RESUMO DOS DADOS RECEBIDOS");
    DEBUG_PRINTLN("========================================");

    for (int i = 0; i < NODE_COUNT; i++) {
        if (dataReceived[i]) {
            DEBUG_PRINTF("Nó %d: Umidade=%.1f%% | Temp=%.1f°C | pH=%.2f\n",
                         allData[i].node_id,
                         allData[i].soil_moisture,
                         allData[i].soil_temperature,
                         allData[i].ph);
        } else {
            DEBUG_PRINTF("Nó %d: ⏳ Aguardando...\n", i + 1);
        }
    }
    DEBUG_PRINTLN("========================================\n");
}

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("🌱 ESP32 - NÓ MESTRE LORA");
    DEBUG_PRINTLN("📡 Agro Tag Network");
    DEBUG_PRINTLN("========================================\n");

    // ===== LORA =====
    DEBUG_PRINTLN("[SETUP] Inicializando módulo LoRa...");
    if (!lora.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[SETUP] ❌ ERRO FATAL: LoRa não iniciou");
        DEBUG_PRINTF("        SS: %d, RST: %d, DIO0: %d\n",
                     LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
        while (true) { delay(1000); }
    }

    // ===== ESP-NOW =====
    DEBUG_PRINTLN("[SETUP] Inicializando ESP-NOW...");
    if (!network.begin()) {
        DEBUG_PRINTLN("[SETUP] ❌ ERRO FATAL: ESP-NOW não iniciou");
        while (true) { delay(1000); }
    }

    network.onReceive(onDataReceived);
    network.onSend(onDataSent);

    // ===== PEERS =====
    DEBUG_PRINTLN("[SETUP] Adicionando nós escravos como peers...");
    for (int i = 0; i < NODE_COUNT; i++) {
        DEBUG_PRINTF("  Nó %d: %02X:%02X:%02X:%02X:%02X:%02X — ",
                     i + 1,
                     nodeMacs[i][0], nodeMacs[i][1], nodeMacs[i][2],
                     nodeMacs[i][3], nodeMacs[i][4], nodeMacs[i][5]);

        DEBUG_PRINTLN(network.addPeer(nodeMacs[i]) ? "✅" : "❌");
    }

    // ===== MAC PRÓPRIO =====
    uint8_t myMac[6];
    network.getMac(myMac);
    DEBUG_PRINTF("[SETUP] Meu MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 myMac[0], myMac[1], myMac[2],
                 myMac[3], myMac[4], myMac[5]);

    DEBUG_PRINTF("[SETUP] Versão chip LoRa: %s\n", lora.getChipVersion().c_str());

    // CORRIGIDO: inicializa lastLoraSend com millis() para não disparar imediatamente
    lastLoraSend = millis();

    DEBUG_PRINTLN("\n[SETUP] ✅ Setup concluído! Aguardando dados dos nós...\n");
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastLoraSend >= SEND_INTERVAL_MS) {

        if (allNodesDataReceived()) {
            DEBUG_PRINTLN("\n[LORA] Todos os dados recebidos! Enviando...");
            printSummary();

            if (lora.sendMultiSensorData(allData, NODE_COUNT)) {
                DEBUG_PRINTLN("[LORA] ✅ Dados enviados para a Raspberry!");
                resetReceivedFlags();
                lastLoraSend = currentMillis;
            } else {
                DEBUG_PRINTLN("[LORA] ❌ Falha no envio. Tentará no próximo ciclo.");
            }
        } else {
            DEBUG_PRINTF("[LORA] Aguardando... (%d/%d nós)\n",
                         receivedCount, NODE_COUNT);
            DEBUG_PRINT("       Faltam: ");
            for (int i = 0; i < NODE_COUNT; i++) {
                if (!dataReceived[i]) DEBUG_PRINTF("%d ", i + 1);
            }
            DEBUG_PRINTLN();

            // Atualiza o timer mesmo sem enviar, para não acumular atraso
            lastLoraSend = currentMillis;
        }
    }

    // CORRIGIDO: yield() em vez de delay(10) — não bloqueia callbacks
    yield();
}