# Arquitetura das Fases 1, 2 e 3

## Objetivo

O firmware controla uma mesa labirinto de dois eixos. A Fase 1 implementa o controle local por joystick analogico e servos. A Fase 2 adiciona a leitura de orientacao da mesa com MPU6050 e o envio periodico desses dados pela serial. A Fase 3 fecha o ciclo com um gemeo digital em tempo real: um bridge Python captura os dados da serial, grava no InfluxDB e os exibe em um dashboard Grafana com modelo 3D interativo.

Como funcionalidade extra de bonificacao, foi adicionado um componente independente `bsp_victory_sensor` que detecta a chegada da bolinha na saida do labirinto por sensor optico.

## Visao Geral

```text
main
└── app_labyrinth
    ├── bsp_board
    ├── bsp_joystick
    ├── bsp_servo
    ├── bsp_mpu6050
    ├── bsp_victory_sensor   ← bonus
    └── rtos_port
```

`app_labyrinth` e a camada de orquestracao. Ela inicializa os drivers, cria as filas e sobe as tarefas FreeRTOS. Os detalhes de hardware ficam isolados nos componentes BSP, e o acesso ao FreeRTOS fica encapsulado em `rtos_port`.

## Camadas

### Aplicacao

- `main`: ponto de entrada minimo da aplicacao.
- `app_labyrinth`: inicializacao, criacao de filas, controle das tarefas, logs, relaxamento dos servos e envio serial.

### BSP

- `bsp_board`: pinagem, canais ADC, centros calibrados do joystick e GPIOs do MPU6050/servos/sensor de vitoria.
- `bsp_joystick`: leitura ADC oneshot, media aparada, filtro EMA, zona morta e normalizacao para percentual.
- `bsp_servo`: controle MCPWM de 50 Hz, mapeamento percentual para largura de pulso e liga/desliga do PWM.
- `bsp_mpu6050`: barramento I2C, configuracao do sensor, calibracao do giroscopio, filtros e calculo de `pitch`/`roll`.
- `bsp_victory_sensor` *(bonus)*: GPIO do sensor optico de reflexao e do LED de vitoria. Encapsula polaridade ativa-low e expoe apenas `bool detected` para a aplicacao.

### Porta RTOS

- `rtos_port`: interface pequena para tarefas, filas e delays. A aplicacao usa essa camada em vez de chamar FreeRTOS diretamente.

### Observabilidade (Fase 3)

O modulo de observabilidade reside em `Grafana/` e e independente do firmware:

- `serial_bridge/bridge.py`: le continuamente o JSON transmitido pelo ESP32-S3 via UART e escreve no InfluxDB v2.x.
- `serial_bridge/serial_scanner.py`: detecta automaticamente a porta serial disponivel (`/dev/ttyACM*` ou `/dev/ttyUSB*`).
- `serial_bridge/influx_writer.py`: acha os campos aninhados do JSON (`accel_g.x` → `accel_g_x`) e grava o measurement `orientacao_mesa`.
- `painel_3d/`: pagina web com modelo Three.js da mesa. Consome dados do InfluxDB via API e exibido como iframe no Grafana.
- `start_system.sh`: orquestrador Bash que valida credenciais, gera `env.js` dinamicamente, verifica o Grafana e inicia o bridge.
- `dashboards/DashboardGrafana.json`: dashboard exportado com graficos de pitch/roll, gauge de orientacao e iframe do gemeo 3D.

## Fluxo da Fase 1

```text
Joystick ADC
    │
    ▼
bsp_joystick
    │
    ▼
joystick_task
    ├── servo_queue ──► servo_task ──► bsp_servo ──► PWM dos servos
    └── debug_queue ──► status_task ─► ESP_LOG/UART
```

O joystick e lido a cada 20 ms. A amostra processada segue para duas filas:

- `servo_queue`: usada pela `servo_task` para mover a mesa.
- `debug_queue`: usada pela `status_task` para imprimir status em baixa frequencia.

A `servo_task` limita o comando em 15%, suaviza o movimento por passos e desliga o PWM quando o eixo volta ao centro e permanece estavel.

## Fluxo da Fase 2

```text
MPU6050
    │ I2C
    ▼
bsp_mpu6050
    │ accel + gyro + pitch/roll filtrados
    ▼
mpu6050_task
    │
    ▼
JSON via UART (115200 bps, a cada 0,5 s)
```

A `mpu6050_task` roda a cada 20 ms, le acelerometro e giroscopio via I2C, calcula `pitch` e `roll` e envia uma linha JSON pela serial a cada 0,5 s no seguinte formato:

```json
{"sensor":"mpu6050","pitch_x_deg":1.23,"roll_y_deg":-0.45,"accel_pitch_deg":1.10,"accel_roll_deg":-0.50,"dt_ms":20.0,"accel_g":{"x":0.012,"y":-0.008,"z":0.998},"gyro_dps":{"x":0.03,"y":-0.02,"z":0.01}}
```

## Fluxo da Fase 3 — Gemeo Digital

```text
JSON via UART
    │
    ▼
serial_bridge/bridge.py
    │ pyserial + influxdb-client
    ▼
InfluxDB v2.x
    │ measurement: orientacao_mesa
    │ tag: sensor="mpu6050"
    ├──► Grafana (Flux queries)
    │       ├── graficos pitch/roll em tempo real
    │       ├── gauge de orientacao
    │       └── iframe do gemeo 3D
    └──► painel_3d/index.js (fetch API InfluxDB)
             └── Three.js ──► malha 3D da mesa rotacionada em tempo real
```

