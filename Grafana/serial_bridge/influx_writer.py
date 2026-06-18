"""@file influx_writer.py
@brief Camada de escrita no InfluxDB 2.x para o measurement orientacao_mesa.

Encapsula o InfluxDBClient, constrói Points a partir dos dados JSON do ESP32
e expõe métodos para escrita de amostras e heartbeat de status.
"""

import logging
import os
import sys

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

logger = logging.getLogger(__name__)

MEASUREMENT = "orientacao_mesa"
TAG_SENSOR = "mpu6050"

# Campos de primeiro nível extraídos diretamente do JSON
_TOP_LEVEL_FIELDS = [
    "pitch_x_deg",
    "roll_y_deg",
    "accel_pitch_deg",
    "accel_roll_deg",
    "dt_ms",
]

# Mapeamento de campos aninhados para nomes achatados
_NESTED_FIELDS = {
    "accel_g": {"x": "accel_g_x", "y": "accel_g_y", "z": "accel_g_z"},
    "gyro_dps": {"x": "gyro_dps_x", "y": "gyro_dps_y", "z": "gyro_dps_z"},
}


_REQUIRED_ENV_VARS = ["INFLUXDB_URL", "INFLUXDB_TOKEN", "INFLUXDB_ORG", "INFLUXDB_BUCKET"]


class InfluxWriter:
    """@brief Writer síncrono para o InfluxDB 2.x.

    Depende estritamente de variáveis de ambiente para configuração
    de conexão. Nenhum valor padrão é fornecido.
    """

    @staticmethod
    def validate_env():
        """@brief Valida a presença das variáveis de ambiente obrigatórias.

        Verifica INFLUXDB_URL, INFLUXDB_TOKEN, INFLUXDB_ORG e INFLUXDB_BUCKET.
        Encerra o programa com mensagem de erro se alguma estiver ausente.
        """
        missing = [var for var in _REQUIRED_ENV_VARS if not os.environ.get(var)]
        if missing:
            for var in missing:
                print(f"ERRO: Variável de ambiente obrigatória ausente: {var}", file=sys.stderr)
            sys.exit(1)

    def __init__(self):
        """@brief Inicializa o client InfluxDB a partir de variáveis de ambiente."""
        self._url = os.environ["INFLUXDB_URL"]
        self._token = os.environ["INFLUXDB_TOKEN"]
        self._org = os.environ["INFLUXDB_ORG"]
        self._bucket = os.environ["INFLUXDB_BUCKET"]

        self._client = InfluxDBClient(
            url=self._url, token=self._token, org=self._org
        )
        self._write_api = self._client.write_api(write_options=SYNCHRONOUS)
        logger.info(
            "InfluxDB configurado: url=%s org=%s bucket=%s",
            self._url,
            self._org,
            self._bucket,
        )

    def _build_point(self, data):
        """@brief Constrói um Point a partir do dicionário JSON do ESP32.

        Achata os campos aninhados (accel_g, gyro_dps) para fields de
        primeiro nível, pois o InfluxDB não suporta hierarquia.

        @param data Dicionário Python parseado do JSON serial.
        @return Objeto Point pronto para escrita (sem timestamp — server-side).
        """
        point = Point(MEASUREMENT).tag("sensor", TAG_SENSOR)

        for key in _TOP_LEVEL_FIELDS:
            if key in data:
                point = point.field(key, float(data[key]))

        for nested_key, mapping in _NESTED_FIELDS.items():
            nested = data.get(nested_key, {})
            for sub_key, flat_name in mapping.items():
                if sub_key in nested:
                    point = point.field(flat_name, float(nested[sub_key]))

        return point

    def write_sample(self, data):
        """@brief Escreve uma amostra completa do MPU6050 no InfluxDB.

        Inclui connection_status=1 indicando leitura saudável.

        @param data Dicionário Python com os dados do sensor.
        """
        point = self._build_point(data).field("connection_status", 1)
        try:
            self._write_api.write(bucket=self._bucket, record=point)
        except Exception as exc:
            logger.error("Falha na escrita InfluxDB: %s", exc)

    def write_status(self, status):
        """@brief Envia um ponto de heartbeat com apenas o connection_status.

        @param status Inteiro: 1 para conectado, 0 para desconectado.
        """
        point = (
            Point(MEASUREMENT)
            .tag("sensor", TAG_SENSOR)
            .field("connection_status", int(status))
        )
        try:
            self._write_api.write(bucket=self._bucket, record=point)
        except Exception as exc:
            logger.error("Falha ao enviar heartbeat: %s", exc)

    def close(self):
        """@brief Fecha o client InfluxDB e libera recursos."""
        try:
            self._client.close()
            logger.info("Client InfluxDB encerrado.")
        except Exception as exc:
            logger.warning("Erro ao fechar client InfluxDB: %s", exc)
