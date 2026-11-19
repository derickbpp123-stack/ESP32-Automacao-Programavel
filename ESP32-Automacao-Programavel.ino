#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>

// Configuração da Rede Wi-Fi (Ponto de Acesso AP)
#define AP_SSID "ESP32_Timer_AP"
#define AP_PASS "12345678"

// Pino do Relé (GPIO 23)
#define RELE_PIN 23
const bool RELE_ATIVO_HIGH = true;

// Pino do Botão Físico (ligado entre D4 / GPIO 4 e GND)
#define BOTAO_PIN 4

// Pinos do Display LCD 16x2 (Modo 4-bits)
#define PIN_RS 14
#define PIN_E  27
#define PIN_D4 26
#define PIN_D5 25
#define PIN_D6 18
#define PIN_D7 19

// Pinos I2C do RTC DS3231
#define SDA_PIN 21
#define SCL_PIN 22

// Limite máximo de agendamentos simultâneos
#define MAX_TIMERS 10

struct Agendamento {
  bool ativo;
  int hora;
  int minuto;
  bool acao;                 // true = LIGAR, false = DESLIGAR
  bool dias[7];              // 0=Dom, 1=Seg, 2=Ter, 3=Qua, 4=Qui, 5=Sex, 6=Sab
  int ultimoMinutoDisparado; // Trava para não repetir no mesmo minuto (-1 = liberado)
};

LiquidCrystal lcd(PIN_RS, PIN_E, PIN_D4, PIN_D5, PIN_D6, PIN_D7);
RTC_DS3231 rtc;
WebServer server(80);
DNSServer dnsServer;

Agendamento agendas[MAX_TIMERS];

bool estadoRele = false;
bool rtcOk = false;
unsigned long ultimoAtualizaDisplay = 0;
unsigned long ultimoResetLCD = 0;

// Controle de Debounce do Botão Físico (D4 -> GND)
bool ultimoEstadoLeituraBotao = HIGH;
bool estadoEstavelBotao = HIGH;
unsigned long ultimoTempoDebounce = 0;
const unsigned long DEBOUNCE_DELAY = 50;

void resincronizarLCD() {
  lcd.begin(16, 2);
  delay(10);
  lcd.clear();
}

void atualizarDisplayLocal() {
  lcd.setCursor(0, 0);
  if (rtcOk) {
    DateTime agora = rtc.now();
    char bufferHora[17];
    snprintf(bufferHora, sizeof(bufferHora), "Hora: %02d:%02d:%02d",
             agora.hour(), agora.minute(), agora.second());
    lcd.print(bufferHora);
  } else {
    lcd.print("RTC Indisponivel");
  }

  lcd.setCursor(0, 1);
  if (estadoRele) {
    lcd.print("Rele: LIGADO    ");
  } else {
    lcd.print("Rele: DESLIGADO ");
  }
}

void aplicarEstadoRele(bool ligar) {
  estadoRele = ligar;
  digitalWrite(RELE_PIN, RELE_ATIVO_HIGH ? (estadoRele ? HIGH : LOW) : (estadoRele ? LOW : HIGH));
}

void acionarRele(bool ligar) {
  aplicarEstadoRele(ligar);
  delay(20);
  resincronizarLCD();
  atualizarDisplayLocal();
}

void verificarBotao() {
  bool leituraAtual = digitalRead(BOTAO_PIN);

  if (leituraAtual != ultimoEstadoLeituraBotao) {
    ultimoTempoDebounce = millis();
  }

  if ((millis() - ultimoTempoDebounce) > DEBOUNCE_DELAY) {
    if (leituraAtual != estadoEstavelBotao) {
      estadoEstavelBotao = leituraAtual;

      // Alterna o relé na borda de descida (quando D4 fecha contato com GND)
      if (estadoEstavelBotao == LOW) {
        acionarRele(!estadoRele);
        Serial.printf("Botao D4 acionado -> Rele: %s\n", estadoRele ? "LIGADO" : "DESLIGADO");
      }
    }
  }

  ultimoEstadoLeituraBotao = leituraAtual;
}