O bridge detecta automaticamente a porta serial, acha os campos aninhados do JSON e os insere no InfluxDB. O Grafana consulta o banco via Flux e atualiza os paineis a cada 500 ms. O gemeo 3D faz requisicoes diretas a API do InfluxDB e aplica os angulos de pitch e roll ao modelo Three.js, sincronizando o movimento virtual ao fisico.

## Fluxo do Bonus — Sensor de Vitoria

```text
Sensor optico GPIO17 (ativo-low)
    │
    ▼
bsp_victory_sensor
    │ bool detected
    ▼
victory_task
    ├── filtro: 3 leituras consecutivas
    ├── detectado: gpio_set_level(LED, 1) + ESP_LOGI
    └── ausente:   gpio_set_level(LED, 0) + ESP_LOGI
```

A polaridade ativa-low e encapsulada no BSP; a tarefa recebe apenas `bool detected`.

## Tarefas FreeRTOS

| Tarefa | Prioridade | Periodicidade | Responsabilidade |
| --- | ---: | --- | --- |
| `joystick_task` | 5 | 20 ms | Le joystick, filtra, normaliza e publica amostras. |
| `servo_task` | 4 | Sob demanda | Consome comandos do joystick, suaviza movimento e atualiza os servos. |
| `mpu6050_task` | 4 | 20 ms | Le MPU6050, calcula orientacao e envia JSON a cada 0,5 s. |
| `status_task` | 3 | 1 s | Imprime status de joystick, servos e estado do PWM. |
| `victory_task` *(bonus)* | 3 | 50 ms | Le sensor optico, aplica filtro e controla LED de vitoria. |

## Inicializacao

```text
app_labyrinth_start
    ├── bsp_board_init
    ├── bsp_joystick_init
    ├── bsp_servo_timer_init
    ├── bsp_servo_init X/Y
    ├── centraliza servos
    ├── desliga PWM apos estabilizacao inicial
    ├── bsp_mpu6050_init
    │   ├── cria barramento I2C
    │   ├── valida WHO_AM_I
    │   ├── configura escala/filtro interno
    │   └── calibra offset do giroscopio
    ├── bsp_victory_sensor_init (bonus)
    │   ├── configura GPIO17 como entrada com pull-up
    │   └── configura GPIO6 como saida, LED apagado
    ├── cria servo_queue/debug_queue
    └── cria joystick_task, servo_task, mpu6050_task, status_task e victory_task
```

## Calibracao e Filtros

### Joystick

- Os centros dos eixos ficam em `BSP_JOYSTICK_X_CENTER_RAW` e `BSP_JOYSTICK_Y_CENTER_RAW`.
- A zona morta elimina pequenas variacoes perto do centro.
- O filtro EMA usa resposta lenta para ruido pequeno e resposta rapida para movimentos bruscos.

### Servos

- O comando final fica limitado a `-15%` ate `+15%`.
- O movimento e suavizado por passos em decimos de percentual.
- Quando o comando e o estado atual voltam a zero, o PWM e desligado apos algumas amostras estaveis.

### MPU6050

- A calibracao inicial mede o offset do giroscopio.
- A calibracao so e aceita diretamente quando a variacao do giroscopio fica abaixo do limite configurado.
- O acelerometro passa por filtro EMA.
- `pitch` e `roll` usam filtro complementar, combinando giroscopio para resposta rapida e acelerometro para corrigir deriva.

### Sensor de Vitoria (bonus)

- O limiar de reflexao e ajustado fisicamente pelo potenciometro do sensor optico.
- O filtro de software exige 3 leituras consecutivas de deteccao antes de acionar o LED.
- A remocao da bolinha zera o contador imediatamente e apaga o LED sem atraso adicional.

## Saidas Seriais

- JSON do MPU6050: enviado com `printf` a cada 0,5 s — consumido pelo bridge Python.
- Logs da aplicacao: enviados com `ESP_LOG` a cada 1 s.
- Logs de vitoria *(bonus)*: enviados com `ESP_LOGI`/`ESP_LOGW` a cada transicao de estado.

## Pontos de Configuracao

### Firmware

- `components/bsp_board/include/bsp_board.h`: pinagem, canais ADC, centro do joystick, pinos I2C e GPIOs do sensor/LED de vitoria.
- `components/bsp_joystick/src/bsp_joystick.c`: zona morta, filtros e normalizacao.
- `components/bsp_servo/src/bsp_servo.c`: pulsos minimo/centro/maximo e limite fisico do PWM.
- `components/bsp_mpu6050/src/bsp_mpu6050.c`: taxa I2C, amostras de calibracao, limiar de estabilidade e filtros.
- `components/app_labyrinth/src/app_labyrinth.c`: periodicidade das tasks, limite de saida dos servos, frequencia de logs, JSON e parametros do sensor de vitoria (`VICTORY_SENSOR_PERIOD_MS`, `VICTORY_FILTER_CONFIRM_COUNT`).

### Observabilidade

- `Grafana/start_system.sh`: URL, token, org e bucket do InfluxDB passados como argumentos posicionais.
- `Grafana/painel_3d/env.js`: gerado dinamicamente pelo `start_system.sh` com as credenciais do InfluxDB para o gemeo 3D.
- `/etc/grafana/grafana.ini`: `min_refresh_interval = 500ms` e `disable_sanitize_html = true` para alta frequencia e iframe do gemeo.
