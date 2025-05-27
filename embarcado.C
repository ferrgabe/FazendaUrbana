// Este código executa no embarcado
// Apenas salvei aqui para facilitar a visualização

#include <CD74HC4067.h>  // Biblioteca para o multiplexador
#include <DHT.h>         // Biblioteca para o DHT22
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== CONFIGURAÇÕES DE REDE  ==========
const char* ssid = "SUA_REDE";
const char* password = "SUA_SENHA";
const char* serverUrl = "http://localhost:5000/api/data";

// ========== CONFIGURAÇÕES GERAIS ==========
// Pinos e parâmetros do LDR e SSR
const int pinoLDR = A0;        // Pino analógico do LDR
const int pinoSSR = 7;         // Pino de controle do SSR
const int R_fixo = 10000;      // Resistor de 10kΩ para o LDR
const int limiarEscuro = 50000; // Limiar para escuro (50kΩ)

// ========== CONFIGURAÇÃO DO DHT22 ==========
#define DHTPIN 6               // Pino digital conectado ao DHT22
#define DHTTYPE DHT22          // Tipo do sensor
DHT dht(DHTPIN, DHTTYPE);      // Objeto DHT

// ========== CONFIGURAÇÕES DOS MULTIPLEXADORES ==========
const int numMultiplexadores = 4;  // 4 multiplexadores (64 canais totais)
const int pinosControle[4] = {8, 9, 10, 11};  // Pinos comuns de controle S0-S3
const int pinosSinal[numMultiplexadores] = {A1, A2, A3, A4}; // Pinos analógicos para cada multiplexador

// Array de objetos multiplexador para sensores
CD74HC4067 mux[numMultiplexadores] = {
  CD74HC4067(pinosControle[0], pinosControle[1], pinosControle[2], pinosControle[3]),
  CD74HC4067(pinosControle[0], pinosControle[1], pinosControle[2], pinosControle[3]),
  CD74HC4067(pinosControle[0], pinosControle[1], pinosControle[2], pinosControle[3]),
  CD74HC4067(pinosControle[0], pinosControle[1], pinosControle[2], pinosControle[3])
};

// ========== CONFIGURAÇÕES DOS MULTIPLEXADORES DE SAÍDA ==========
const int numMultiplexadoresSaida = 4;
const int pinosControleSaida[4] = {A5, 12, 13, A6};
const int pinosSinalSaida[numMultiplexadoresSaida] = {2, 3, 4, 5};

// Array de objetos multiplexador para válvulas
CD74HC4067 muxSaida[numMultiplexadoresSaida] = {
  CD74HC4067(pinosControleSaida[0], pinosControleSaida[1], pinosControleSaida[2], pinosControleSaida[3]),
  CD74HC4067(pinosControleSaida[0], pinosControleSaida[1], pinosControleSaida[2], pinosControleSaida[3]),
  CD74HC4067(pinosControleSaida[0], pinosControleSaida[1], pinosControleSaida[2], pinosControleSaida[3]),
  CD74HC4067(pinosControleSaida[0], pinosControleSaida[1], pinosControleSaida[2], pinosControleSaida[3])
};

// ========== REGRAS DE NEGÓCIO ==========
const int NUM_ARVORES = 20;      // Sensores 0-19 (Árvores)
const int NUM_VASOS = 30;        // Sensores 20-49 (Vasos)
const int NUM_HORTA = 2;         // Sensores 50-51 (Horta)
const int UMIDADE_MIN_ARVORES = 30;
const int UMIDADE_MIN_VASOS = 20;
const int UMIDADE_MIN_HORTA = 50;
const int NUM_VALVULAS = 52;     // 1 válvula por sensor
#define TEMPO_IRRIGACAO 5000     // 5 segundos de irrigação

// ========== VARIÁVEIS DE CONTROLE ==========
unsigned long ultimaLeituraLDR = 0;
unsigned long ultimaLeituraDHT = 0;
const unsigned long intervaloLeitura = 30000;  // 30 segundos
const unsigned long intervaloLeituraSensores = 3000; // 3 segundos
const unsigned long intervaloLeituraDHT = 5000; // 5 segundos (2 é o mínimo para DHT22)

struct ValveStatus {
  bool isOpen;
  unsigned long lastActivation;
  int activationCount;
};

