const int PINO_SENSOR = 15;
const int DEBOUNCE_MS = 50;

int estadoAnterior = HIGH;
int estadoAtual = HIGH;
unsigned long ultimaMudanca = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);
  pinMode(PINO_SENSOR, INPUT_PULLUP);  
  Serial.println("Sensor digital iniciado.");
}

void loop() {
  int leitura = digitalRead(PINO_SENSOR);

  if (leitura != estadoAnterior) {
    ultimaMudanca = millis();
    estadoAnterior = leitura;
  }

  if ((millis() - ultimaMudanca) > DEBOUNCE_MS) {
    if (leitura != estadoAtual) {
      estadoAtual = leitura;
      Serial.print("Sensor: ");
      Serial.println(estadoAtual == LOW ? "PRESENÇA DETECTADA" : "SEM PRESENÇA");
    }
  }
}