# Grafana — Captura Serial e Visualização

Módulo de observabilidade do projeto Labirinto para a disciplina de Sistemas Embarcados 2026.1.

Este diretório contém o bridge Python (`serial_bridge/bridge.py`) responsável por ler dados JSON de orientação (MPU6050) transmitidos pelo ESP32-S3 via UART e inseri-los em um banco InfluxDB v2.x, além do script Bash (`start_system.sh`) que orquestra a inicialização do sistema, levanta o servidor do Gêmeo Digital 3D e abre automaticamente o dashboard do Grafana no navegador.

## Pré-requisitos

### Softwares

| Software | Versão mínima | Finalidade |
| --- | --- | --- |
| Python | 3.10+ | Execução do bridge de captura serial |
| InfluxDB | 2.x | Armazenamento de séries temporais |
| Grafana | 10.x+ | Visualização dos dashboards |

Os três serviços devem estar instalados e rodando localmente como serviços `systemd`.

### Dependências Python

As bibliotecas abaixo devem ser instaladas dentro de um ambiente virtual (`venv`) localizado em `serial_bridge/venv/`:

| Biblioteca | Finalidade |
| --- | --- |
| `pyserial` | Comunicação com a porta serial (UART 115200 bps) |
| `influxdb-client` | Client oficial para escrita no InfluxDB 2.x |

Instalação do ambiente virtual e dependências:

```bash
cd serial_bridge/
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt

```

## Configurações Avançadas do Grafana (Servidor)

Para que o Gêmeo Digital 3D funcione corretamente e os gráficos atualizem em alta velocidade, é obrigatório alterar o arquivo de configuração principal do Grafana (`/etc/grafana/grafana.ini`).

Edite o arquivo no terminal com privilégios de administrador:

```bash
sudo nano /etc/grafana/grafana.ini

```

### 1. Taxa de Atualização de Alta Frequência

O Grafana bloqueia atualizações em intervalos menores que 5 segundos por padrão. Para acompanhar a dinâmica física da mesa em tempo real, precisamos remover essa trava.

* Procure a seção `[dashboards]`.
* Remova o ponto e vírgula (`;`) do início da linha para ativá-la.
* Altere o valor para `500ms`:

```ini
[dashboards]
min_refresh_interval = 500ms

```

### 2. Permissão de Iframes (Gêmeo Digital 3D)

O sistema de segurança nativo (Sanitizador HTML) bloqueia a exibição de iframes, deixando a tela do painel branca. Para liberar a visualização do modelo Three.js, desative essa proteção:

* Procure a seção `[panels]`.
* Descomente a linha apagando o `;` e ative a permissão alterando o valor para `true`:

```ini
[panels]
disable_sanitize_html = true

```

Após fazer as duas alterações, salve o arquivo e reinicie o serviço para aplicar as novas regras:

```bash
sudo systemctl restart grafana-server

```

## Configuração do InfluxDB v2.x

Após instalar o InfluxDB e garantir que o serviço está ativo (`systemctl status influxdb`), siga os passos abaixo pela interface web:

1. **Acessar a interface web** em `http://localhost:8086`.
2. **Criar a Organização**:
* Navegue até **Settings** → **Organizations** → **Create Organization**.
* Informe o nome exato: `ifpb`.


3. **Criar o Bucket**:
* Navegue até **Load Data** → **Buckets** → **Create Bucket**.
* Informe o nome exato: `labirinto`.
* Defina a política de retenção conforme necessário (ou deixe como padrão para retenção indefinida).


4. **Gerar o API Token**:
* Navegue até **Load Data** → **API Tokens** → **Generate API Token**.
* Selecione **All Access API Token** para ambientes de desenvolvimento, ou **Custom API Token** com permissões de leitura e escrita restritas ao bucket `labirinto` para ambientes mais controlados.
* **Copie e guarde o token gerado**. Ele será utilizado como segundo argumento do script `start_system.sh`.



## Configuração do Grafana e Importação do Dashboard

### Criação da Fonte de Dados

1. **Acessar a interface web** do Grafana em `http://localhost:3000`.
2. **Adicionar fonte de dados**:
   - Navegue até **Connections** → **Data Sources** → **Add data source**.
   - Selecione **InfluxDB**.
3. **Configurar a conexão**:
   - Altere a **Query Language** para **Flux** (obrigatório).
   - Preencha os campos:
     - **URL**: `http://localhost:8086`
     - **Organization**: `ifpb`
     - **Token**: o token gerado na etapa anterior.
     - **Default Bucket**: `labirinto`
   - Clique em **Save & Test** e verifique a mensagem de sucesso.

### Importação do Dashboard

