#include <Wire.h>
#include <WiFi.h>
#include <RtcPCF8563.h> // MODIFICAÇÃO: Biblioteca para o PCF8563
#include "esp_sleep.h"    // Para Deep Sleep e causas de despertar
#include "esp_task_wdt.h" // Para Watchdog Timer

#define GEIGER_PIN GPIO_NUM_4 // Pino do detector Geiger
#define PULSE_GEIGER_LEVEL 1 // 1 para HIGH (RISING edge), 0 para LOW (FALLING edge) ao despertar
#define ARRAY_SIZE 24 // 24 horas no dia

// MODIFICAÇÃO: Pino do ESP32-C3 conectado ao pino INT do PCF8563
const int RTC_INT_PIN = GPIO_NUM_5; // Exemplo: use GPIO5 (lembre-se do resistor pull-up!)

// Dados da rede
const char* ssid = "UEFS_VISITANTES";
const char* password = "";


// Timeout do Watchdog em segundos
#define WDT_TIMEOUT_S 30

Rtc_Pcf8563 rtc;

// é necessário definir com esse formato para o modo deep sleep
RTC_DATA_ATTR unsigned long pulsosDesdeUltimoWakeup = 0;
RTC_DATA_ATTR unsigned long acumuladoPorHora[ARRAY_SIZE] = {0};
RTC_DATA_ATTR int ultimaHoraRegistrada = -1;

// função da interrupção do Geiger
void IRAM_ATTR contarPulsoISR() {
  pulsosDesdeUltimoWakeup++;
}

// Função auxiliar para imprimir array no Serial
void print_array_serial(const char* nomeArray, unsigned long arr[], int tamanho) {
  Serial.print(nomeArray);
  Serial.print(": [");
  for (int i = 0; i < tamanho; i++) {
    Serial.print(arr[i]);
    if (i < tamanho - 1) {
      Serial.print(", ");
    }
  }
  Serial.println("]");
}

// configura no modo Task
void watchdogConfig() {
  Serial.println("Configurando Watchdog Timer...");
  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << 0), // Para ESP32C3 (single core, core 0)
    .trigger_panic = true       
  };

  esp_err_t err = esp_task_wdt_init(&twdt_config);
  if (err == ESP_OK) {
    Serial.println("Watchdog Timer inicializado.");
    err = esp_task_wdt_add(NULL);
    if (err == ESP_OK) {
      Serial.println("Tarefa atual adicionada ao Watchdog.");
    } else {
      Serial.printf("Falha ao adicionar tarefa atual ao Watchdog: %s (%d)\n", esp_err_to_name(err), err);
    }
  } else {
    Serial.printf("Falha ao inicializar Watchdog Timer: %s (%d)\n", esp_err_to_name(err), err);
    if (err == ESP_ERR_INVALID_STATE) {
        Serial.println("WDT já inicializado? Tentando adicionar tarefa...");
        err = esp_task_wdt_add(NULL);
        if (err == ESP_OK) {
            Serial.println("Tarefa atual adicionada ao WDT (que já estava inicializado).");
        } else {
            Serial.printf("Falha ao adicionar tarefa ao WDT (que já estava inicializado): %s (%d)\n", esp_err_to_name(err), err);
        }
    }
  }
}

