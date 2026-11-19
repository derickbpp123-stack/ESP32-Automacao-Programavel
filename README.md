# Automação ESP32

Projeto de automação com ESP32, relé, botão físico, display LCD 16x2 e módulo RTC DS3231. O sistema cria uma rede Wi-Fi própria e disponibiliza uma página web para controlar o relé e cadastrar até 10 agendamentos semanais.

## Funcionalidades

- Controle do relé pela página web (ligar/desligar).
- Alternância do relé por botão físico.
- Relógio e estado do relé exibidos no LCD.
- Leitura de data e hora pelo RTC DS3231.
- Até 10 agendamentos, com seleção de horário, ação e dias da semana.
- Exclusão individual ou de todos os agendamentos.
- Access Point Wi-Fi e DNS cativo para facilitar o acesso à interface.

## Hardware

- ESP32
- Módulo relé compatível com a lógica de acionamento utilizada
- RTC DS3231
- Display LCD 16x2 compatível com interface paralela
- Botão push-button
- Jumpers e alimentação adequada

Consulte [PINAGEM.md](PINAGEM.md) para as ligações e [COMPONENTES.md](COMPONENTES.md) para a lista de componentes e bibliotecas.

## Instalação

1. Instale o suporte de placas ESP32 no Arduino IDE.
2. Instale a biblioteca **RTClib** (Adafruit) pelo Gerenciador de Bibliotecas.
3. Verifique se a biblioteca **LiquidCrystal** está instalada.
4. Abra `ESP32-Automacao-Programavel.ino`.
5. Selecione a placa ESP32 correta e a porta serial.
6. Confira as ligações e grave o programa.

As bibliotecas `WiFi.h`, `WebServer.h`, `DNSServer.h` e `Wire.h` são fornecidas pelo core ESP32.

## Acesso à interface

Após iniciar, o ESP32 cria uma rede Wi-Fi:

- **SSID:** `ESP32_Timer_AP`
- **Senha inicial configurada no código:** `12345678`
- **Endereço local padrão do Access Point:** `192.168.4.1`

Conecte um celular ou computador à rede e abra o endereço indicado. Altere `AP_PASS` no código antes de usar o projeto em um ambiente real, especialmente se o repositório for público.

## Observações importantes

- Os agendamentos são armazenados somente na memória RAM. Eles são apagados quando o ESP32 reinicia ou perde alimentação.
- Quando o DS3231 informa que perdeu a alimentação, o código ajusta o relógio com a data e hora da compilação (`__DATE__` e `__TIME__`). Depois disso, o RTC mantém a contagem usando sua própria bateria.
- A saída do relé está configurada como ativa em nível alto (`RELE_ATIVO_HIGH = true`). Ajuste essa configuração conforme o módulo utilizado.
- Este repositório documenta o estado atual do projeto; valide a montagem e o comportamento do circuito antes de conectar cargas.

## Arquivo principal

`ESP32-Automacao-Programavel.ino`
