"""@file serial_scanner.py
@brief Auto-detecção da porta serial do ESP32 transmitindo JSON do MPU6050.

Escaneia portas /dev/ttyUSB* e /dev/ttyACM*, abre cada candidata e valida
se os dados recebidos são JSON com o campo sensor="mpu6050".
"""

import glob
import json
import logging

import serial

logger = logging.getLogger(__name__)

SERIAL_BAUDRATE = 115200
SERIAL_TIMEOUT_S = 2.0
SCAN_PATTERNS = ["/dev/ttyACM*", "/dev/ttyUSB*"]
MAX_PROBE_LINES = 10
EXPECTED_SENSOR_TAG = "mpu6050"


def _list_candidate_ports():
    """@brief Enumera portas seriais candidatas no sistema.

    @return Lista de paths absolutos encontrados via glob.
    """
    ports = []
    for pattern in SCAN_PATTERNS:
        ports.extend(sorted(glob.glob(pattern)))
    return ports


def _is_valid_mpu6050_json(line):
    """@brief Verifica se uma linha é JSON válido do MPU6050.

    @param line String bruta lida da serial.
    @return Booleano indicando se o JSON contém sensor="mpu6050".
    """
    try:
        data = json.loads(line)
        return isinstance(data, dict) and data.get("sensor") == EXPECTED_SENSOR_TAG
    except (json.JSONDecodeError, ValueError):
        return False


def scan_for_device():
    """@brief Escaneia portas seriais e retorna a primeira com dados válidos.

    Abre cada porta candidata a 115200 bps, lê até MAX_PROBE_LINES linhas
    e verifica se alguma é JSON válido do MPU6050.

    @return Objeto serial.Serial aberto e pronto, ou None se nenhuma porta
            válida for encontrada.
    """
    candidates = _list_candidate_ports()
    if not candidates:
        logger.debug("Nenhuma porta serial candidata encontrada.")
        return None

    for port_path in candidates:
        logger.debug("Testando porta: %s", port_path)
        try:
            ser = serial.Serial(
                port=port_path,
                baudrate=SERIAL_BAUDRATE,
                timeout=SERIAL_TIMEOUT_S,
            )
            ser.reset_input_buffer()

            for _ in range(MAX_PROBE_LINES):
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if _is_valid_mpu6050_json(line):
                    logger.info("Dispositivo detectado em %s", port_path)
                    return ser

            ser.close()
        except (serial.SerialException, OSError) as exc:
            logger.debug("Porta %s indisponível: %s", port_path, exc)

    return None
