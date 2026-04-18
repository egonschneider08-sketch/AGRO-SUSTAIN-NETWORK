#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// IDENTIFICAÇÃO DO NÓ MESTRE
// ============================================
#define NODE_ID             5       // Mestre é o nó 5
#define NODE_COUNT          4       // Número de nós ESCRAVOS (1 a 4)
#define TOTAL_NODES         5       // Total incluindo o mestre

// ============================================
// INTERVALOS DE TEMPO
// ============================================
#define SEND_INTERVAL_MS    10000UL // Envia via LoRa a cada 10 segundos

// ============================================
// MACS DOS ESPs ESCRAVOS (NÓS 1, 2, 3, 4)
// ============================================
#define NODE_1_MAC  {0x24, 0x6F, 0x28, 0xAA, 0xAA, 0x01}
#define NODE_2_MAC  {0x24, 0x6F, 0x28, 0xBB, 0xBB, 0x02}
#define NODE_3_MAC  {0x24, 0x6F, 0x28, 0xCC, 0xCC, 0x03}
#define NODE_4_MAC  {0x24, 0x6F, 0x28, 0xDD, 0xDD, 0x04}

// ============================================
// PINAGEM DO LORA (ESP32)
// ============================================
#define LORA_SS_PIN         5
#define LORA_RST_PIN        14
#define LORA_DIO0_PIN       2
#define LORA_FREQUENCY      915E6   // Américas

// ============================================
// DEBUG
// ============================================
#define DEBUG_ENABLE        1

#if DEBUG_ENABLE
    #define DEBUG_PRINT(x)    Serial.print(x)
    #define DEBUG_PRINTLN(x)  Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H