#ifndef CONFIG_H
#define CONFIG_H

#define NODE_ID             1
#define SEND_OFFSET_MS      0      // Node1 envia em t=0, 3s, 6s...
#define SEND_INTERVAL_MS    3000

// MAC do próximo ESP (Node 2)
#define NEXT_MAC            {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// Pinos MAX485
#define DE_RE_PIN           4

#endif