"""
Agro sustain Network — Raspberry Pi LoRa Receiver
=============================================
Recebe pacotes LoRa do nó mestre ESP32, desempacota a struct SensorData
e salva em banco SQLite local.

Compatível com: SX1276 / SX1278 (Ra-02) e RFM95 via SPI
Biblioteca: pyLoRa (SX127x) ou sx126x conforme o chip

Instale as dependências:
    pip install pyLoRa RPi.GPIO spidev

Pinagem padrão (Raspberry Pi → Ra-02/RFM95):
    3.3V  → VCC
    GND   → GND
    GPIO10 (MOSI) → MOSI
    GPIO9  (MISO) → MISO
    GPIO11 (SCLK) → SCK
    GPIO8  (CE0)  → NSS/CS
    GPIO22        → RST
    GPIO25        → DIO0

Ajuste PIN_RST e PIN_DIO0 abaixo se necessário.
"""

import struct
import time
import sqlite3
import logging
import signal
import sys
from datetime import datetime
from dataclasses import dataclass
from typing import Optional

# ============================================================
# CONFIGURAÇÃO — ajuste conforme seu hardware
# ============================================================
LORA_FREQUENCY   = 915        # MHz — deve ser igual ao ESP32 (915, 868 ou 433)
LORA_SF          = 12         # Spreading Factor
LORA_BW          = 125000     # Bandwidth Hz
LORA_CR          = 8          # Coding Rate (4/8)

PIN_RST          = 22         # GPIO RST do módulo LoRa
PIN_DIO0         = 25         # GPIO DIO0 do módulo LoRa

DB_PATH          = "agrotag.db"

# ============================================================
# LOGGING
# ============================================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler("receiver.log"),
    ]
)
log = logging.getLogger("agrotag")

# ============================================================
# STRUCT — deve ser idêntica ao data_types.h do ESP32
#
# struct SensorData {
#     uint8_t  node_id;
#     float    soil_moisture;
#     float    soil_temperature;
#     float    air_humidity;
#     float    ec;
#     float    ph;
#     float    nitrogen;
#     float    phosphorus;
#     float    potassium;
#     uint32_t timestamp_ms;
# }  __attribute__((packed))  → 37 bytes
# ============================================================
SENSOR_STRUCT_FMT  = "<BffffffffI"   # little-endian, packed
SENSOR_STRUCT_SIZE = struct.calcsize(SENSOR_STRUCT_FMT)  # deve ser 37

@dataclass
class SensorData:
    node_id:          int
    soil_moisture:    float
    soil_temperature: float
    air_humidity:     float
    ec:               float
    ph:               float
    nitrogen:         float
    phosphorus:       float
    potassium:        float
    timestamp_ms:     int
    received_at:      str = ""

    def __post_init__(self):
        if not self.received_at:
            self.received_at = datetime.now().isoformat()

    def __str__(self):
        return (
            f"Nó {self.node_id} | "
            f"Umidade={self.soil_moisture:.1f}% | "
            f"Temp={self.soil_temperature:.1f}°C | "
            f"pH={self.ph:.2f} | "
            f"EC={self.ec:.1f}µS/cm | "
            f"N={self.nitrogen:.1f} P={self.phosphorus:.1f} K={self.potassium:.1f} mg/kg"
        )


def unpack_sensor(raw: bytes) -> Optional[SensorData]:
    """Desempacota bytes recebidos via LoRa na struct SensorData."""
    if len(raw) != SENSOR_STRUCT_SIZE:
        log.warning(f"Tamanho inesperado: {len(raw)} bytes (esperado {SENSOR_STRUCT_SIZE})")
        return None
    try:
        fields = struct.unpack(SENSOR_STRUCT_FMT, raw)
        node_id = fields[0]
        if not (1 <= node_id <= 5):
            log.warning(f"node_id inválido: {node_id}")
            return None
        return SensorData(*fields)
    except struct.error as e:
        log.error(f"Erro ao desempacotar: {e}")
        return None


def unpack_multi_sensor(raw: bytes) -> list[SensorData]:
    """
    Desempacota pacote com múltiplos sensores.
    Formato: [count: 1 byte][SensorData × count]
    """
    if len(raw) < 1:
        return []

    count = raw[0]
    payload = raw[1:]
    expected = count * SENSOR_STRUCT_SIZE

    if len(payload) != expected:
        log.warning(f"Payload inválido: {len(payload)} bytes para {count} sensores "
                    f"(esperado {expected})")
        return []

    results = []
    for i in range(count):
        chunk = payload[i * SENSOR_STRUCT_SIZE:(i + 1) * SENSOR_STRUCT_SIZE]
        sensor = unpack_sensor(chunk)
        if sensor:
            results.append(sensor)

    return results


# ============================================================
# BANCO DE DADOS SQLite
# ============================================================

