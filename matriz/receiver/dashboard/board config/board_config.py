"""
board_config.py — Configuração de hardware SPI para pyLoRa
===========================================================
Ajuste os pinos GPIO conforme sua ligação física.

Pinagem padrão (Raspberry Pi 40 pinos → Ra-02 / RFM95):

    Raspberry Pi     Módulo LoRa
    ───────────────────────────
    3.3V     (Pin 1)  → VCC
    GND      (Pin 6)  → GND
    GPIO10   (Pin 19) → MOSI
    GPIO9    (Pin 21) → MISO
    GPIO11   (Pin 23) → SCK
    GPIO8/CE0(Pin 24) → NSS/CS
    GPIO22   (Pin 15) → RST
    GPIO25   (Pin 22) → DIO0

Se usar pinos diferentes, altere as constantes abaixo.
"""

import RPi.GPIO as GPIO
import spidev

# ── Pinos GPIO (numeração BCM) ────────────────────────────────────────────────
PIN_RST  = 22
PIN_DIO0 = 25
PIN_CS   = 8    # CE0 do SPI (controlado via spidev, não manualmente)

# ── SPI ───────────────────────────────────────────────────────────────────────
SPI_BUS  = 0
SPI_DEV  = 0
SPI_FREQ = 5_000_000  # 5 MHz


class BOARD:
    """Abstração de hardware compatível com pyLoRa."""

    spi = None

    @classmethod
    def setup(cls):
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)

        GPIO.setup(PIN_DIO0, GPIO.IN,  pull_up_down=GPIO.PUD_DOWN)
        GPIO.setup(PIN_RST,  GPIO.OUT)
        GPIO.setup(PIN_CS,   GPIO.OUT)

        GPIO.output(PIN_CS,  GPIO.HIGH)
        GPIO.output(PIN_RST, GPIO.HIGH)

        cls.spi = spidev.SpiDev()
        cls.spi.open(SPI_BUS, SPI_DEV)
        cls.spi.max_speed_hz = SPI_FREQ
        cls.spi.mode = 0b00  # SPI Mode 0

    @classmethod
    def reset(cls):
        GPIO.output(PIN_RST, GPIO.LOW)
        import time; time.sleep(0.01)
        GPIO.output(PIN_RST, GPIO.HIGH)
        import time; time.sleep(0.01)

    @classmethod
    def teardown(cls):
        if cls.spi:
            cls.spi.close()
        GPIO.cleanup()