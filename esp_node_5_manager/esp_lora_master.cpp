#include <Arduino.h>
#include "config.h"

// Inclui os headers da pasta common (sem duplicação)
#include "data_types.h"
#include "esp_now_manager.h"
#include "lora_sender.h"      // Este está na pasta local

// ============================================
// OBJETOS GLOBAIS
// ============================================
ESPNowManager network;
LoraSender lora(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

// Armazena dados recebidos de cada nó
SensorData allData[5];
bool dataReceived[5] = {false};

// MACs dos nós escravos (1 a 4)
uint8_t nodeMacs[4][6] = {NODE_1_MAC, NODE_2_MAC, NODE_3_MAC, NODE_4_MAC};

// Controle de tempo
unsigned long lastLoraSend = 0;

// ============================================
// CALLBACKS DO ESP-NOW
// ============================================

// Callback quando dados são recebidos via ESP-NOW
void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    // Verifica se o tamanho é compatível
    if (len != sizeof(SensorData)) {
        DEBUG_PRINTF("[RECV] Tamanho inesperado: %d bytes (esperado %d)\n", len, sizeof(SensorData));
        return;
    }

    // Copia os dados
    SensorData* received = (SensorData*)data;

    // Verifica se o node_id é válido (1 a 5)
    if (received->node_id >= 1 && received->node_id <= 5) {
        // Armazena os dados (node_id 1 vai para índice 0, etc)
        allData[received->node_id - 1] = *received;
        dataReceived[received->node_id - 1] = true;

        DEBUG_PRINTF("[RECV] ✅ Dados recebidos do nó %d\n", received->node_id);
        DEBUG_PRINTF("       Umidade solo: %.1f%% | pH: %.2f\n",
                     received->soil_moisture, received->ph);
    } else {
        DEBUG_PRINTF("[RECV] ⚠️ Node ID inválido: %d\n", received->node_id);
    }
}

// Callback quando dados são enviados via ESP-NOW (confirmação)
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

// Verifica se todos os 5 nós já enviaram dados
bool allNodesDataReceived() {
    for (int i = 0; i < 5; i++) {
        if (!dataReceived[i]) {
            return false;
        }
    }
    return true;
}

// Reseta as flags de recebimento (após enviar via LoRa)
void resetReceivedFlags() {
    for (int i = 0; i < 5; i++) {
        dataReceived[i] = false;
    }
    DEBUG_PRINTLN("[FLAGS] Flags de recebimento resetadas");
}

// Imprime resumo dos dados recebidos
void printSummary() {
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("📊 RESUMO DOS DADOS RECEBIDOS");
    DEBUG_PRINTLN("========================================");

    for (int i = 0; i < 5; i++) {
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
    // Inicializa serial para debug
    Serial.begin(115200);
    delay(1000);

    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("🌱 ESP32 - NÓ MESTRE LORA");
    DEBUG_PRINTLN("📡 Agro Tag Network");
    DEBUG_PRINTLN("========================================\n");

    // ===== INICIALIZA LORA =====
    DEBUG_PRINTLN("[SETUP] Inicializando módulo LoRa...");
    if (!lora.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[SETUP] ❌ ERRO FATAL: LoRa não iniciou");
        DEBUG_PRINTLN("[SETUP] Verifique as conexões dos pinos:");
        DEBUG_PRINTF("       SS: %d, RST: %d, DIO0: %d\n", LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
        while (1) {
            delay(1000);
            Serial.println("❌ ERRO: Reinicie o ESP32 e verifique o LoRa");
        }
    }

    // ===== INICIALIZA ESP-NOW =====
    DEBUG_PRINTLN("[SETUP] Inicializando ESP-NOW...");
    if (!network.begin()) {
        DEBUG_PRINTLN("[SETUP] ❌ ERRO FATAL: ESP-NOW não iniciou");
        while (1) {
            delay(1000);
            Serial.println("❌ ERRO: ESP-NOW falhou");
        }
    }

    // ===== REGISTRA CALLBACKS =====
    network.onReceive(onDataReceived);
    network.onSend(onDataSent);

    // ===== ADICIONA TODOS OS NÓS ESCRAVOS COMO PEERS =====
    DEBUG_PRINTLN("[SETUP] Adicionando nós escravos como peers...");

    for (int i = 0; i < 4; i++) {
        DEBUG_PRINTF("  Adicionando nó %d: ", i + 1);
        for (int j = 0; j < 6; j++) {
            DEBUG_PRINTF("%02X", nodeMacs[i][j]);
            if (j < 5) DEBUG_PRINT(":");
        }

        if (network.addPeer(nodeMacs[i])) {
            DEBUG_PRINTLN(" ✅");
        } else {
            DEBUG_PRINTLN(" ❌");
        }
    }

    // ===== MOSTRA MEU MAC =====
    uint8_t myMac[6];
    network.getMac(myMac);
    DEBUG_PRINT("[SETUP] Meu MAC Address: ");
    for (int i = 0; i < 6; i++) {
        DEBUG_PRINTF("%02X", myMac[i]);
        if (i < 5) DEBUG_PRINT(":");
    }
    DEBUG_PRINTLN();

    // ===== MOSTRA VERSÃO DO LORA =====
    String loraVersion = lora.getChipVersion();
    DEBUG_PRINTF("[SETUP] Versão do chip LoRa: %s\n", loraVersion.c_str());

    DEBUG_PRINTLN("\n[SETUP] ✅ Setup concluído! Aguardando dados dos nós...\n");
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
    unsigned long currentMillis = millis();

    // ===== A CADA 10 SEGUNDOS, VERIFICA SE PODE ENVIAR VIA LORA =====
    if (currentMillis - lastLoraSend >= SEND_INTERVAL_MS) {

        // Verifica se recebeu dados de todos os 5 nós
        if (allNodesDataReceived()) {
            DEBUG_PRINTLN("\n[LORA] Todos os dados recebidos! Enviando via LoRa...");

            // Imprime resumo antes de enviar
            printSummary();

            // Envia os dados via LoRa
            if (lora.sendMultiSensorData(allData, 5)) {
                DEBUG_PRINTLN("[LORA] ✅ Dados enviados com sucesso para a Raspberry!");

                // Reseta as flags após envio bem-sucedido
                resetReceivedFlags();
                lastLoraSend = currentMillis;
            } else {
                DEBUG_PRINTLN("[LORA] ❌ Falha no envio via LoRa. Tentará novamente no próximo ciclo.");
            }
        } else {
            // Ainda não recebeu todos os dados
            DEBUG_PRINTF("[LORA] Aguardando dados... (%d/5 nós recebidos)\n",
                         (int)(dataReceived[0] + dataReceived[1] + dataReceived[2] + dataReceived[3] + dataReceived[4]));

            // Mostra quais nós faltam
            DEBUG_PRINT("       Aguardando: ");
            for (int i = 0; i < 5; i++) {
                if (!dataReceived[i]) {
                    DEBUG_PRINTF("%d ", i + 1);
                }
            }
            DEBUG_PRINTLN();
        }
    }

    // Pequeno delay para evitar watchdog
    delay(10);
}