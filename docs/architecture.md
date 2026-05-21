# Arquitetura da Fase 1

## Objetivo

A Fase 1 implementa o controle local da mesa labirinto com três responsabilidades principais: ler o joystick analógico, comandar os servos por PWM e emitir status/logs pela serial.

## Camadas

### BSP

A camada BSP isola detalhes da ESP-IDF e do hardware:

- `bsp_board`: pinout e constantes elétricas.
- `bsp_joystick`: ADC oneshot, filtro EMA, zona morta e normalização.
- `bsp_servo`: LEDC 50 Hz e conversão de percentual para pulso de servo.
- `bsp_status_led`: GPIO do LED de pronto.

### Porta RTOS

`rtos_port` encapsula FreeRTOS em uma interface pequena de tarefas, filas e delay. A aplicação não inclui headers do FreeRTOS diretamente.

### Aplicação

`app_labyrinth` inicializa os BSPs, cria filas e sobe as tarefas:

1. `joystick_task` publica amostras processadas.
2. `servo_task` consome amostras e atualiza os atuadores.
3. `status_task` consome amostras de debug, liga o LED e registra logs seriais.

## Fluxo de dados

```text
ADC joystick -> bsp_joystick -> joystick_task
                                 ├── servo_queue -> servo_task -> bsp_servo -> PWM
                                 └── debug_queue -> status_task -> ESP_LOG/UART
```

## Pontos de calibração

Os principais ajustes iniciais estão em:

- `bsp_joystick_default_config`: centro, zona morta e intensidade do filtro.
- `bsp_servo_x_default_config` e `bsp_servo_y_default_config`: pulso mínimo, central e máximo.
- `bsp_board.h`: canais ADC e GPIOs.
