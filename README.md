# Ponderada 2 — Integração com Raspberry Pi Pico W

Esse repositório contém o firmware para o Raspberry Pi Pico W, desenvolvido como parte da Atividade Ponderada 2. O firmware lê sensores digitais e analógicos e envia telemetria para o backend desenvolvido na [Atividade Ponderada 1](https://github.com/pietrabatista/ponderada_backinfra).

---

## Framework Utilizado
 
**Arduino Framework** via suporte à placa Raspberry Pi Pico W pelo pacote [arduino-pico](https://github.com/earlephilhower/arduino-pico) de Earlé Philhower, instalado no Arduino IDE.
 
---

 
## Sensores Integrados
 
| Sensor | Tipo | Pino GPIO | Range de Valores |
|--------|------|-----------|-----------------|
| Sensor de Presença (simulado com jumper) | Digital | GP15 | 0 (ausente) ou 1 (presente) |
| Sensor de Tensão (simulado com jumper) | Analógico (ADC) | GP26 (ADC0) | 0V a 3.3V (0 a 4095 em 12 bits) |
 
**Observação:** Por não possuir sensores físicos, os sensores foram simulados com jumpers fêmea-fêmea conectados diretamente nos pinos do Pico W:
 - Sensor digital: jumper entre GP15 e GND simula ausência/presença
- Sensor analógico: jumper entre GP26 e AGND/3V3 simula variação de tensão
 
---

## Diagrama de Conexão
 
```
                    Raspberry Pi Pico W
                   ┌─────────────────────┐
               3V3 │ 36              1   │ GP0
           3V3_EN  │ 35              2   │ GP1
             AGND  │ 33              3   │ GND
        GP28 (A2)  │ 34              4   │ GP2
        GP27 (A1)  │ 32              5   │ GP3
        GP26 (A0)◄─┼─31  [JUMPER]   6   │ GP4
                   │                     │
             GP15 ◄┼─20  [JUMPER]  18   │ GND
                   └─────────────────────┘
 
Sensor Digital:
  - Uma ponta do jumper fêmea-fêmea no GP15 (pino 20)
  - Outra ponta no GND (pino 18) → simula PRESENÇA DETECTADA
  - Ponta solta → simula SEM PRESENÇA
 
Sensor Analógico:
  - Uma ponta do jumper fêmea-fêmea no GP26/A0 (pino 31)
  - Outra ponta no AGND (pino 33) → leitura ≈ 0V
  - Outra ponta no 3V3 (pino 36) → leitura ≈ 3.3V
```
 
---
 
 ## Instruções de Compilação e Gravação
 
### Pré-requisitos
 
- Arduino IDE instalado
- Suporte à placa Raspberry Pi Pico W instalado
 
### Instalando o suporte à placa
 
1. Abra o Arduino IDE
2. Vá em **File → Preferences**
3. No campo **Additional boards manager URLs**, adicione:
```
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```
4. Vá em **Tools → Board → Boards Manager**
5. Pesquise por `Pico` e instale **Raspberry Pi Pico/RP2040**
6. Selecione a placa em **Tools → Board → Raspberry Pi RP2040 → Raspberry Pi Pico W**
 
### Primeira gravação
 
Na primeira gravação, o Pico W precisa estar em modo BOOTSEL:
 
1. Desconecte o USB
2. Segure o botão **BOOTSEL** no Pico W
3. Conecte o USB enquanto segura o botão
4. Solte o botão. O Pico aparece como pendrive `RPI-RP2`
5. Clique em **Upload** no Arduino IDE
 
Nas gravações seguintes, o Arduino IDE grava automaticamente sem precisar do BOOTSEL.
 
---
 
## Configuração de Rede
 
No arquivo do firmware, altere as seguintes constantes:
 
```cpp
const char* SSID = "SSID da rede Wi-Fi";        // Nome da rede Wi-Fi 2.4GHz
const char* SENHA = "SENHA da rede Wi-Fi";      // Senha da rede Wi-Fi
const char* BACKEND_URL = "https://seu-endpoint/telemetry"; // URL do backend
const char* DEVICE_ID = "pico-w-01";          // Identificador do dispositivo
```
 
### Expondo o backend com ngrok
 
Como o backend roda localmente via Docker, é necessário expor a porta 3000 usando o [ngrok](https://ngrok.com):
 
```bash
ngrok http 3000
```
 
Use a URL gerada pelo ngrok como `BACKEND_URL` no firmware.
 
---
 
## Implementações Técnicas
 
### Sensor Digital (IRQ Handler)
 
O sensor digital utiliza **interrupção de hardware (IRQ)** para detectar mudanças no pino GP15. Em vez de verificar o pino a cada volta do loop, o hardware interrompe o processador exatamente quando o sinal muda, garantindo que nenhuma mudança seja perdida:
 
```cpp
void onSensorDigital() {
  unsigned long agora = millis();
  if (agora - ultimaMudancaIRQ > DEBOUNCE_MS) {
    ultimaMudancaIRQ = agora;
    mudouEstado = true;
  }
}
 
attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_DIGITAL), onSensorDigital, CHANGE);
```
 
O debounce é implementado dentro do próprio IRQ handler, ignorando mudanças que ocorram em menos de 50ms.
 
### Sensor Analógico (ADC com Média Móvel)
 
O sensor analógico utiliza o ADC do Pico W em resolução de **12 bits (0 a 4095)** com média móvel de 5 leituras para suavização do sinal:
 
```cpp
analogReadResolution(12);  // 12 bits = 0 a 4095
float tensao = media * 3.3 / 4095.0;  // conversão para volts
```
 
### Conectividade Wi-Fi (Máquina de Estados)
 
A conexão Wi-Fi é gerenciada por uma **máquina de estados** com 3 estados formais, permitindo que o loop principal continue rodando normalmente enquanto o Wi-Fi tenta se reconectar:
 
```
DESCONECTADO → CONECTANDO → CONECTADO
                  ↓ timeout 15s
              DESCONECTADO (tenta novamente)
```
 
### Envio de Telemetria (Retry Automático)
 
A telemetria é enviada a cada 5 segundos via HTTP POST. Em caso de falha, o sistema tenta reenviar automaticamente:
 
```cpp
bool ok = enviarTelemetria("presenca", "digital", valor);
if (!ok) enviarTelemetria("presenca", "digital", valor);  // retry
```
 
---
 
## Formato da Telemetria
 
O firmware envia pacotes JSON para o endpoint `POST /telemetry` a cada 5 segundos:
 
```json
{
  "device_id": "pico-w-01",
  "timestamp": "2024-01-01T00:01:30Z",
  "sensor_type": "presenca",
  "reading_type": "digital",
  "value": 1.0
}
```
 
| Campo | Descrição |
|-------|-----------|
| `device_id` | Identificador único do dispositivo |
| `timestamp` | Horário da leitura (gerado a partir do uptime do Pico) |
| `sensor_type` | Tipo do sensor: `presenca` ou `tensao` |
| `reading_type` | Tipo de leitura: `digital` ou `analogico` |
| `value` | Valor lido pelo sensor |
 
---
 
## Evidências de Funcionamento
 
### Bloco 1 — Leitura de sensor digital com debounce
![Bloco 1](evidencias/bloco1_sensor_digital.png)
 
### Bloco 2 — Leitura de sensor analógico com ADC 12 bits e média móvel
![Bloco 2 - 3V3](evidencias/bloco2_sensor_analogico_3v3.png)
![Bloco 2 - GND](evidencias/bloco2_sensor_analogico_gnd.png)
 
### Firmware Final — IRQ + Telemetria sendo enviada com HTTP 201
![Firmware Final](evidencias/firmware_final_serial.png)
 
### Firmware Final — Dados chegando no backend
![Backend](evidencias/firmware_final_backend.png)
 
---
 
## Estrutura do Repositório
 
```
ponderada_integracao_picow/
├── bloco1_sensor_digital/
│   └── bloco1_sensor_digital.ino
├── bloco2_sensor_analogico/
│   └── bloco2_sensor_analogico_v2.ino
├── bloco3_wifi/
│   └── bloco3_wifi.ino
├── bloco4_telemetria/
│   └── bloco4_telemetria.ino
├── firmware_final/
│   └── firmware_final.ino
├── evidencias/
│   ├── bloco1_sensor_digital.png
│   ├── bloco2_sensor_analogico_3v3.png
│   ├── bloco2_sensor_analogico_gnd.png
│   ├── firmware_final_serial.png
│   └── firmware_final_backend.png
└── README.md
```
 
---
 
## Referência — Atividade 1
 
Backend utilizado para receber a telemetria: [ponderada_backinfra](https://github.com/pietrabatista/ponderada_backinfra)