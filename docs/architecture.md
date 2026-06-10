# Arquitetura das Fases 1 e 2

## Objetivo

O firmware controla uma mesa labirinto de dois eixos. A Fase 1 implementa o controle local por joystick analogico e servos. A Fase 2 adiciona a leitura de orientacao da mesa com MPU6050 e o envio periodico desses dados pela serial.

## Visao Geral

```text
main
└── app_labyrinth
    ├── bsp_board
    ├── bsp_joystick
    ├── bsp_servo
    ├── bsp_mpu6050
    └── rtos_port
```

`app_labyrinth` e a camada de orquestracao. Ela inicializa os drivers, cria as filas e sobe as tarefas FreeRTOS. Os detalhes de hardware ficam isolados nos componentes BSP, e o acesso ao FreeRTOS fica encapsulado em `rtos_port`.

## Camadas

### Aplicacao

- `main`: ponto de entrada minimo da aplicacao.
- `app_labyrinth`: inicializacao, criacao de filas, controle das tarefas, logs, relaxamento dos servos e envio serial.

### BSP

- `bsp_board`: pinagem, canais ADC, centros calibrados do joystick e GPIOs do MPU6050/servos.
- `bsp_joystick`: leitura ADC oneshot, media aparada, filtro EMA, zona morta e normalizacao para percentual.
- `bsp_servo`: controle MCPWM de 50 Hz, mapeamento percentual para largura de pulso e liga/desliga do PWM.
- `bsp_mpu6050`: barramento I2C, configuracao do sensor, calibracao do giroscopio, filtros e calculo de `pitch`/`roll`.

### Porta RTOS

- `rtos_port`: interface pequena para tarefas, filas e delays. A aplicacao usa essa camada em vez de chamar FreeRTOS diretamente.

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

A `servo_task` limita o comando em 15%, suaviza o movimento por passos e desliga o PWM quando o eixo volta ao centro e permanece estavel. Isso reduz esforco mecanico e evita o servo tentando corrigir ruido quando a mesa esta parada.

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
JSON via UART
```

A `mpu6050_task` e exclusiva para o sensor. Ela roda a cada 20 ms, le acelerometro e giroscopio via I2C, calcula `pitch` e `roll` e envia uma linha JSON pela serial a cada 0,5 s.

O envio serial segue este formato:

```json
{"sensor":"mpu6050","pitch_x_deg":1.23,"roll_y_deg":-0.45,"accel_pitch_deg":1.10,"accel_roll_deg":-0.50,"dt_ms":20.0,"accel_g":{"x":0.012,"y":-0.008,"z":0.998},"gyro_dps":{"x":0.03,"y":-0.02,"z":0.01}}
```

## Tarefas FreeRTOS

| Tarefa | Prioridade | Periodicidade | Responsabilidade |
| --- | ---: | --- | --- |
| `joystick_task` | 5 | 20 ms | Le joystick, filtra, normaliza e publica amostras. |
| `servo_task` | 4 | Sob demanda | Consome comandos do joystick, suaviza movimento e atualiza os servos. |
| `mpu6050_task` | 4 | 20 ms | Le MPU6050, calcula orientacao e envia JSON a cada 0,5 s. |
| `status_task` | 3 | 1 s | Imprime status de joystick, servos e estado do PWM. |

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
    ├── cria servo_queue/debug_queue
    └── cria joystick_task, servo_task, mpu6050_task e status_task
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

## Saidas Seriais

- JSON do MPU6050: enviado com `printf` a cada 0,5 s.
- Logs da aplicacao: enviados com `ESP_LOG` a cada 1 s.

Os logs de status incluem joystick bruto/processado, alvo e posicao atual dos servos, ultimo pulso calculado e estado do PWM (`on`/`off`).

## Pontos de Configuracao

- `components/bsp_board/include/bsp_board.h`: pinagem, canais ADC, centro do joystick e pinos I2C.
- `components/bsp_joystick/src/bsp_joystick.c`: zona morta, filtros e normalizacao.
- `components/bsp_servo/src/bsp_servo.c`: pulsos minimo/centro/maximo e limite fisico do PWM.
- `components/bsp_mpu6050/src/bsp_mpu6050.c`: taxa I2C, amostras de calibracao, limiar de estabilidade e filtros.
- `components/app_labyrinth/src/app_labyrinth.c`: periodicidade das tasks, limite de saida dos servos, frequencia de logs e JSON.