ValveStatus valveStatus[NUM_VALVULAS] = {0};

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  pinMode(pinoSSR, OUTPUT);
  
  // Configura os pinos de controle dos multiplexadores como saída
  for (int i = 0; i < 4; i++) {
    pinMode(pinosControle[i], OUTPUT);
    pinMode(pinosControleSaida[i], OUTPUT);
  }
  
  // Configura os pinos de sinal de saída
  for (int i = 0; i < numMultiplexadoresSaida; i++) {
    pinMode(pinosSinalSaida[i], OUTPUT);
  }
  
  dht.begin(); // Inicializa o sensor DHT22

    WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }
  Serial.println("Conectado!");
  
  Serial.println("Sistema iniciado com sucesso!");
  Serial.println("Configuração:");
  Serial.println("- 52 Sensores FC-28 (20 Árvores, 30 Vasos, 2 Horta)");
  Serial.println("- 52 Válvulas solenoides (1:1)");
  Serial.println("- Sensor DHT22 (Temperatura e Umidade)");
  Serial.println("- Controle de iluminação automática");
}

// ========== LOOP PRINCIPAL ==========
void loop() {
  leituraLDR();      // Verifica o LDR a cada 30 segundos
  lerDHT22();        // Lê o DHT22 a cada 2 segundos
  lerSensoresFC28(); // Lê os 52 sensores de umidade
  verificarValvulas(); // Verifica válvulas que precisam ser desligadas
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 60000) { // Envia a cada 1 minuto
    sendDataToBackend(temperatura, umidade, R_LDR, valveStatus);
    lastSend = millis();
  }
  
  delay(intervaloLeituraSensores); // Intervalo geral do loop
}

