#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>  // Biblioteca para fazer chamadas HTTP
#include <PubSubClient.h>       // Biblioteca para MQTT

#define DHTPIN 2    // Pino onde o sensor DHT está conectado
#define DHTTYPE DHT11  // Tipo do sensor DHT (DHT11)

DHT dht(DHTPIN, DHTTYPE);  // Inicializa o sensor DHT

const char* ssid = "your_SSID";           // Nome da rede WiFi
const char* password = "your_PASSWORD";   // Senha da rede WiFi
const char* mqtt_server = "broker.mqtt.com";  // Endereço do servidor MQTT

WiFiClient espClient;
PubSubClient client(espClient);

// Função para conectar ao WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);

  // Inicia a conexão WiFi
  WiFi.begin(ssid, password);

  // Aguarda até que a conexão seja estabelecida
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Tentando conectar ao WiFi...");
  }

  Serial.println("Conectado ao WiFi!");
}

void setup() {
  Serial.begin(115200);  // Inicia a comunicação serial
  dht.begin();           // Inicia o sensor DHT

  setup_wifi();          // Conecta ao WiFi
  client.setServer(mqtt_server, 1883);  // Configura o servidor MQTT
}

void loop() {
  // Lê a temperatura e a umidade do sensor DHT
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Verifica se as leituras são válidas
  if (isnan(temp) || isnan(humidity)) {
    Serial.println("Falha ao ler o sensor DHT!");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temp);
  Serial.print(" °C, Umidade: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Enviar dados via HTTP
  if (WiFi.status() == WL_CONNECTED) {  // Verifica se está conectado ao WiFi
    HTTPClient http;
    http.begin("http://seuservidor.com/api/dados");  // URL do servidor para enviar os dados
    http.addHeader("Content-Type", "application/json");  // Cabeçalho da requisição

    // Monta o payload com os dados em formato JSON
    String payload = "{\"temperatura\": " + String(temp) + ", \"umidade\": " + String(humidity) + "}";
    
    int httpResponseCode = http.POST(payload);  // Envia a requisição POST com os dados
    
    // Verifica a resposta do servidor
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Resposta do servidor: " + response);
    } else {
      Serial.println("Erro na requisição HTTP");
    }

    http.end();  // Fecha a conexão HTTP
  }

  // Enviar dados via MQTT
  if (!client.connected()) {
    reconnect_mqtt();  // Se não estiver conectado, tenta reconectar ao servidor MQTT
  }
  client.loop();  // Mantém a conexão MQTT ativa

  // Publica os dados no tópico MQTT
  String mqtt_payload = "Temperatura: " + String(temp) + " °C, Umidade: " + String(humidity) + " %";
  client.publish("sensor/dados", mqtt_payload.c_str());

  delay(2000);  // Espera 2 segundos antes de fazer uma nova leitura
}

// Função para reconectar ao servidor MQTT
void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao servidor MQTT...");
    if (client.connect("ESP8266Client")) {
      Serial.println("Conectado ao servidor MQTT!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000);  // Aguarda 5 segundos antes de tentar reconectar
    }
  }
}
