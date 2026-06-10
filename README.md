# Projeto - Labirinto

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.1--dev-E7352C?logo=espressif&logoColor=white)
![Target](https://img.shields.io/badge/Target-ESP32--S3-222222?logo=espressif&logoColor=white)
![Language](https://img.shields.io/badge/Language-C-00599C?logo=c&logoColor=white)
![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-5C2D91)
![Docs](https://img.shields.io/badge/Docs-Doxygen-2C4AA8)
![Status](https://img.shields.io/badge/Fase%202-Build%20OK-2EA44F)

Firmware da segunda entrega do projeto final de Sistemas Embarcados 2026.1: uma mesa labirinto controlada por joystick analógico, com dois servomotores para inclinação nos eixos X/Y e leitura de orientação por MPU6050.

O projeto foi estruturado como uma aplicação ESP-IDF modular, com abstração de hardware por componente, documentação Doxygen e uma camada própria para isolar o uso direto do RTOS.

## Status Da Fase 2

Fase 2 implementada e com build validado em `10/06/2026`.

Funcionalidades entregues:

- Leitura via I2C dos dados de aceleração e giroscópio do MPU6050.
- Cálculo dos ângulos `pitch` no eixo X e `roll` no eixo Y.
- Filtro EMA no acelerômetro e filtro complementar acelerômetro/giroscópio.
- Calibração inicial do offset do giroscópio quando a mesa está estável dentro do limite configurado.
- Envio periódico via UART em JSON a cada 0,5 s.
- Logs de status da aplicação a cada 1 s.
- Tarefa FreeRTOS dedicada para o MPU6050: `mpu6050_task`.

Exemplo de saída serial do MPU6050:

```json
{"sensor":"mpu6050","pitch_x_deg":1.23,"roll_y_deg":-0.45,"accel_pitch_deg":1.10,"accel_roll_deg":-0.50,"dt_ms":20.0,"accel_g":{"x":0.012,"y":-0.008,"z":0.998},"gyro_dps":{"x":0.03,"y":-0.02,"z":0.01}}
```

## Escopo Da Fase 1

- Leitura analógica dos eixos X e Y do joystick.
- Filtragem e normalização dos comandos para a faixa de -100% a 100%.
- Conversão dos comandos em PWM de 50 Hz para dois servomotores.
- Movimento proporcional e suavizado da mesa.
- Organização em três tarefas, conforme requisito da entrega.
- Logs seriais para debug.
- Normalização do centro do joystick configurável por macros em `bsp_board.h`.
- Desligamento do PWM dos servos quando a mesa retorna ao repouso.

## Arquitetura

```text
main
└── app_labyrinth
    ├── bsp_board
    ├── bsp_joystick
    ├── bsp_mpu6050
    ├── bsp_servo
    └── rtos_port
```

| Camada | Responsabilidade |
| --- | --- |
| `main` | Ponto de entrada mínimo da aplicação. |
| `app_labyrinth` | Orquestra inicialização, filas, tarefas, logs e envio serial. |
| `bsp_board` | Centraliza pinagem, canais ADC, centros do joystick e configuração comum de board. |
| `bsp_joystick` | Encapsula ADC, filtro EMA, zona morta e curva de resposta. |
| `bsp_mpu6050` | Encapsula I2C, calibração do giroscópio, filtros e cálculo de pitch/roll. |
| `bsp_servo` | Encapsula MCPWM e mapeia comando percentual para pulso de servo. |
| `rtos_port` | Interface de abstração para tarefas, filas e delays do FreeRTOS. |

Mais detalhes estão em [docs/architecture.md](docs/architecture.md).

## Fluxo De Dados

```text
Joystick ADC
    │
    ▼
bsp_joystick
    │
    ▼
joystick_task
    ├── servo_queue ──► servo_task ──► bsp_servo ──► PWM dos servos
    └── debug_queue ──► status_task ─► ESP_LOG / UART

MPU6050 I2C
    │
    ▼
bsp_mpu6050
    │
    ▼
mpu6050_task ──► JSON via UART
```

## Tarefas FreeRTOS

| Tarefa | Prioridade | Responsabilidade | Frequência |
| --- | ---: | --- | --- |
| `joystick_task` | 5 | Lê joystick X/Y, filtra e publica amostras processadas. | 50 Hz |
| `servo_task` | 4 | Atualiza os dois servos a partir do comando mais recente. | Sob demanda |
| `mpu6050_task` | 4 | Lê aceleração/giroscópio, calcula pitch/roll e envia JSON serial. | 50 Hz, saída a cada 0,5 s |
| `status_task` | 3 | Emite logs seriais de status do joystick e servos. | 1 Hz |

## Hardware

| Sinal | Pino ESP32-S3 | Função |
| --- | --- | --- |
| Joystick X | GPIO7 / ADC1_CH6 | Entrada analógica do eixo X |
| Joystick Y | GPIO8 / ADC1_CH7 | Entrada analógica do eixo Y |
| Servo X | GPIO4 | PWM 50 Hz para inclinação no eixo X |
| Servo Y | GPIO5 | PWM 50 Hz para inclinação no eixo Y |
| MPU6050 SDA | GPIO35 | Barramento I2C |
| MPU6050 SCL | GPIO36 | Barramento I2C |

A pinagem fica centralizada em `components/bsp_board/include/bsp_board.h`, facilitando ajustes de montagem sem alterar a lógica da aplicação.

## Requisitos

- ESP32-S3.
- ESP-IDF v6.1-dev ou compatível.
- Doxygen, opcional, para geração da documentação HTML.

## Build E Gravação

Configure o ambiente da ESP-IDF e compile:

```bash
idf.py build
```

Grave e acompanhe os logs seriais:

```bash
idf.py -p PORT flash monitor
```

Substitua `PORT` pela porta serial do ESP32-S3, por exemplo `/dev/ttyACM0`.

## Documentação

Os headers públicos dos componentes possuem comentários Doxygen. Para gerar a documentação HTML:

```bash
doxygen Doxyfile
```

O HTML será gerado em:

```text
docs/doxygen/html
```

## Estrutura Do Projeto

```text
.
├── components
│   ├── app_labyrinth
│   ├── bsp_board
│   ├── bsp_joystick
│   ├── bsp_mpu6050
│   ├── bsp_servo
│   └── rtos_port
├── docs
│   └── architecture.md
├── main
│   └── projeto_labirinto.c
├── CMakeLists.txt
├── Doxyfile
└── README.md
```

## Estado Atual

- Fase 1 implementada: joystick, servos, suavização e logs.
- Fase 2 implementada: MPU6050, pitch/roll, filtros, JSON serial e task dedicada.
- Build validado com `idf.py build`.
- Documentação validada com `doxygen Doxyfile`.
- Binário gerado em `build/projeto-labirinto.bin`.