void conectarWifiEnviarDados() {
  esp_task_wdt_reset();
  Serial.println("Tentando conectar ao Wi-Fi para enviar dados...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
    esp_task_wdt_reset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // alterar esse formato para a nova biblioteca 
    DateTime agora = rtc.getDateTime(); // MODIFICAÇÃO: Usar getDateTime() para PCF8563

    Serial.println("--- Enviando Dados Acumulados por Hora ---");
    print_array_serial("Contagem Horaria", acumuladoPorHora, ARRAY_SIZE);
    Serial.print("Pulsos na hora atual (antes do reset): "); Serial.println(pulsosDesdeUltimoWakeup);
    
    delay(100);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Wi-Fi desconectado.");
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi.");
  }
  esp_task_wdt_reset();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  watchdogConfig(); // Configura o WDT primeiro
  esp_task_wdt_reset();

  // Inicialização do RTC (SEUS pinos I2C)
  // MODIFICAÇÃO: Wire.begin() para os pinos que você especificou
  if (!Wire.begin(4, 5)) { // GPIO10 como SDA, GPIO8 como SCL
     Serial.println("Falha ao iniciar Wire (I2C). Verifique os pinos SDA/SCL.");
     while(1) { esp_task_wdt_reset(); delay(1000); }
  }
  
  // MODIFICAR: Inicialização do PCF8563 para nova biblioteca!!!
  if (!rtc.begin()) {
    Serial.println("Erro: Não foi possível encontrar o RTC PCF8563!");
    while(1) { esp_task_wdt_reset(); delay(1000); }
  }
  Serial.println("RTC PCF8563 encontrado.");

  // MODIFICAÇÃO: Verificar perda de energia e ajustar hora (apenas na primeira vez ou após troca de bateria)
  // O PCF8563 não tem um método 'lostPower()' como o DS3231 na RTClib.
  // Você pode usar rtc.isSet() ou verificar uma data muito antiga para saber se precisa ajustar.
  // Para a primeira vez, descomente a linha abaixo e ajuste para a hora atual.
  /*
  Serial.println("Ajustando horário do RTC para data/hora da compilação...");
  DateTime comp_time(F(__DATE__), F(__TIME__));
  rtc.setDateTime(comp_time.year(), comp_time.month(), comp_time.day(), 
                  comp_time.hour(), comp_time.minute(), comp_time.second());
  Serial.println("Hora ajustada. COMENTE ESSA LINHA APÓS A PRIMEIRA CONFIGURAÇÃO!");
  */

  // MODIFICAR: alterar para o formato de alarme da nova biblioteca!!!
  rtc.setAlarm(0, -1, -1, -1); // Minuto=0, Hora=Qualquer, Dia do mês=Qualquer, Dia da semana=Qualquer
  rtc.enableAlarm();   // Habilita o alarme no PCF8563
  rtc.clearAlarm();    // Limpa qualquer flag de alarme anterior (importante para novas interrupções)
  Serial.println("Alarme do PCF8563 configurado para disparar a cada hora (minuto 00).");
  
  esp_task_wdt_reset();
  esp_sleep_wakeup_cause_t causaDespertar = esp_sleep_get_wakeup_cause();
  DateTime agora = rtc.getDateTime(); // MODIFICAR: Usar getDateTime() para a nova biblioteca

  Serial.print("Hora atual: ");
  Serial.print(agora.year()); Serial.print("/"); Serial.print(agora.month()); Serial.print("/"); Serial.print(agora.day());
  Serial.print(" "); Serial.print(agora.hour()); Serial.print(":"); Serial.print(agora.minute()); Serial.print(":"); Serial.println(agora.second());
  Serial.print(" (Dia da semana: "); Serial.print(agora.dayOfTheWeek()); Serial.println(")");


  // Lógica de processamento de pulsos acumulados
  // Esta parte precisa ser revisada para garantir que os pulsos sejam corretamente atribuídos à hora anterior
  // quando o despertar for pelo RTC (a cada hora).
  if (causaDespertar == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Boot normal (não de Deep Sleep). Inicializando contadores.");
    pulsosDesdeUltimoWakeup = 0;
    for(int i = 0; i < ARRAY_SIZE; i++) acumuladoPorHora[i] = 0;
    ultimaHoraRegistrada = agora.hour(); // Inicia com a hora atual
  } else {
    Serial.print("Despertou de Deep Sleep. Causa: ");
    if (causaDespertar == ESP_SLEEP_WAKEUP_EXT0 || causaDespertar == ESP_SLEEP_WAKEUP_GPIO) {
      // Se despertou pelo Geiger (GPIO) ou pelo RTC (EXT0)
      if (causaDespertar == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("RTC (EXT0 - Alarme).");
        // Limpa o flag de alarme do RTC para a próxima interrupção
        rtc.clearAlarm(); // MUITO IMPORTANTE!
      } else { // ESP_SLEEP_WAKEUP_GPIO
        Serial.println("Geiger (GPIO).");
      }

      // Se a hora mudou desde a última vez que registramos (o que acontece no wakeup do RTC)
      // ou se é o primeiro despertar da hora, acumulamos os pulsos da hora anterior.
      if (agora.hour() != ultimaHoraRegistrada && ultimaHoraRegistrada != -1) {
          acumuladoPorHora[ultimaHoraRegistrada] += pulsosDesdeUltimoWakeup;
          Serial.printf("Pulsos da hora %d (%lu) acumulados.\n", ultimaHoraRegistrada, pulsosDesdeUltimoWakeup);
          pulsosDesdeUltimoWakeup = 0; // Zera para a nova hora
      } else if (ultimaHoraRegistrada == -1) { // Primeiro despertar do dia
          // Pode ocorrer se o ESP32 resetar e o RTC mantiver a hora.
          // Neste caso, pulsosDesdeUltimoWakeup já deve estar em 0 do RTC_DATA_ATTR.
          // Apenas define a ultimaHoraRegistrada.
      }
      
      ultimaHoraRegistrada = agora.hour(); // Atualiza a última hora registrada
      
      // Se o wakeup foi pelo RTC (alarme horário), é hora de conectar e enviar
      if (causaDespertar == ESP_SLEEP_WAKEUP_EXT0) {
        conectarWifiEnviarDados();
      }

    } else {
      Serial.print("Causa desconhecida: "); Serial.println(causaDespertar);
    }
  }

  Serial.print("Pulsos desde último wakeup (acumulados na hora atual): "); Serial.println(pulsosDesdeUltimoWakeup);
  print_array_serial("Contagem Horaria Atual (Array)", acumuladoPorHora, ARRAY_SIZE);


  esp_task_wdt_reset();

  // --- Configuração para o próximo ciclo de sono ---
  // Desativa todas as fontes de wakeup anteriores (segurança)
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); 
  
  // 1. Configurar o wakeup pelo pino do Geiger
  pinMode(GEIGER_PIN, INPUT); 
  // O attachInterrupt é para contar pulsos ENQUANTO o ESP32 está acordado.
  // Para acordar do Deep Sleep, usamos esp_sleep_enable_gpio_wakeup.
  attachInterrupt(digitalPinToInterrupt(GEIGER_PIN), contarPulsoISR, (PULSE_GEIGER_LEVEL == 1 ? RISING : FALLING));
  
  if (gpio_wakeup_enable(GEIGER_PIN, (PULSE_GEIGER_LEVEL == 1 ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL)) == ESP_OK) {
      Serial.print("Wakeup por GPIO "); Serial.print(GEIGER_PIN); Serial.println(" (Geiger) habilitado.");
      if (esp_sleep_enable_gpio_wakeup() != ESP_OK) {
         Serial.println("Falha ao habilitar GPIO wakeup geral.");
      }
  } else {
      Serial.print("Falha ao habilitar wakeup para GPIO "); Serial.println(GEIGER_PIN);
  }

  // 2. Configurar o wakeup pelo alarme do RTC (pino INT do PCF8563)
  // O pino INT do PCF8563 vai para LOW quando o alarme dispara.
  if (esp_sleep_enable_ext0_wakeup(RTC_INT_PIN, LOW) == ESP_OK) {
    Serial.print("Wakeup por RTC (pino INT: GPIO"); Serial.print(RTC_INT_PIN); Serial.println(") habilitado.");
  } else {
    Serial.println("Falha ao habilitar wakeup por RTC (EXT0). Verifique o pino e a configuração.");
  }
  

  Serial.println("Entrando em Deep Sleep...");
  Serial.flush(); 
  esp_deep_sleep_start(); 
}

// --- Loop ---
void loop() {
  // O loop principal não é executado no Deep Sleep.
  // Tudo é tratado no setup() após o despertar.
}