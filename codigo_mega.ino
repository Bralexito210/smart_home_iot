#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <ArduinoJson.h>

// LCD I2C 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pines sensores
const int trigPin = 38;
const int echoPin = 36;
// Sensor de llama removido

// Umbral para apertura automática de puerta
const int umbralDistancia = 20; // cm - se abre si distancia < 20cm

// LEDs (digitales)
const int leds[] = {52, 50, 48, 46, 44, 42}; // 6 LEDs

// Servo (puerta)
Servo puerta;
const int pinServo = 5;
int estadoPuerta = 0; // 0 = cerrada, 1 = abierta

// Buzzer (opcional)
const int pinBuzzer = 4;

unsigned long tAnterior = 0;
const unsigned long intervalo = 2000; // cada 2 segundos

String comandoSerial = "";

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);  // Comunicación con ESP8266
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(pinBuzzer, OUTPUT);
  
  for (int i = 0; i < 6; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], 1); // Cambio: iniciar apagados
  }
  
  puerta.attach(pinServo);
  puerta.write(90); // cerrada
  
  lcd.setCursor(0, 0);
  lcd.print("Sistema iniciado");
}

void loop() {
  // Enviar datos periódicamente
  if (millis() - tAnterior >= intervalo) {
    enviarDatos();
    tAnterior = millis();
  }
  
  // Leer comandos del ESP8266
  while (Serial1.available()) {
    char c = Serial1.read();
    comandoSerial += c;
    if (c == '}') {
      if (comandoSerial.startsWith("#")) {
        comandoSerial.remove(0, 1); // Eliminar #
        procesarComando(comandoSerial);
      }
      comandoSerial = "";
    }
  }
}

// Enviar datos a ESP8266 como JSON
void enviarDatos() {
  int distancia = medirDistancia();
  
  // Control automático de puerta por distancia
  if (distancia < umbralDistancia && distancia > 0) {
    if (estadoPuerta == 0) { // Si está cerrada, abrirla
      estadoPuerta = 1;
      puerta.write(170); // Abrir puerta
      digitalWrite(pinBuzzer, HIGH);
      delay(100);
      digitalWrite(pinBuzzer, LOW);
    }
  } else if (distancia > umbralDistancia + 5) { // Histéresis para evitar oscilaciones
    if (estadoPuerta == 1) { // Si está abierta, cerrarla
      estadoPuerta = 0;
      puerta.write(90); // Cerrar puerta
    }
  }
  
  // Estado actual LEDs
  String ledsEstado = "";
  for (int i = 0; i < 6; i++) {
    ledsEstado += digitalRead(leds[i]);
  }
  
  StaticJsonDocument<200> doc;
  String salida;
  
  doc["u"] = distancia;     // ultrasonido
  doc["d"] = ledsEstado;    // estado LEDs
  doc["p"] = estadoPuerta;  // puerta
  
  serializeJson(doc, salida);
  Serial1.println("DATOS:" + salida); // Prefijo para identificar
  
  mostrarLCD( distancia);
}

// Mostrar en pantalla
void mostrarLCD( int dist) {
  lcd.clear();
  
  lcd.setCursor(0, 1);
  lcd.print("Dist:");
  lcd.print(dist);
  lcd.print("cm");
  
  lcd.setCursor(0, 2);
  lcd.print("Puerta:");
  lcd.print(estadoPuerta == 1 ? "Abierta" : "Cerrada");
  
  lcd.setCursor(0, 3);
  if (dist < umbralDistancia && dist > 0) {
    lcd.print("OBJETO DETECTADO");
  } else {
    lcd.print("Esperando...");
  }
}

// Medición del ultrasonido
int medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duracion = pulseIn(echoPin, HIGH, 30000);
  int distancia = duracion * 0.034 / 2;
  
  if (distancia == 0 || distancia > 400) return 999; // fuera de rango
  return distancia;
}

// Procesar JSON recibido desde el ESP
void procesarComando(String jsonStr) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    Serial.println("Error JSON");
    return;
  }
  
  // Actualizar LEDs (ahora busca campo "d")
  if (doc.containsKey("d")) {
    String luces = doc["d"];
    for (int i = 0; i < 6 && i < luces.length(); i++) {
      digitalWrite(leds[i], luces[i] == '1' ? HIGH : LOW);
    }
  }
  
  // Actualizar puerta (solo si no está en modo automático)
  if (doc.containsKey("p")) {
    int estado = doc["p"];
    // Solo permitir control manual si no hay objeto cerca
    int distanciaActual = medirDistancia();
    if (distanciaActual >= umbralDistancia || distanciaActual == 999) {
      estadoPuerta = estado;
      puerta.write(estado == 1 ? 170 : 90); // 170° abierta, 90° cerrada
    }
  }
  
  Serial.println("Comando procesado OK");
}