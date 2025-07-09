#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Jose";
const char* password = "08535588";
const char* flask_server = "http://192.168.18.110:5000"; // IP del servidor Flask

ESP8266WebServer server(80);
WiFiClient client;

String comandoPendiente = "";
String bufferSerial = "";

unsigned long ultimoEnvio = 0;
const unsigned long intervaloEnvio = 3000; // Enviar datos cada 3 segundos

void setup() {
  Serial.begin(9600);  // Comunicación con el Arduino Mega
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
  
  // Ruta para recibir comandos del Flask
  server.on("/comando", HTTP_POST, handleComando);
  
  // Ruta para verificar estado
  server.on("/status", HTTP_GET, [](){
    server.send(200, "application/json", "{\"status\":\"online\"}");
  });
  
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
  
  // Leer datos del Arduino
  while (Serial.available()) {
    char c = Serial.read();
    bufferSerial += c;
    
    if (c == '\n') {
      procesarDatosArduino(bufferSerial);
      bufferSerial = "";
    }
  }
  
  // Si hay comando pendiente, enviarlo al Arduino
  if (comandoPendiente.length() > 4 && comandoPendiente.startsWith("#")) {
    Serial.print(comandoPendiente);
    Serial.println("Comando enviado al Arduino: " + comandoPendiente);
    comandoPendiente = "";
  }
}

// Procesar datos recibidos del Arduino
void procesarDatosArduino(String datos) {
  if (datos.startsWith("DATOS:")) {
    String json = datos.substring(6); // Quitar "DATOS:"
    json.trim();
    
    // Enviar datos al servidor Flask
    enviarDatosAFlask(json);
  }
}

// Enviar datos al servidor Flask
void enviarDatosAFlask(String jsonData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, String(flask_server) + "/datos");
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Datos enviados a Flask: " + String(httpCode));
    } else {
      Serial.println("Error enviando datos a Flask: " + String(httpCode));
    }
    
    http.end();
  }
}

// Manejar comandos recibidos del Flask
void handleComando() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Falta cuerpo JSON\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON inválido\"}");
    return;
  }
  
  // Crear comando para Arduino
  String comando = "#{\"d\":\"";
  
  if (doc.containsKey("d")) {
    comando += doc["d"].as<String>();
  } else {
    comando += "000000"; // Default
  }
  
  comando += "\",\"p\":";
  
  if (doc.containsKey("p")) {
    comando += doc["p"].as<int>();
  } else {
    comando += "0"; // Default
  }
  
  comando += "}";
  
  comandoPendiente = comando;
  Serial.println("Comando preparado para Arduino: " + comando);
  
  server.send(200, "application/json", "{\"status\":\"comando recibido\"}");
}

// Obtener comandos del Flask periódicamente
void verificarComandosFlask() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, String(flask_server) + "/comando");
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String comando = http.getString();
      if (comando.startsWith("#")) {
        comandoPendiente = comando;
        Serial.println("Comando recibido de Flask: " + comando);
      }
    }
    
    http.end();
  }
}