void verificarTimers() {
  if (!rtcOk) return;

  DateTime agora = rtc.now();
  uint8_t diaHoje = agora.dayOfTheWeek(); // 0 (Dom) a 6 (Sab)
  int horaAtual = agora.hour();
  int minAtual = agora.minute();

  for (int i = 0; i < MAX_TIMERS; i++) {
    if (!agendas[i].ativo) continue;

    // Libera o disparo assim que o relógio avançar para o próximo minuto
    if (agendas[i].ultimoMinutoDisparado != -1 && agendas[i].ultimoMinutoDisparado != minAtual) {
      agendas[i].ultimoMinutoDisparado = -1;
    }

    if (agendas[i].dias[diaHoje] &&
        agendas[i].hora == horaAtual &&
        agendas[i].minuto == minAtual &&
        agendas[i].ultimoMinutoDisparado != minAtual) {

      acionarRele(agendas[i].acao);
      agendas[i].ultimoMinutoDisparado = minAtual;
      Serial.printf("Agenda #%d executada: %s as %02d:%02d\n",
                    i + 1, agendas[i].acao ? "LIGAR" : "DESLIGAR", horaAtual, minAtual);
    }
  }
}

void enviarPaginaWeb() {
  String bufferDataHora = "RTC Indisponivel";
  const char* nomesDias[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};

  if (rtcOk) {
    DateTime agora = rtc.now();
    char temp[40];
    snprintf(temp, sizeof(temp), "%s, %02d/%02d/%04d %02d:%02d",
             nomesDias[agora.dayOfTheWeek()],
             agora.day(), agora.month(), agora.year(),
             agora.hour(), agora.minute());
    bufferDataHora = String(temp);
  }

  // Monta a lista HTML das programações ativas
  int totalAtivos = 0;
  String listaTimersHTML = "";

  for (int i = 0; i < MAX_TIMERS; i++) {
    if (!agendas[i].ativo) continue;
    totalAtivos++;

    String diasStr = "";
    for (int d = 0; d < 7; d++) {
      if (agendas[i].dias[d]) {
        if (diasStr.length() > 0) diasStr += " ";
        diasStr += nomesDias[d];
      }
    }

    char itemBuf[320];
    snprintf(itemBuf, sizeof(itemBuf),
             "<div class='timer-item'>"
             "<div>"
             "<span class='timer-time'>%02d:%02d</span> "
             "<span class='badge %s'>%s</span>"
             "<div class='timer-days'>%s</div>"
             "</div>"
             "<a href='/delTimer?id=%d' class='btn-del'>Excluir</a>"
             "</div>",
             agendas[i].hora, agendas[i].minuto,
             agendas[i].acao ? "badge-on" : "badge-off",
             agendas[i].acao ? "LIGAR" : "DESLIGAR",
             diasStr.c_str(), i);

    listaTimersHTML += String(itemBuf);
  }

  if (totalAtivos == 0) {
    listaTimersHTML = "<div class='empty-msg'>Nenhuma programação cadastrada.</div>";
  }

  String html;
  html.reserve(5200);

  html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Automação ESP32</title>
      <style>
        *, *::before, *::after { box-sizing: border-box; }
        body { font-family: 'Segoe UI', Roboto, Arial, sans-serif; background: #eef2f5; margin: 0; padding: 16px; min-height: 100vh; display: flex; justify-content: center; align-items: flex-start; color: #2c3e50; }
        .card { background: #fff; padding: 22px; border-radius: 18px; box-shadow: 0 8px 24px rgba(0,0,0,0.08); max-width: 420px; width: 100%; margin: 8px 0; }
        h1 { font-size: 1.45em; margin: 0 0 4px; text-align: center; }
        .ip-info { font-size: 0.82em; color: #7f8c8d; text-align: center; margin-bottom: 16px; }
        .clock { font-size: 1.15em; font-weight: bold; color: #2980b9; background: #ebf5fb; padding: 12px; border-radius: 10px; text-align: center; margin-bottom: 14px; }
        .status-box { font-size: 1.1em; padding: 12px; border-radius: 10px; font-weight: bold; text-align: center; margin-bottom: 16px; }
        .status-on { background: #d4edda; color: #155724; }
        .status-off { background: #f8d7da; color: #721c24; }
        .btn-group { display: flex; gap: 10px; margin-bottom: 20px; }
        .btn { flex: 1; padding: 14px; font-size: 0.98em; border: none; border-radius: 10px; color: #fff; text-decoration: none; font-weight: bold; text-align: center; cursor: pointer; }
        .btn-on { background: #27ae60; }
        .btn-off { background: #e74c3c; }
        .btn-add { background: #2980b9; width: 100%; display: block; margin-top: 14px; margin-bottom: 4px; }
        .btn-clear { background: #95a5a6; width: 100%; display: block; margin-top: 12px; margin-bottom: 2px; padding: 10px; font-size: 0.88em; }
        .section { background: #f8f9fa; padding: 15px; border-radius: 12px; border: 1px solid #e9ecef; margin-bottom: 16px; }
        .section:last-child { margin-bottom: 0; }
        .section h2 { font-size: 1.05em; margin: 0 0 12px; border-bottom: 1px solid #dee2e6; padding-bottom: 8px; display: flex; justify-content: space-between; }
        .form-row { display: flex; gap: 8px; margin-bottom: 14px; }
        .form-control { padding: 10px; font-size: 1em; border-radius: 8px; border: 1px solid #ced4da; flex: 1; min-width: 0; background: #fff; }
        .days-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 5px; margin-bottom: 4px; }
        .days-grid input { display: none; }
        .days-grid label { background: #e9ecef; padding: 9px 2px; border-radius: 6px; cursor: pointer; color: #495057; font-weight: bold; font-size: 0.78em; text-align: center; user-select: none; }
        .days-grid input:checked + label { background: #2980b9; color: #fff; }
        .timer-item { display: flex; justify-content: space-between; align-items: center; gap: 10px; background: #fff; padding: 10px 12px; border-radius: 8px; border: 1px solid #dee2e6; margin-bottom: 8px; }
        .timer-item:last-of-type { margin-bottom: 0; }
        .timer-time { font-size: 1.15em; font-weight: bold; }
        .timer-days { font-size: 0.8em; color: #6c757d; margin-top: 3px; }
        .badge { font-size: 0.75em; padding: 2px 7px; border-radius: 5px; font-weight: bold; vertical-align: middle; }
        .badge-on { background: #d4edda; color: #155724; }
        .badge-off { background: #f8d7da; color: #721c24; }
        .btn-del { background: #e74c3c; color: #fff; text-decoration: none; padding: 7px 10px; border-radius: 6px; font-size: 0.82em; font-weight: bold; flex-shrink: 0; }
        .empty-msg { text-align: center; color: #868e96; font-size: 0.9em; padding: 10px 0; }
        .limit-msg { text-align: center; color: #c0392b; font-size: 0.88em; font-weight: bold; margin-top: 10px; }
      </style>
    </head>
    <body>
      <div class="card">
        <h1>Automação ESP32</h1>
        <div class="ip-info">IP Local: IP_DO_ESP32</div>

        <div class="clock">DATA_HORA_RTC</div>
        <div class="status-box STATUS_CLASSE">RELÉ: STATUS_TEXTO</div>

        <div class="btn-group">
          <a href="/liga" class="btn btn-on">LIGAR</a>
          <a href="/desliga" class="btn btn-off">DESLIGAR</a>
        </div>

        <div class="section">
          <h2><span>Nova Programação</span><span>CONTADOR_TIMERS/10</span></h2>
          FORMULARIO_BLOCO
        </div>

        <div class="section">
          <h2><span>Programações Salvas</span></h2>
          LISTA_TIMERS
          BOTAO_LIMPAR_TODOS
        </div>
      </div>
      <script>
        function validarDias() {
          if (!document.querySelector('.days-grid input:checked')) {
            alert('Selecione pelo menos um dia da semana.');
            return false;
          }
          return true;
        }
      </script>
    </body>
    </html>
)rawliteral";

  String formBloco;
  if (totalAtivos < MAX_TIMERS) {
    formBloco = R"rawliteral(
      <form action="/addTimer" method="GET" onsubmit="return validarDias()">
        <div class="form-row">
          <input type="time" name="t" class="form-control" required>
          <select name="a" class="form-control">
            <option value="1">Ligar Relé</option>
            <option value="0">Desligar Relé</option>
          </select>
        </div>
        <div class="days-grid">
          <input type="checkbox" id="d0" name="d0" value="1"><label for="d0">Dom</label>
          <input type="checkbox" id="d1" name="d1" value="1" checked><label for="d1">Seg</label>
          <input type="checkbox" id="d2" name="d2" value="1" checked><label for="d2">Ter</label>
          <input type="checkbox" id="d3" name="d3" value="1" checked><label for="d3">Qua</label>
          <input type="checkbox" id="d4" name="d4" value="1" checked><label for="d4">Qui</label>
          <input type="checkbox" id="d5" name="d5" value="1" checked><label for="d5">Sex</label>
          <input type="checkbox" id="d6" name="d6" value="1"><label for="d6">Sáb</label>
        </div>
        <button type="submit" class="btn btn-add">ADICIONAR PROGRAMAÇÃO</button>
      </form>
    )rawliteral";
  } else {
    formBloco = "<div class='limit-msg'>Limite máximo de 10 programações atingido. Exclua uma abaixo para adicionar outra.</div>";
  }

  html.replace("IP_DO_ESP32", WiFi.softAPIP().toString());
  html.replace("DATA_HORA_RTC", bufferDataHora);
  html.replace("STATUS_TEXTO", estadoRele ? "LIGADO (ON)" : "DESLIGADO (OFF)");
  html.replace("STATUS_CLASSE", estadoRele ? "status-on" : "status-off");
  html.replace("CONTADOR_TIMERS", String(totalAtivos));
  html.replace("FORMULARIO_BLOCO", formBloco);
  html.replace("LISTA_TIMERS", listaTimersHTML);
  html.replace("BOTAO_LIMPAR_TODOS", totalAtivos > 0 ? "<a href='/clearTimers' class='btn btn-clear'>Excluir Todas</a>" : "");

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // 1. Inicializa o array das 10 programações
  for (int i = 0; i < MAX_TIMERS; i++) {
    agendas[i].ativo = false;
    agendas[i].ultimoMinutoDisparado = -1;
  }

  // 2. Configura o botão físico entre D4 (GPIO 4) e GND
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  // 3. Configura o Relé
  pinMode(RELE_PIN, OUTPUT);
  aplicarEstadoRele(false);

  // 4. Inicializa LCD e RTC DS3231
  resincronizarLCD();
  Wire.begin(SDA_PIN, SCL_PIN);
  if (rtc.begin()) {
    rtcOk = true;
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // 5. Inicia Access Point Wi-Fi e DNS cativo
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.start(53, "*", WiFi.softAPIP());

  // 6. Rotas Web
  server.on("/", enviarPaginaWeb);

  server.on("/liga", []() {
    acionarRele(true);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/desliga", []() {
    acionarRele(false);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/addTimer", []() {
    if (server.hasArg("t") && server.hasArg("a")) {
      int slotLivre = -1;
      for (int i = 0; i < MAX_TIMERS; i++) {
        if (!agendas[i].ativo) {
          slotLivre = i;
          break;
        }
      }

      if (slotLivre != -1) {
        String tempo = server.arg("t");
        int pos = tempo.indexOf(':');
        bool algumDia = false;

        for (int d = 0; d < 7; d++) {
          String argDia = "d" + String(d);
          agendas[slotLivre].dias[d] = server.hasArg(argDia);
          if (agendas[slotLivre].dias[d]) algumDia = true;
        }

        if (pos > 0 && algumDia) {
          agendas[slotLivre].hora = tempo.substring(0, pos).toInt();
          agendas[slotLivre].minuto = tempo.substring(pos + 1).toInt();
          agendas[slotLivre].acao = (server.arg("a") == "1");
          agendas[slotLivre].ultimoMinutoDisparado = -1;
          agendas[slotLivre].ativo = true;
        }
      }
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/delTimer", []() {
    if (server.hasArg("id")) {
      int id = server.arg("id").toInt();
      if (id >= 0 && id < MAX_TIMERS) {
        agendas[id].ativo = false;
        agendas[id].ultimoMinutoDisparado = -1;
      }
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/clearTimers", []() {
    for (int i = 0; i < MAX_TIMERS; i++) {
      agendas[i].ativo = false;
      agendas[i].ultimoMinutoDisparado = -1;
    }
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
  atualizarDisplayLocal();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Leitura contínua do botão físico (D4 -> GND)
  verificarBotao();

  // Prevenção periódica contra ruído no LCD a cada 10s
  if (millis() - ultimoResetLCD >= 10000) {
    ultimoResetLCD = millis();
    resincronizarLCD();
    atualizarDisplayLocal();
  }

  // Atualização do relógio e checagem das 10 programações a cada 1s
  if (millis() - ultimoAtualizaDisplay >= 1000) {
    ultimoAtualizaDisplay = millis();
    atualizarDisplayLocal();
    verificarTimers();
  }
}