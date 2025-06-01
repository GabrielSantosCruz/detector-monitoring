#include <Wire.h>
#include <WiFi.h>
#include "RTClib.h"

#define GEIGER_PIN 0
#define ARRAY_SIZE 24

// dados da rede
const char* ssid = "Mojo Dojo Casa House";
const char* password = "depoiseuteconto";

// modo de baixo consumo
// desligar o bluetooth
// ver como burlar o firewall
// configurar o Watchdog Timer

RTC_DS3231 rtc;                                            // Objeto do RTC
volatile unsigned long contadorHoras[ARRAY_SIZE] = { 0 };  // Vetor para contar os pulsos por hora
volatile unsigned long testePulso = 0;
int minutoAtual = 0;

// Função de interrupção
void IRAM_ATTR contarPulso() {
  testePulso++;
  contadorHoras[minutoAtual]++;  // Incrementa a contagem a cada detecção de pulso
}

void print_array(){
  Serial.print("[");
  for(int i = 0; i < ARRAY_SIZE; i++){
    Serial.print(contadorHoras[i]);
    if(i < ARRAY_SIZE-1){
      Serial.print(", ");
    }
  }
  Serial.println("]");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  // conexão do RTC
  Wire.begin(10, 8);  // SDA (GPIO 10), SCL (GPIO 8)

  // Conectar ao Wi-Fi
  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
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
    Serial.println("O RTC perdeu energia! Ajustando o horário...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Ajusta o relógio somente se necessário
  } else {
    Serial.println("RTC funcionando corretamente.");
  }

  pinMode(GEIGER_PIN, INPUT);  // Configura o pino como entrada

  // Configura interrupção na borda de descida
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), contarPulso, FALLING);
}

void loop() {
  DateTime agora = rtc.now();    // Obtém a hora atual do DS3231
  minutoAtual = agora.minute();  // Pega apenas o minuto

  // A cada n*10³ segundos, envia os dados
  static unsigned long ultimoEnvio = 0;
  if (millis() - ultimoEnvio > 3000) {
    Serial.println(testePulso);
    ultimoEnvio = millis();
  }
}