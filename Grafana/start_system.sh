#!/usr/bin/env bash
##
## @file start_system.sh
## @brief Orquestrador do sistema de monitoramento da mesa labirinto.
##
## Ponto de entrada principal que gera as credenciais do Gêmeo Digital,
## verifica o estado do Grafana e inicia o bridge de captura serial.
##
## Uso:
##   ./start_system.sh <URL> <TOKEN> <ORG> <BUCKET>
##
## Exemplo:
##   ./start_system.sh http://localhost:8086 meu_token ifpb labirinto
##

set -euo pipefail

# ── Validação de parâmetros (Fail-Fast) ──────────────────────────────────────

if [ "$#" -ne 4 ]; then
    echo "Erro: número de parâmetros inválido (recebido: $#, esperado: 4)." >&2
    echo "" >&2
    echo "Uso: $0 <INFLUXDB_URL> <INFLUXDB_TOKEN> <INFLUXDB_ORG> <INFLUXDB_BUCKET>" >&2
    echo "Exemplo: $0 http://localhost:8086 meu_token ifpb labirinto" >&2
    exit 1
fi

# ── Exportação das credenciais InfluxDB ──────────────────────────────────────

export INFLUXDB_URL="$1"
export INFLUXDB_TOKEN="$2"
export INFLUXDB_ORG="$3"
export INFLUXDB_BUCKET="$4"

echo "[start_system] Credenciais InfluxDB configuradas (org=$INFLUXDB_ORG, bucket=$INFLUXDB_BUCKET)."

# ── Grafana (preparação para futura inicialização do painel) ─────────────────

if systemctl is-active --quiet grafana-server || systemctl is-active --quiet grafana; then
    echo "[start_system] Grafana está rodando."
else
    echo "[start_system] Aviso: Grafana não está rodando. Considere iniciar com 'sudo systemctl start grafana-server'." >&2
fi

# ── Abertura do Dashboard no navegador ───────────────────────────────────────

GRAFANA_URL="http://localhost:3000"

echo "[start_system] Abrindo o Dashboard no navegador..."

if command -v xdg-open &>/dev/null; then
    xdg-open "$GRAFANA_URL" &>/dev/null &
else
    python -m webbrowser "$GRAFANA_URL" &>/dev/null &
fi

# ── Configuração do Gêmeo Digital 3D ─────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PAINEL_DIR="${SCRIPT_DIR}/painel_3d"

if [ -d "$PAINEL_DIR" ]; then
    echo "[start_system] Injetando variáveis no painel 3D..."
    
    # Gera o arquivo env.js com os parâmetros passados no terminal
    cat <<EOF > "${PAINEL_DIR}/env.js"
export const ENV = {
    URL: "${INFLUXDB_URL}",
    TOKEN: "${INFLUXDB_TOKEN}",
    ORG: "${INFLUXDB_ORG}",
    BUCKET: "${INFLUXDB_BUCKET}"
};
EOF
    echo "[start_system] Painel configurado. Lembre-se de iniciar o servidor web manualmente na pasta painel_3d/."
else
    echo "[start_system] Aviso: Diretório do painel 3D não encontrado em $PAINEL_DIR." >&2
fi

# ── Execução do Bridge serial → InfluxDB ─────────────────────────────────────

VENV_ACTIVATE="${SCRIPT_DIR}/serial_bridge/venv/bin/activate"
BRIDGE_SCRIPT="${SCRIPT_DIR}/serial_bridge/bridge.py"

if [ ! -f "$VENV_ACTIVATE" ]; then
    echo "Erro: venv não encontrada em ${VENV_ACTIVATE}" >&2
    echo "Execute primeiro: python -m venv serial_bridge/venv && source serial_bridge/venv/bin/activate && pip install -r serial_bridge/requirements.txt" >&2
    exit 1
fi

echo "[start_system] Iniciando bridge serial..."

# shellcheck source=/dev/null
source "$VENV_ACTIVATE"

# Assume o processo atual, economizando recursos do terminal
exec python "$BRIDGE_SCRIPT"