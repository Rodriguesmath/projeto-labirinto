"""@file bridge.py
@brief Serviço de captura serial → InfluxDB com reconexão automática.

Ponto de entrada do bridge. Implementa a máquina de estados
Scanning ↔ Capturing com watchdog de reconexão e tratamento
de sinais para shutdown limpo.
"""

import json
import logging
import signal
import sys
import time

import serial

from influx_writer import InfluxWriter
from serial_scanner import scan_for_device

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("bridge")

SCAN_INTERVAL_S = 3.0
EXPECTED_SENSOR_TAG = "mpu6050"


class Bridge:
    """@brief Máquina de estados do bridge serial → InfluxDB.

    Opera em dois modos: Scanning (busca por porta) e Capturing (leitura
    contínua). Transiciona automaticamente entre eles conforme a
    disponibilidade da porta serial.
    """

    def __init__(self):
        """@brief Inicializa o bridge com estado Scanning."""
        self._serial = None
        self._writer = InfluxWriter()
        self._running = True

        signal.signal(signal.SIGINT, self._handle_shutdown)
        signal.signal(signal.SIGTERM, self._handle_shutdown)

    def _handle_shutdown(self, signum, frame):
        """@brief Handler de sinal para encerramento limpo.

        @param signum Número do sinal recebido.
        @param frame Stack frame (não utilizado).
        """
        sig_name = signal.Signals(signum).name
        logger.info("Sinal %s recebido. Encerrando...", sig_name)
        self._running = False

    def _close_serial(self):
        """@brief Fecha a porta serial se estiver aberta."""
        if self._serial and self._serial.is_open:
            port_name = self._serial.port
            self._serial.close()
            logger.info("Porta %s fechada.", port_name)
        self._serial = None

    def _scan_loop(self):
        """@brief Loop de busca por porta serial válida.

        Permanece escaneando a cada SCAN_INTERVAL_S até encontrar um
        dispositivo ou receber sinal de shutdown.
        """
        logger.info("Aguardando dispositivo serial...")
        while self._running:
            self._serial = scan_for_device()
            if self._serial:
                return
            time.sleep(SCAN_INTERVAL_S)

    def _capture_loop(self):
        """@brief Loop de captura contínua dos dados seriais.

        Lê linhas da porta serial, faz parse do JSON e envia ao InfluxDB.
        Retorna ao scan_loop se a porta for perdida.
        """
        port_name = self._serial.port
        logger.info("Capturando dados de %s...", port_name)

        while self._running:
            try:
                raw = self._serial.readline()
            except (serial.SerialException, OSError) as exc:
                logger.warning("Conexão perdida em %s: %s", port_name, exc)
                self._on_disconnect()
                return

            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            try:
                data = json.loads(line)
            except (json.JSONDecodeError, ValueError):
                continue

            if data.get("sensor") != EXPECTED_SENSOR_TAG:
                continue

            self._writer.write_sample(data)

    def _on_disconnect(self):
        """@brief Ações executadas ao detectar perda da porta serial."""
        self._writer.write_status(0)
        self._close_serial()

    def run(self):
        """@brief Loop principal da máquina de estados do bridge.

        Alterna entre Scanning e Capturing até receber sinal de shutdown.
        """
        logger.info("Bridge serial → InfluxDB iniciado.")

        try:
            while self._running:
                self._scan_loop()
                if not self._running:
                    break
                self._capture_loop()
        finally:
            self._on_disconnect()
            self._writer.close()
            logger.info("Bridge encerrado.")


if __name__ == "__main__":
    InfluxWriter.validate_env()
    bridge = Bridge()
    bridge.run()
