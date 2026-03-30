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

int estadoAnterior = HIGH;
int estadoAtual = HIGH;
unsigned long ultimaMudanca = 0;

int janela[TAMANHO_JANELA] = {0, 0, 0, 0, 0};
int indexJanela = 0;

unsigned long ultimoEnvio = 0;
const int INTERVALO_ENVIO = 5000;  // envia a cada 5 segundos

void conectarWiFi() {
  Serial.print("Conectando na rede ");
  Serial.print(SSID);
  WiFi.begin(SSID, SENHA);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

bool enviarTelemetria(String sensorType, String readingType, float value) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setInsecure();  // ignora verificação de certificado SSL
  http.begin(BACKEND_URL);
  http.addHeader("Content-Type", "application/json");

  unsigned long s = millis() / 1000;
  unsigned long h = (s / 3600) % 24;
  unsigned long m = (s / 60) % 60;
  unsigned long sec = s % 60;
  char timestamp[30];
  sprintf(timestamp, "2024-01-01T%02lu:%02lu:%02luZ", h, m, sec);

  // JSON
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

  Serial.println(http.errorToString(httpCode));

  http.end();
  return httpCode == 201;
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  pinMode(PINO_SENSOR_DIGITAL, INPUT_PULLUP);
  pinMode(PINO_SENSOR_ANALOGICO, INPUT);
  analogReadResolution(12);
  conectarWiFi();
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
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado! Reconectando...");
    conectarWiFi();
  }

  int leitura = digitalRead(PINO_SENSOR_DIGITAL);
  if (leitura != estadoAnterior) {
    ultimaMudanca = millis();
    estadoAnterior = leitura;
  }
  if ((millis() - ultimaMudanca) > DEBOUNCE_MS) {
    if (leitura != estadoAtual) {
      estadoAtual = leitura;
      Serial.print("Sensor digital: ");
      Serial.println(estadoAtual == LOW ? "PRESENÇA DETECTADA" : "SEM PRESENÇA");
    }
  }

  int leituraADC = analogRead(PINO_SENSOR_ANALOGICO);
  janela[indexJanela] = leituraADC;
  indexJanela = (indexJanela + 1) % TAMANHO_JANELA;
  float media = calcularMedia();
  float tensao = media * 3.3 / 4095.0;

  // envio de telemetria a cada 5 segundos 
  if ((millis() - ultimoEnvio) > INTERVALO_ENVIO) {
    ultimoEnvio = millis();

    // envia sensor digital
    bool okDigital = enviarTelemetria("presenca", "digital", estadoAtual == LOW ? 1.0 : 0.0);
    Serial.println(okDigital ? "Digital enviado!" : "Falha no envio digital, tentando novamente...");
    if (!okDigital) enviarTelemetria("presenca", "digital", estadoAtual == LOW ? 1.0 : 0.0);

    // envia sensor analógico
    bool okAnalogico = enviarTelemetria("tensao", "analogico", tensao);
    Serial.println(okAnalogico ? "Analógico enviado!" : "Falha no envio analógico, tentando novamente...");
    if (!okAnalogico) enviarTelemetria("tensao", "analogico", tensao);
  }
}