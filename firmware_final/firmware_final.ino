#include <WiFi.h>
#include <HTTPClient.h>

const char* SSID = "SSID da rede Wi-Fi";
const char* SENHA = "SENHA da rede Wi-Fi";
const char* BACKEND_URL = "https://nonenticingly-grooveless-nisha.ngrok-free.dev/telemetry";
const char* DEVICE_ID = "pico-w-01";

const int PINO_SENSOR_DIGITAL = 15;
const int PINO_SENSOR_ANALOGICO = 26;
const int DEBOUNCE_MS = 50;
const int TAMANHO_JANELA = 5;

// variáveis do sensor digital com IRQ
volatile bool mudouEstado = false;
volatile unsigned long ultimaMudancaIRQ = 0;
int estadoAtual = HIGH;

// variáveis do sensor analógico
int janela[TAMANHO_JANELA] = {0, 0, 0, 0, 0};
int indexJanela = 0;

// controle de envio
unsigned long ultimoEnvio = 0;
const int INTERVALO_ENVIO = 5000;

//  IRQ handler 
void onSensorDigital() {
  unsigned long agora = millis();
  if (agora - ultimaMudancaIRQ > DEBOUNCE_MS) {
    ultimaMudancaIRQ = agora;
    mudouEstado = true;
  }
}

// máquina de estados 
enum EstadoWiFi { DESCONECTADO, CONECTANDO, CONECTADO };
EstadoWiFi estadoWiFi = DESCONECTADO;
unsigned long iniciouConexao = 0;

void gerenciarWiFi() {
  switch (estadoWiFi) {
    case DESCONECTADO:
      Serial.println("Wi-Fi: iniciando conexão...");
      WiFi.begin(SSID, SENHA);
      iniciouConexao = millis();
      estadoWiFi = CONECTANDO;
      break;

    case CONECTANDO:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi: conectado!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        estadoWiFi = CONECTADO;
      } else if (millis() - iniciouConexao > 15000) {
        Serial.println("Wi-Fi: timeout, tentando novamente...");
        WiFi.disconnect();
        estadoWiFi = DESCONECTADO;
      }
      break;

    case CONECTADO:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi: conexão perdida!");
        estadoWiFi = DESCONECTADO;
      }
      break;
  }
}

bool enviarTelemetria(String sensorType, String readingType, float value) {
  if (estadoWiFi != CONECTADO) return false;

  HTTPClient http;
  http.setInsecure();
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(BACKEND_URL);
  http.addHeader("Content-Type", "application/json");

  unsigned long s = millis() / 1000;
  unsigned long h = (s / 3600) % 24;
  unsigned long m = (s / 60) % 60;
  unsigned long sec = s % 60;
  char timestamp[30];
  sprintf(timestamp, "2024-01-01T%02lu:%02lu:%02luZ", h, m, sec);

  String json = "{";
  json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"sensor_type\":\"" + sensorType + "\",";
  json += "\"reading_type\":\"" + readingType + "\",";
  json += "\"value\":" + String(value);
  json += "}";

  Serial.print("Enviando: ");
  Serial.println(json);

  int httpCode = http.POST(json);
  Serial.print("Resposta HTTP: ");
  Serial.println(httpCode);

  http.end();
  return httpCode == 201;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(PINO_SENSOR_DIGITAL, INPUT_PULLUP);
  pinMode(PINO_SENSOR_ANALOGICO, INPUT);
  analogReadResolution(12);

  // registra o IRQ handler no pino digital
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_DIGITAL), onSensorDigital, CHANGE);

  Serial.println("Sensores iniciados.");
}

float calcularMedia() {
  int soma = 0;
  for (int i = 0; i < TAMANHO_JANELA; i++) {
    soma += janela[i];
  }
  return (float)soma / TAMANHO_JANELA;
}

void loop() {
  // gerencia Wi-Fi com máquina de estados
  gerenciarWiFi();

  //  sensor digital via IRQ 
  if (mudouEstado) {
    mudouEstado = false;
    estadoAtual = digitalRead(PINO_SENSOR_DIGITAL);
    Serial.print("Sensor digital (IRQ): ");
    Serial.println(estadoAtual == LOW ? "PRESENÇA DETECTADA" : "SEM PRESENÇA");
  }

  int leituraADC = analogRead(PINO_SENSOR_ANALOGICO);
  janela[indexJanela] = leituraADC;
  indexJanela = (indexJanela + 1) % TAMANHO_JANELA;
  float media = calcularMedia();
  float tensao = media * 3.3 / 4095.0;

  // --- envio de telemetria a cada 5 segundos ---
  if (estadoWiFi == CONECTADO && (millis() - ultimoEnvio) > INTERVALO_ENVIO) {
    ultimoEnvio = millis();

    bool okDigital = enviarTelemetria("presenca", "digital", estadoAtual == LOW ? 1.0 : 0.0);
    Serial.println(okDigital ? "Digital enviado!" : "Falha no envio digital, tentando novamente...");
    if (!okDigital) enviarTelemetria("presenca", "digital", estadoAtual == LOW ? 1.0 : 0.0);

    bool okAnalogico = enviarTelemetria("tensao", "analogico", tensao);
    Serial.println(okAnalogico ? "Analógico enviado!" : "Falha no envio analógico, tentando novamente...");
    if (!okAnalogico) enviarTelemetria("tensao", "analogico", tensao);
  }

  delay(100);
}