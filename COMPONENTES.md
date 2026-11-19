# Componentes e bibliotecas

## Componentes de hardware

| Componente | Função |
|---|---|
| ESP32 | Controlador principal e servidor web |
| Módulo relé | Acionamento da carga |
| RTC DS3231 | Manutenção e leitura de data/hora |
| LCD 16x2 | Exibição do relógio e estado do relé |
| Botão push-button | Alternância manual do relé |
| Jumpers | Conexões entre os módulos |
| Fonte/alimentação adequada | Alimentação do circuito |

## Bibliotecas

| Biblioteca | Utilização |
|---|---|
| WiFi.h | Access Point Wi-Fi |
| WebServer.h | Servidor HTTP |
| DNSServer.h | DNS cativo |
| Wire.h | Comunicação I2C com o RTC |
| RTClib.h (Adafruit) | Comunicação com o DS3231 |
| LiquidCrystal.h | Controle do LCD em modo 4 bits |

As bibliotecas WiFi, WebServer, DNSServer e Wire acompanham o pacote de placas ESP32. Instale RTClib pelo Gerenciador de Bibliotecas da Arduino IDE. LiquidCrystal costuma estar disponível na instalação padrão da IDE.
