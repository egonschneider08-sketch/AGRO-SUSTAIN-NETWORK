"""
sx1276.py — Driver SX1276 via spidev puro
==========================================
Acessa o chip LoRa SX1276/SX1278 diretamente via SPI
sem depender de pyLoRa ou outras bibliotecas de alto nível.

Compatível com: Raspberry Pi 3 + Ubuntu
"""

import spidev
import RPi.GPIO as GPIO
import time

# ── Registradores SX1276 ──────────────────────────────────────────────────────
REG_FIFO                = 0x00
REG_OP_MODE             = 0x01
REG_FRF_MSB             = 0x06
REG_FRF_MID            = 0x07
REG_FRF_LSB             = 0x08
REG_PA_CONFIG           = 0x09
REG_LNA                 = 0x0C
REG_FIFO_ADDR_PTR       = 0x0D
REG_FIFO_RX_CURRENT_ADDR= 0x10
REG_IRQ_FLAGS           = 0x12
REG_RX_NB_BYTES         = 0x13
REG_PKT_SNR_VALUE       = 0x19
REG_PKT_RSSI_VALUE      = 0x1A
REG_MODEM_CONFIG_1      = 0x1D
REG_MODEM_CONFIG_2      = 0x1E
REG_MODEM_CONFIG_3      = 0x26
REG_VERSION             = 0x42

# ── Modos de operação ─────────────────────────────────────────────────────────
MODE_LONG_RANGE         = 0x80
MODE_SLEEP              = 0x00
MODE_STDBY              = 0x01
MODE_RX_CONTINUOUS      = 0x05

# ── IRQ flags ─────────────────────────────────────────────────────────────────
IRQ_RX_DONE             = 0x40
IRQ_CRC_ERROR           = 0x20

# ── Frequência base do oscilador SX1276 ──────────────────────────────────────
FXOSC                   = 32_000_000
FSTEP                   = FXOSC / (2 ** 19)


class SX1276:
    def __init__(self, pin_rst: int, pin_dio0: int,
                 spi_bus: int = 0, spi_dev: int = 0):
        self._pin_rst  = pin_rst
        self._pin_dio0 = pin_dio0
        self._spi      = spidev.SpiDev()
        self._spi_bus  = spi_bus
        self._spi_dev  = spi_dev

    def begin(self, frequency_mhz: float,
              sf: int = 12, bw: int = 125000, cr: int = 8) -> bool:
        """Inicializa o módulo e configura parâmetros LoRa."""
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(self._pin_rst,  GPIO.OUT)
        GPIO.setup(self._pin_dio0, GPIO.IN)

        self._spi.open(self._spi_bus, self._spi_dev)
        self._spi.max_speed_hz = 5_000_000
        self._spi.mode = 0b00

        self._reset()

        version = self._read_reg(REG_VERSION)
        if version != 0x12:
            print(f"[SX1276] Chip não detectado! Version reg = 0x{version:02X} (esperado 0x12)")
            return False

        print(f"[SX1276] Chip detectado — version=0x{version:02X}")

        # Modo sleep para configurar
        self._write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_SLEEP)
        time.sleep(0.01)

        # Frequência
        self._set_frequency(frequency_mhz)

        # LNA boost
        self._write_reg(REG_LNA, 0x23)

        # Modem config
        self._set_bw(bw)
        self._set_cr(cr)
        self._set_sf(sf)

        # AGC automático
        self._write_reg(REG_MODEM_CONFIG_3, 0x04)

        # Modo standby
        self._write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_STDBY)
        time.sleep(0.01)

        print(f"[SX1276] Configurado — {frequency_mhz} MHz SF{sf} BW{bw//1000}kHz CR4/{cr}")
        return True

    def start_rx(self):
        """Coloca o módulo em modo de recepção contínua."""
        self._write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_RX_CONTINUOUS)

    def packet_available(self) -> bool:
        """Retorna True se há um pacote disponível (polling via IRQ flag)."""
        return bool(self._read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE)

    def read_packet(self) -> tuple[bytes, int, float]:
        """
        Lê o pacote disponível do FIFO.
        Retorna: (payload: bytes, rssi: int, snr: float)
        """
        irq = self._read_reg(REG_IRQ_FLAGS)

        # Limpa flags IRQ
        self._write_reg(REG_IRQ_FLAGS, irq)

        if irq & IRQ_CRC_ERROR:
            return b"", 0, 0.0

        nb_bytes  = self._read_reg(REG_RX_NB_BYTES)
        rx_addr   = self._read_reg(REG_FIFO_RX_CURRENT_ADDR)
        self._write_reg(REG_FIFO_ADDR_PTR, rx_addr)

        payload = bytes(self._read_burst(REG_FIFO, nb_bytes))

        # RSSI e SNR
        snr_raw = self._read_reg(REG_PKT_SNR_VALUE)
        snr     = snr_raw / 4.0 if snr_raw < 128 else (snr_raw - 256) / 4.0
        rssi    = self._read_reg(REG_PKT_RSSI_VALUE) - 157

        return payload, rssi, snr

    def close(self):
        self._write_reg(REG_OP_MODE, MODE_LONG_RANGE | MODE_SLEEP)
        self._spi.close()
        GPIO.cleanup()

    # ── Privados ──────────────────────────────────────────────────────────────

    def _reset(self):
        GPIO.output(self._pin_rst, GPIO.LOW)
        time.sleep(0.01)
        GPIO.output(self._pin_rst, GPIO.HIGH)
        time.sleep(0.01)

    def _write_reg(self, reg: int, value: int):
        self._spi.xfer2([reg | 0x80, value & 0xFF])

    def _read_reg(self, reg: int) -> int:
        return self._spi.xfer2([reg & 0x7F, 0x00])[1]

    def _read_burst(self, reg: int, length: int) -> list[int]:
        return self._spi.xfer2([reg & 0x7F] + [0x00] * length)[1:]

    def _set_frequency(self, mhz: float):
        frf = int((mhz * 1_000_000) / FSTEP)
        self._write_reg(REG_FRF_MSB, (frf >> 16) & 0xFF)
        self._write_reg(REG_FRF_MID, (frf >> 8)  & 0xFF)
        self._write_reg(REG_FRF_LSB,  frf         & 0xFF)

    def _set_bw(self, bw: int):
        bw_map = {
            7800: 0, 10400: 1, 15600: 2, 20800: 3,
            31250: 4, 41700: 5, 62500: 6, 125000: 7,
            250000: 8, 500000: 9
        }
        bw_val = bw_map.get(bw, 7)  # default 125kHz
        cfg1 = self._read_reg(REG_MODEM_CONFIG_1)
        self._write_reg(REG_MODEM_CONFIG_1, (cfg1 & 0x0F) | (bw_val << 4))

    def _set_cr(self, cr: int):
        # cr = 5..8 → valor 1..4
        cr_val = max(1, min(4, cr - 4))
        cfg1 = self._read_reg(REG_MODEM_CONFIG_1)
        self._write_reg(REG_MODEM_CONFIG_1, (cfg1 & 0xF1) | (cr_val << 1))

    def _set_sf(self, sf: int):
        sf = max(6, min(12, sf))
        cfg2 = self._read_reg(REG_MODEM_CONFIG_2)
        self._write_reg(REG_MODEM_CONFIG_2, (cfg2 & 0x0F) | (sf << 4))
        # Otimizações para SF11/SF12
        if sf >= 11:
            cfg3 = self._read_reg(REG_MODEM_CONFIG_3)
            self._write_reg(REG_MODEM_CONFIG_3, cfg3 | 0x08)