def init_db(path: str) -> sqlite3.Connection:
    conn = sqlite3.connect(path, check_same_thread=False)
    conn.execute("""
                 CREATE TABLE IF NOT EXISTS sensor_readings (
                                                                id               INTEGER PRIMARY KEY AUTOINCREMENT,
                                                                received_at      TEXT    NOT NULL,
                                                                node_id          INTEGER NOT NULL,
                                                                soil_moisture    REAL,
                                                                soil_temperature REAL,
                                                                air_humidity     REAL,
                                                                ec               REAL,
                                                                ph               REAL,
                                                                nitrogen         REAL,
                                                                phosphorus       REAL,
                                                                potassium        REAL,
                                                                timestamp_ms     INTEGER,
                                                                rssi             INTEGER,
                                                                snr              REAL
                 )
                 """)
    conn.execute("""
                 CREATE INDEX IF NOT EXISTS idx_node_time
                     ON sensor_readings (node_id, received_at)
                 """)
    conn.commit()
    log.info(f"Banco de dados inicializado: {path}")
    return conn


def save_to_db(conn: sqlite3.Connection, data: SensorData,
               rssi: int = 0, snr: float = 0.0):
    conn.execute("""
                 INSERT INTO sensor_readings (
                     received_at, node_id,
                     soil_moisture, soil_temperature, air_humidity,
                     ec, ph, nitrogen, phosphorus, potassium,
                     timestamp_ms, rssi, snr
                 ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
                 """, (
                     data.received_at, data.node_id,
                     data.soil_moisture, data.soil_temperature, data.air_humidity,
                     data.ec, data.ph, data.nitrogen, data.phosphorus, data.potassium,
                     data.timestamp_ms, rssi, snr
                 ))
    conn.commit()


# ============================================================
# LORA — inicialização
# ============================================================

def init_lora():
    """
    Inicializa o módulo LoRa via pyLoRa.
    Se seu módulo for SX1262, substitua por sx126x.
    """
    try:
        from SX127x.LoRa import LoRa
        from SX127x.board_config import BOARD

        BOARD.setup()
        BOARD.reset()

        class AgroTagLoRa(LoRa):
            def __init__(self, verbose=False):
                super().__init__(verbose)
                self.set_mode(0x81)  # SLEEP
                self.set_dio_mapping([0] * 6)

        lora = AgroTagLoRa(verbose=False)
        lora.set_freq(LORA_FREQUENCY)
        lora.set_spreading_factor(LORA_SF)
        lora.set_bw(9)          # 9 = 125 kHz na pyLoRa
        lora.set_coding_rate(5) # 5 = CR4/8 na pyLoRa
        lora.set_rx_crc(True)
        lora.set_mode(0x85)     # RXCONT

        log.info(f"LoRa inicializado — {LORA_FREQUENCY} MHz SF{LORA_SF}")
        return lora

    except ImportError:
        log.error("Biblioteca pyLoRa não encontrada.")
        log.error("Instale com: pip install pyLoRa")
        sys.exit(1)
    except Exception as e:
        log.error(f"Erro ao inicializar LoRa: {e}")
        sys.exit(1)


# ============================================================
# RECEPÇÃO PRINCIPAL (polling)
# ============================================================

def receive_loop(conn: sqlite3.Connection):
    """
    Loop de recepção usando polling (sem interrupção GPIO).
    Compatível com qualquer configuração de hardware.
    """
    try:
        from SX127x.LoRa import LoRa
        from SX127x.board_config import BOARD
    except ImportError:
        log.error("pyLoRa não instalado. Execute: pip install pyLoRa")
        sys.exit(1)

    lora = init_lora()
    log.info("Aguardando pacotes LoRa...\n")

    running = True

    def handle_exit(sig, frame):
        nonlocal running
        log.info("Encerrando receptor...")
        running = False

    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    while running:
        try:
            # Verifica flag IRQ de RxDone (registro 0x12, bit 6)
            irq_flags = lora.get_irq_flags()

            if irq_flags.get("rx_done"):
                payload = lora.read_payload(nocheck=True)
                rssi    = lora.get_pkt_rssi_value()
                snr     = lora.get_pkt_snr_value()

                raw = bytes(payload)
                log.info(f"Pacote recebido: {len(raw)} bytes | RSSI={rssi} dBm | SNR={snr} dB")

                sensors = unpack_multi_sensor(raw)

                if not sensors:
                    log.warning("Nenhum sensor válido no pacote")
                else:
                    for s in sensors:
                        log.info(f"  ✅ {s}")
                        save_to_db(conn, s, rssi=rssi, snr=snr)

                # Limpa flags IRQ
                lora.set_irq_flags(
                    rx_done=True, rx_timeout=True, valid_header=True,
                    crc_error=True, fhss_change_channel=True, cad_done=True,
                    tx_done=True, cad_detected=True
                )

                # Volta ao modo recepção contínua
                lora.set_mode(0x85)

        except Exception as e:
            log.error(f"Erro no loop de recepção: {e}")
            time.sleep(1)

        time.sleep(0.05)  # 50ms polling — não sobrecarrega a CPU

    BOARD.teardown()
    log.info("Receptor encerrado.")


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":
    log.info("=== AgroTag Network — Receptor LoRa ===")
    log.info(f"Struct SensorData: {SENSOR_STRUCT_SIZE} bytes")

    assert SENSOR_STRUCT_SIZE == 37, (
        f"Tamanho da struct incorreto: {SENSOR_STRUCT_SIZE} bytes (esperado 37). "
        "Verifique SENSOR_STRUCT_FMT."
    )

    conn = init_db(DB_PATH)
    receive_loop(conn)
    conn.close()