# Pinagem do projeto

## ESP32

| Dispositivo | Pino do dispositivo | GPIO do ESP32 |
|---|---|---:|
| Relé | IN | 23 |
| Botão | Um terminal | 4 |
| Botão | Outro terminal | GND |
| RTC DS3231 | SDA | 21 |
| RTC DS3231 | SCL | 22 |
| LCD 16x2 | RS | 14 |
| LCD 16x2 | E | 27 |
| LCD 16x2 | D4 | 26 |
| LCD 16x2 | D5 | 25 |
| LCD 16x2 | D6 | 18 |
| LCD 16x2 | D7 | 19 |

## Observações

- O botão utiliza `INPUT_PULLUP`; portanto, deve fechar contato entre GPIO 4 e GND quando pressionado.
- O barramento I2C do RTC é inicializado com SDA no GPIO 21 e SCL no GPIO 22.
- O relé está configurado para acionamento em nível alto. Confira a lógica elétrica do módulo antes de conectar.
- Confira a tensão de alimentação e a compatibilidade lógica de cada módulo com o ESP32. Não alimente cargas diretamente pelos GPIOs.