void sendDataToBackend(float temp, float umid, float ldr, ValveStatus valves[]) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Criar JSON
    DynamicJsonDocument doc(2048);
    doc["temperature"] = temp;
    doc["humidity"] = umid;
    doc["ldrResistance"] = ldr;
    doc["lightsStatus"] = (ldr > limiarEscuro);

    JsonArray soilSensors = doc.createNestedArray("soilSensors");
    for (int i = 0; i < NUM_VALVULAS; i++) {
      JsonObject sensor = soilSensors.createNestedObject();
      sensor["sensorId"] = i;
      sensor["moisture"] = /* Ler umidade do sensor i */;
      sensor["activationCount"] = valves[i].activationCount;
    }

    String payload;
    serializeJson(doc, payload);
    int httpCode = http.POST(payload);

    if (httpCode > 0) {
      Serial.printf("Dados enviados! Código: %d\n", httpCode);
    } else {
      Serial.printf("Erro na requisição: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

// ========== FUNÇÕES DE LEITURA ==========
void leituraLDR() {
  unsigned long tempoAtual = millis();

  if (tempoAtual - ultimaLeituraLDR >= intervaloLeitura) {
    ultimaLeituraLDR = tempoAtual;

    int valorAnalogico = analogRead(pinoLDR);
    float R_LDR = R_fixo * (1023.0 / valorAnalogico - 1) / 1000;  // Em kΩ

    Serial.print("\n[LDR] Resistência: ");
    Serial.print(R_LDR);
    Serial.print(" kΩ | Status: ");
    
    if (R_LDR > limiarEscuro) {
      digitalWrite(pinoSSR, HIGH);
      Serial.println("Luzes LIGADAS (Ambiente escuro)");
    } else {
      digitalWrite(pinoSSR, LOW);
      Serial.println("Luzes DESLIGADAS (Ambiente claro)");
    }
  }
}

void lerDHT22() {
  unsigned long tempoAtual = millis();
  
  if (tempoAtual - ultimaLeituraDHT >= intervaloLeituraDHT) {
    ultimaLeituraDHT = tempoAtual;
    
    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();
    
    if (!isnan(umidade) && !isnan(temperatura)) {
      Serial.print("[DHT22] Temp: ");
      Serial.print(temperatura);
      Serial.print("°C | Umidade: ");
      Serial.print(umidade);
      Serial.println("%");
    } else {
      Serial.println("[DHT22] Erro na leitura!");
    }
  }
}

void lerSensoresFC28() { 
  for (int muxNum = 0; muxNum < numMultiplexadores; muxNum++) {
    digitalWrite(pinosControle[0] + muxNum, HIGH);
    
    for (int channel = 0; channel < 16; channel++) {
      int sensorId = muxNum * 16 + channel;
      if (sensorId >= 52) break;
      
      mux[muxNum].setChannel(channel);
      int valorFC28 = analogRead(pinosSinal[muxNum]);
      float umidadePercentual = map(valorFC28, 0, 1023, 100, 0);

      controlarIrrigacao(sensorId, umidadePercentual);
      logSensor(sensorId, umidadePercentual);
      
      delay(20);
    }
    
    digitalWrite(pinosControle[0] + muxNum, LOW);
  }
}

// ========== FUNÇÕES DE CONTROLE ==========
void controlarIrrigacao(int sensorId, float umidade) {
  int umidadeMinima;
  
  if (sensorId < NUM_ARVORES) {
    umidadeMinima = UMIDADE_MIN_ARVORES;
  } else if (sensorId < NUM_ARVORES + NUM_VASOS) {
    umidadeMinima = UMIDADE_MIN_VASOS;
  } else {
    umidadeMinima = UMIDADE_MIN_HORTA;
  }

  if (umidade < umidadeMinima) {
    ativarSolenoide(sensorId, true);
  } else {
    ativarSolenoide(sensorId, false);
  }
}

void ativarSolenoide(int sensorId, bool estado) {
  if (sensorId >= NUM_VALVULAS) return;
  
  if (estado && !valveStatus[sensorId].isOpen) {
    int muxNum = sensorId / 16;
    int channel = sensorId % 16;
    
    muxSaida[muxNum].setChannel(channel);
    digitalWrite(pinosSinalSaida[muxNum], HIGH);
    
    valveStatus[sensorId].isOpen = true;
    valveStatus[sensorId].lastActivation = millis();
    valveStatus[sensorId].activationCount++;
    
    logValveStatus(sensorId);
  }
  // IMPORTANTE: Entra na condição abaixo caso ainda esteja dentro do tempo de irrigação anterior.
  else if (!estado && valveStatus[sensorId].isOpen && 
          (millis() - valveStatus[sensorId].lastActivation) > TEMPO_IRRIGACAO) {
    int muxNum = sensorId / 16;
    int channel = sensorId % 16;
    
    muxSaida[muxNum].setChannel(channel);
    digitalWrite(pinosSinalSaida[muxNum], LOW);
    
    valveStatus[sensorId].isOpen = false;
    logValveStatus(sensorId);
  }
}

// IMPORTANTE: É executado em cada loop, avalia se tem válvulas ligadas que já "excederam" seu tempo de exec.
void verificarValvulas() {
  for (int i = 0; i < NUM_VALVULAS; i++) {
    if (valveStatus[i].isOpen && (millis() - valveStatus[i].lastActivation) > TEMPO_IRRIGACAO) {
      ativarSolenoide(i, false);
    }
  }
}

// ========== FUNÇÕES AUXILIARES ==========
void logSensor(int id, float umidade) {
  String tipoPlanta = "";
  if (id < NUM_ARVORES) tipoPlanta = "Árvore";
  else if (id < NUM_ARVORES + NUM_VASOS) tipoPlanta = "Vaso";
  else tipoPlanta = "Horta";

  Serial.print("[Sensor #");
  Serial.print(id);
  Serial.print(" (");
  Serial.print(tipoPlanta);
  Serial.print(")] Umidade: ");
  Serial.print(umidade);
  Serial.print("% | Válvula ");
  Serial.print(id);
  Serial.print(": ");
  Serial.println(umidade < getUmidMinPorTipo(id) ? "IRRIGAR" : "OK");
}

void logValveStatus(int valveId) {
  Serial.print("[Válvula ");
  Serial.print(valveId);
  Serial.print("] Status: ");
  Serial.print(valveStatus[valveId].isOpen ? "ABERTA" : "FECHADA");
  Serial.print(" | Ativações: ");
  Serial.print(valveStatus[valveId].activationCount);
  Serial.print(" | Tempo: ");
  Serial.print((millis() - valveStatus[valveId].lastActivation)/1000);
  Serial.println("s");
}

int getUmidMinPorTipo(int sensorId) {
  if (sensorId < NUM_ARVORES) return UMIDADE_MIN_ARVORES;
  if (sensorId < NUM_ARVORES + NUM_VASOS) return UMIDADE_MIN_VASOS;
  return UMIDADE_MIN_HORTA;
}