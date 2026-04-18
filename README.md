# AgroTag Network

Sistema embarcado distribuído para monitoramento agrícola de precisão. Sensores de campo coletam dados de solo e ambiente via Modbus RS485 e os transmitem sem fio até um gateway central — sem necessidade de Wi-Fi ou internet.

---

## Stack

### Nós Sensores (×4) — ESP32
- **Linguagem:** C++ (Arduino Framework)
- **Build:** PlatformIO
- **Comunicação:** ESP-NOW (peer-to-peer, sem roteador)
- **Sensor:** Modbus RTU via RS485 (MAX485)
- **Biblioteca Modbus:** [ModbusMaster](https://github.com/4-20ma/ModbusMaster)

### Nó Mestre — ESP32
- **Linguagem:** C++ (Arduino Framework)
- **Build:** PlatformIO
- **Recepção:** ESP-NOW
- **Transmissão:** LoRa SX1276 (Ra-02 / RFM95)
- **Biblioteca LoRa:** [arduino-LoRa](https://github.com/sandeepmistry/arduino-LoRa)

### Gateway — Raspberry Pi 3
- **Sistema Operacional:** Ubuntu
- **Linguagem:** Python 3
- **Recepção LoRa:** Driver SX1276 próprio via `spidev` (sem dependência de pyLoRa)
- **Banco de dados:** SQLite 3
- **Bibliotecas:** `RPi.GPIO`, `spidev`

---

## Topologia

```
[Nó 1]──┐
[Nó 2]──┤  ESP-NOW   [Mestre]  LoRa   [Raspberry Pi]
[Nó 3]──┤ ─────────▶  ESP32  ───────▶  SQLite + Dashboard
[Nó 4]──┘
```

---

## Dados Coletados

Cada nó coleta via sensor Modbus RS485 (NPK + pH + ambiente):

| Parâmetro | Unidade |
|---|---|
| Umidade do solo | % |
| Temperatura do solo | °C |
| Umidade do ar | % |
| Condutividade elétrica (EC) | µS/cm |
| pH | — |
| Nitrogênio (N) | mg/kg |
| Fósforo (P) | mg/kg |
| Potássio (K) | mg/kg |

---

## Protocolo de Comunicação

| Trecho | Tecnologia | Alcance típico |
|---|---|---|
| Sensor → Nó ESP32 | Modbus RTU / RS485 | até 1200 m (cabo) |
| Nó → Mestre | ESP-NOW (2.4 GHz) | até 200 m (ar livre) |
| Mestre → Raspberry | LoRa 915 MHz SF12 | até 5 km (ar livre) |

---

## Estrutura do Repositório

```
agrotag-network/
├── common/               # Código compartilhado entre os nós ESP32
│   ├── data_types.h      # Struct SensorData (37 bytes, packed)
│   ├── esp_now_manager   # Abstração ESP-NOW
│   └── sensor_modbus     # Driver Modbus RS485
├── node_slave/           # Firmware dos nós sensores (1 a 4)
├── esp_manager/          # Firmware do nó mestre + driver LoRa
└── rasp_lora_receiver/   # Receptor Python para Raspberry Pi
    ├── receiver.py        # Loop de recepção LoRa + persistência SQLite
    ├── sx1276.py          # Driver SX1276 via spidev
    └── dashboard.py       # Dashboard de terminal em tempo real
```

---

## Dependências

**ESP32 (PlatformIO)**
```ini
lib_deps =
    4-20ma/ModbusMaster @ ^2.0.1   ; nós escravos
    sandeepmistry/LoRa @ ^0.8.0    ; nó mestre
```

**Raspberry Pi**
```bash
pip install RPi.GPIO spidev
```