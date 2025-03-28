#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "RTClib.h"

// #define BOTAO 8                                     // Pino do botão
#define SERVER_URL "http://192.168.198.114:3000/dados"  // URL do servidor
#define GEIGER_PIN 0                                    // Pino do detector

// dados da rede
const char* ssid = "Casa_2";
const char* password = "gami2022";

RTC_DS3231 rtc;                                    // Objeto do RTC
volatile unsigned long contadorHoras[24] = { 0 };  // Vetor para contar apertos por hora
// bool botaoPressionado = false;                     // Flag para evitar múltiplos incrementos
int minutoAtual = 0;

// Função de interrupção
void IRAM_ATTR contarPulso() {
  contadorHoras[minutoAtual]++;  // Incrementa a contagem a cada detecção de pulso
}

// Função para enviar os dados via HTTP POST
void enviarDados() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    // Criando JSON com os dados
    String json = "{ \"horas\": [";
    for (int i = 0; i < 24; i++) {
      json += String(contadorHoras[i]);
      if (i < 23) json += ", ";
    }
    json += "] }";

    // interação com o servidor
    int resposta = http.POST(json);
    if (resposta > 0) {
      Serial.print("Enviado com sucesso! Resposta do servidor: ");
      Serial.println(http.getString());
    } else {
      Serial.print("Erro ao enviar: ");
      Serial.println(http.errorToString(resposta).c_str());
    }
    http.end();

  } else {
    Serial.println("Wi-Fi desconectado! Tentando reconectar...");
    WiFi.begin(ssid, password);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Wire.begin(10, 8);  // SDA (GPIO 10), SCL (GPIO 8)

  // Conectar ao Wi-Fi
  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("WiFi Conectado!");

  // Inicializa o RTC
  if (!rtc.begin()) {
    Serial.println("Não foi possível encontrar o RTC");
    while (1);  // trava a execução caso o dispositivo não seja encontrado
  }
  Serial.println("RTC encontrado!");

  // Configuração para garantir que o RTC funcione com a bateria
  Wire.beginTransmission(0x68);  // Endereço do DS3231
  Wire.write(0x0E);              // Seleciona o registrador de controle
  Wire.write(0b00011100);        // Habilita saída de onda quadrada e ativa o oscilador
  Wire.endTransmission();

  if (rtc.lostPower()) {
    Serial.println("⚠ O RTC perdeu energia! Ajustando o horário...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Ajusta o relógio somente se necessário
  } else {
    Serial.println("✅ RTC funcionando corretamente.");
  }

  // pinMode(BOTAO, INPUT_PULLUP);  // Configura o botão com pull-up interno
  pinMode(GEIGER_PIN, INPUT);  // Configura o pino como entrada
  // Configura interrupção na borda de descida
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), contarPulso, FALLING);
}

void loop() {
  DateTime agora = rtc.now();    // Obtém a hora atual do DS3231
  minutoAtual = agora.minute();  // Pega apenas o minuto

  // //Verifica se o botão foi pressionado
  // if (digitalRead(BOTAO) == LOW && !botaoPressionado) {
  //   contadorHoras[minutoAtual]++;  // Incrementa a posição correspondente ao minuto
  //   botaoPressionado = true;
  //   Serial.print("Botão pressionado! Minuto: ");
  //   Serial.println(minutoAtual);
  //   enviarDados();
  // }

  // // Reseta a flag quando o botão for solto
  // if (digitalRead(BOTAO) == HIGH) {
  //   botaoPressionado = false;
  // }

  // A cada 3 segundos, envia os dados
  static unsigned long ultimoEnvio = 0;
  if (millis() - ultimoEnvio > 3000) {
    enviarDados();
    ultimoEnvio = millis();
  }
}

