#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include "RTClib.h"

// #define BOTAO 8                                     // Pino do botão
#define SERVER_URL "http://192.168.198.114:3000/dados"  // URL do servidor
#define GEIGER_PIN 0                                    // Pino do detector
#define CS_PIN     20

// dados da rede
const char* ssid = "UEFS_VISITANTES";
const char* password = "";

RTC_DS3231 rtc;                                    // Objeto do RTC
volatile unsigned long contadorHoras[24] = { 0 };  // Vetor para contar apertos por hora
// bool botaoPressionado = false;                     // Flag para evitar múltiplos incrementos
int minutoAtual = 0;

// Função de interrupção
void IRAM_ATTR contarPulso() {
  contadorHoras[minutoAtual]++;  // Incrementa a contagem a cada detecção de pulso
}

// Função para enviar os dados via HTTP POST
void sendData() {
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

// Função para salvar os dados no cartão SD
void writeData() {
  File arquivo = SD.open("/dadosDetector.txt", FILE_APPEND);
  if (arquivo) {
    arquivo.print("[");
    for(int i = 0; i < 24; i++){
      arquivo.print(contadorHoras[i]);
      if(i < 23){
        arquivo.print(", ");
      }
    }
    arquivo.print("]");
    arquivo.close();
  } else {
    Serial.println("Erro ao abrir o arquivo");
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
  Wire.endTransmission();        // finazlia a comunicação

  if (rtc.lostPower()) {
    Serial.println("⚠ O RTC perdeu energia! Ajustando o horário...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Ajusta o relógio somente se necessário
  } else {
    Serial.println("✅ RTC funcionando corretamente.");
  }

  pinMode(GEIGER_PIN, INPUT);  // Configura o pino como entrada

  // Configura interrupção na borda de descida
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), contarPulso, FALLING);

  Serial.println("Inicializando cartão SD...");

  if (!SD.begin(CS_PIN)) {
    Serial.println("⚠ Falha ao inicializar o cartão SD!");
    return;
  }
  Serial.println("✅ Cartão SD inicializado com sucesso!");
}

void loop() {
  DateTime agora = rtc.now();    // Obtém a hora atual do DS3231
  minutoAtual = agora.minute();  // Pega apenas o minuto

  static unsigned long ultimoEnvio = 0;
  if (millis() - ultimoEnvio > 6000) {
    sendData();
    writeData();
    ultimoEnvio = millis();
  }
}