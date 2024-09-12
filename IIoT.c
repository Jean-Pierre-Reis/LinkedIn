#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>  // Biblioteca para NVS (Non-Volatile Storage)

// Configurações do sensor DHT
#define DHTPIN 2
#define DHTTYPE DHT11
#define MAX_RETRIES 5
#define RETRY_DELAY 2000
#define SEND_INTERVAL 5000  // Intervalo de envio de dados (em milissegundos)

// Configurações HTTP seguras (OAuth2)
const char* serverName = "https://seu-servidor-seguro.com/dados";

// Configurações MQTT com TLS e certificado de cliente
const char* mqtt_server = "seu-servidor-mqtt-seguro.com";
const char* mqtt_topic = "IIoT/dados";
const int mqtt_port = 8883;  // Porta MQTT segura (TLS)
WiFiClientSecure espClientSecure;
PubSubClient mqttClient(espClientSecure);

// Certificados e chave privada do cliente MQTT
const char* client_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIID....\n"  // Certificado do cliente
"-----END CERTIFICATE-----\n";

const char* client_key = \
"-----BEGIN PRIVATE KEY-----\n" \
"MIIEv....\n"  // Chave privada do cliente
"-----END PRIVATE KEY-----\n";

const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIID....\n"  // Certificado da CA do servidor
"-----END CERTIFICATE-----\n";

// Variáveis para controle de tempo
unsigned long lastSendTime = 0;
DHT dht(DHTPIN, DHTTYPE);

// NVS para armazenar credenciais
Preferences preferences;

// Função para armazenar as credenciais no NVS
void saveCredentialsToNVS(const char* ssid, const char* password, const char* token) {
  preferences.begin("credStorage", false);
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_password", password);
  preferences.putString("oauth_token", token);
  preferences.end();
}

// Função para carregar as credenciais do NVS
void loadCredentialsFromNVS(String &ssid, String &password, String &token) {
  preferences.begin("credStorage", true);
  ssid = preferences.getString("wifi_ssid", "");
  password = preferences.getString("wifi_password", "");
  token = preferences.getString("oauth_token", "");
  preferences.end();
}

// Função para autenticar e obter o token OAuth via HTTPS
String authenticateOAuth() {
  WiFiClientSecure client;
  HTTPClient https;
  client.setInsecure();  // Ou use client.setCACert(ca_cert) se tiver o certificado da CA

  https.begin(client, "https://seu-servidor-oauth.com/token");  // URL do servidor OAuth
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "grant_type=client_credentials&client_id=your_client_id&client_secret=your_client_secret";
  int httpCode = https.POST(postData);

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();
    String token = payload.substring(payload.indexOf("access_token\":\"") + 15);
    token = token.substring(0, token.indexOf("\""));

    // Atualiza o token na NVS
    saveCredentialsToNVS("your_SSID", "your_PASSWORD", token);

    return token;
  } else {
    Serial.println("Erro ao autenticar OAuth");
    return "";
  }
}

// Função para conectar ao WiFi com tratamento de erros e tentativas
void connectToWiFi(const char* ssid, const char* password) {
  int retries = 0;
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && retries < MAX_RETRIES) {
    Serial.print("Tentando conectar ao WiFi... Tentativa ");
    Serial.println(retries + 1);
    retries++;
    delay(RETRY_DELAY);  // Espera entre tentativas
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado ao WiFi.");
  } else {
    Serial.println("Falha ao conectar ao WiFi após múltiplas tentativas.");
    ESP.restart();  // Reinicia o dispositivo em caso de falha crítica
  }
}

// Função para conectar ao servidor MQTT com certificado de cliente
void connectToMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  espClientSecure.setCACert(ca_cert);  // Certificado da CA
  espClientSecure.setCertificate(client_cert);  // Certificado do cliente
  espClientSecure.setPrivateKey(client_key);  // Chave privada do cliente

  while (!mqttClient.connected()) {
    Serial.println("Conectando ao MQTT com certificado...");
    if (mqttClient.connect("ESP8266Client")) {
      Serial.println("Conectado ao MQTT.");
    } else {
      Serial.print("Falha ao conectar. Código de erro: ");
      Serial.println(mqttClient.state());
      delay(RETRY_DELAY);  // Aguarda antes de tentar novamente
    }
  }
}

// Função para enviar dados via HTTPS com OAuth2
void sendDataHTTPS(float temperature, float humidity, String token) {
  if (WiFi.status() == WL_CONNECTED) { // Verifica se está conectado ao WiFi
    HTTPClient https;
    espClientSecure.setCACert(ca_cert);  // Certificado para HTTPS
    https.begin(espClientSecure, serverName);  // Inicializa HTTPS
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    https.addHeader("Authorization", "Bearer " + token);  // Adiciona o token OAuth

    String httpRequestData = "temperature=" + String(temperature) + "&humidity=" + String(humidity);
    int httpResponseCode = https.POST(httpRequestData);

    if (httpResponseCode > 0) {
      Serial.print("Código de resposta HTTPS: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Erro na solicitação HTTPS: ");
      Serial.println(https.errorToString(httpResponseCode).c_str());
    }

    https.end();  // Fecha a conexão HTTPS
  } else {
    Serial.println("Erro: Não conectado ao WiFi para enviar dados HTTPS.");
  }
}

// Função de inicialização do sistema
void setup() {
  Serial.begin(115200);
  dht.begin();  // Inicializa o sensor DHT

  // Carregar as credenciais da NVS
  String ssid, password, token;
  loadCredentialsFromNVS(ssid, password, token);

  // Se as credenciais estiverem vazias, elas precisam ser inseridas manualmente
  if (ssid == "" || password == "") {
    Serial.println("Insira as credenciais de WiFi...");
    // Aqui, você pode adicionar uma lógica para capturar as credenciais, por exemplo, via Bluetooth ou Serial.
    saveCredentialsToNVS("your_SSID", "your_PASSWORD", "");  // Salva as novas credenciais
    loadCredentialsFromNVS(ssid, password, token);  // Carrega as credenciais atualizadas
  }

  // Conecta ao WiFi
  connectToWiFi(ssid.c_str(), password.c_str());

  // Configura o servidor MQTT com segurança TLS e certificado de cliente
  mqttClient.setServer(mqtt_server, mqtt_port);
  espClientSecure.setCACert(ca_cert);
  espClientSecure.setCertificate(client_cert);
  espClientSecure.setPrivateKey(client_key);

  // Autenticar OAuth se o token estiver vazio
  if (token == "") {
    token = authenticateOAuth();  // Obtém novo token OAuth
  }
}

// Loop principal
void loop() {
  // Verifica se já é hora de enviar os dados
  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = currentMillis;

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    // Lê os dados do sensor
    if (!isnan(temperature) && !isnan(humidity)) {
      Serial.print("Temperatura: ");
      Serial.print(temperature);
      Serial.print(" °C, Umidade: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Envia os dados via HTTPS (com OAuth)
      String token;
      preferences.begin("credStorage", true);  // Acessa o token salvo
      token = preferences.getString("oauth_token", "");
      preferences.end();
      sendDataHTTPS(temperature, humidity, token);
    } else {
      Serial.println("Erro: Falha ao ler dados do sensor.");
    }
  }

  mqttClient.loop();  // Mantém a conexão MQTT ativa
}
