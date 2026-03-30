const int PINO_SENSOR_DIGITAL = 15;
const int PINO_SENSOR_ANALOGICO = 26;
const int DEBOUNCE_MS = 50;
const int TAMANHO_JANELA = 5;  // média móvel de 5 leituras

// variáveis do sensor digital
int estadoAnterior = HIGH;
int estadoAtual = HIGH;
unsigned long ultimaMudanca = 0;

// variáveis do sensor analógico
int janela[TAMANHO_JANELA] = {0, 0, 0, 0, 0};
int indexJanela = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);
  pinMode(PINO_SENSOR_DIGITAL, INPUT_PULLUP);
  pinMode(PINO_SENSOR_ANALOGICO, INPUT);
  analogReadResolution(12);  // força 12 bits (0-4095)
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
  indexJanela = (indexJanela + 1) % TAMANHO_JANELA;  // volta pro 0 depois do 4

  float media = calcularMedia();
  float tensao = media * 3.3 / 4095.0;  // converte para volts

  Serial.print("ADC bruto: ");
  Serial.print(leituraADC);
  Serial.print(" | Média: ");
  Serial.print(media);
  Serial.print(" | Tensão: ");
  Serial.print(tensao);
  Serial.println("V");

  delay(500);
}