1. Navegue até **Dashboards** → **Import** (ou clique no ícone **+** → **Import dashboard**).
2. Clique em **Upload dashboard JSON file** e selecione o arquivo `dashboards/DashboardGrafana.json` localizado neste diretório.
3. Na tela de configuração, selecione a **fonte de dados InfluxDB** recém-criada no campo correspondente.
4. Clique em **Import** para confirmar.

O dashboard importado estará imediatamente disponível. Para ativar a atualização rápida configurada anteriormente, selecione **500ms** no menu de recarregamento no canto superior direito do painel.

## Gêmeo Digital 3D (Three.js)

O modelo 3D da mesa (localizado na pasta `painel_3d/`) atua como um cliente independente no navegador, consumindo dados do InfluxDB via API.

### Sincronização de Velocidade

O Grafana e o banco de dados estão configurados para atualizar a cada 500ms. Para evitar a "dessincronização de relógios" e o arrasto visual do modelo 3D, o arquivo JavaScript precisa consultar o banco de forma agressiva e atualizar a posição instantaneamente, sem aplicar cálculos extras de suavização.

Certifique-se de que o final do seu arquivo `painel_3d/index.js` esteja configurado da seguinte maneira:

```javascript
// Consulta o banco a cada 50 milissegundos para capturar a mudança exata no InfluxDB
setInterval(fetchInflux, 50);

function animate() {
    requestAnimationFrame(animate);
    
    // Iguala a variável diretamente. A mesa vai travar na posição instantaneamente, no mesmo compasso do Grafana
    mesa.rotation.x = targetPitch;
    mesa.rotation.z = -targetRoll; // Inversão de eixo para espelhar o comportamento visual correto
    
    renderer.render(scene, camera);
}

```

## Formato de Dados Esperado

O firmware do ESP32-S3 transmite via UART (115200 bps) linhas JSON no seguinte formato:

```json
{
  "sensor": "mpu6050",
  "pitch_x_deg": 1.23,
  "roll_y_deg": -0.45,
  "accel_pitch_deg": 1.10,
  "accel_roll_deg": -0.50,
  "dt_ms": 20.0,
  "accel_g": {
    "x": 0.012,
    "y": -0.008,
    "z": 0.998
  },
  "gyro_dps": {
    "x": 0.03,
    "y": -0.02,
    "z": 0.01
  }
}
```

O bridge detecta automaticamente a porta serial (`/dev/ttyACM*` ou `/dev/ttyUSB*`), achata os campos aninhados (`accel_g.x` → `accel_g_x`) e insere no InfluxDB sob o measurement `orientacao_mesa` com a tag `sensor="mpu6050"`.

## Como Executar

### Uso do `start_system.sh`

O script orquestrador opera com validação estrita (Fail-Fast) e exige exatamente 4 argumentos posicionais:

```bash
./start_system.sh <INFLUXDB_URL> <INFLUXDB_TOKEN> <INFLUXDB_ORG> <INFLUXDB_BUCKET>
```

| Argumento | Descrição |
| --- | --- |
| `INFLUXDB_URL` | URL do servidor InfluxDB |
| `INFLUXDB_TOKEN` | Token de autenticação da API |
| `INFLUXDB_ORG` | Organização configurada no InfluxDB |
| `INFLUXDB_BUCKET` | Bucket de destino dos dados |

Exemplo prático:

```bash
./start_system.sh "http://localhost:8086" "SEU_TOKEN" "ifpb" "labirinto"
```

### Comportamento

1. Valida os 4 argumentos. Encerra com mensagem de erro se algum estiver ausente.
2. Exporta as credenciais como variáveis de ambiente (`INFLUXDB_URL`, `INFLUXDB_TOKEN`, `INFLUXDB_ORG`, `INFLUXDB_BUCKET`).
3. Verifica se o serviço Grafana está ativo via `systemctl`.
4. **Inicia automaticamente o servidor web local do Gêmeo Digital na porta 8000 (em segundo plano).**
5. Abre automaticamente o navegador padrão em `http://localhost:3000` (dashboard do Grafana).
6. Ativa o ambiente virtual e inicia o bridge de captura no terminal.

O bridge escaneia as portas seriais disponíveis e mantém a captura contínua. Para encerrar o sistema, utilize `Ctrl+C` no terminal. O script interromperá automaticamente o bridge e o servidor web do Gêmeo Digital.

## Estrutura do Diretório

```text
Grafana/
├── README.md
├── start_system.sh
├── dashboards/
│   └── DashboardGrafana.json
├── painel_3d/
│   ├── index.html
│   └── index.js
└── serial_bridge/
    ├── .gitignore
    ├── bridge.py
    ├── serial_scanner.py
    ├── influx_writer.py
    ├── requirements.txt
    └── venv/
```
