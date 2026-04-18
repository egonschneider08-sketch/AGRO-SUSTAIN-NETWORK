#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// IDENTIFICAÇÃO DO NÓ
// Altere NODE_ID para cada ESP: 1, 2, 3 ou 4
// ============================================
#define NODE_ID             1

// ============================================
// OFFSET DE ENVIO (evita colisão entre nós)
// Nó 1: 0ms | Nó 2: 750ms | Nó 3: 1500ms | Nó 4: 2250ms
// ============================================
#define SEND_OFFSET_MS      0UL
#define SEND_INTERVAL_MS    3000UL
#define READ_INTERVAL_MS    10000UL

// ============================================
// MAC DO PRÓXIMO NÓ NA CADEIA
// Nó 1 → Nó 2 → Nó 3 → Nó 4 → Mestre
// ⚠️ Substitua pelo MAC real
// ============================================
#define NEXT_MAC            {0x24, 0x6F, 0x28, 0xBB, 0xBB, 0x02}

// ============================================
// PINOS RS485 / MAX485
// Se DE e RE estão ligados juntos, use o mesmo pino
// ============================================
#define DE_PIN              4
#define RE_PIN              4   // Mesmo pino se hardware compartilhar

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