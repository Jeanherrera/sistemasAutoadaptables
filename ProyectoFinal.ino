#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#include <WiFi.h>
//#include <WebSocketsClient_Generic.h>
//#include <ArduinoJson.h>

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pines semáforo 1
#define RED_LED_PIN_1     5
#define YELLOW_LED_PIN_1  4
#define GREEN_LED_PIN_1   6

// Pines semáforo 2
#define RED_LED_PIN_2     7
#define YELLOW_LED_PIN_2  15
#define GREEN_LED_PIN_2   16

// Sensores infrarrojos carril 1 (CNY4–CNY6)
#define CNY4 39
#define CNY5 38
#define CNY6 37

// Sensores infrarrojos carril 2 (CNY1–CNY3)
#define CNY1 42
#define CNY2 41
#define CNY3 40

// Sensores ambientales
#define SENSOR_PIN        12  // Visibilidad
#define CO2_SENSOR_PIN    14  // CO₂

// Estados del semáforo
#define RED              0
#define RED_TO_YELLOW    1
#define GREEN            2
#define GREEN_TO_YELLOW  3

const char* ssid = "Usuario";
const char* password = "Clave";

//WebSocketsClient webSocket;

unsigned long lastSent = 0;
const unsigned long interval = 5000;

int state = 0;
bool bad = false;
bool co2High = false;
int sensor_other_city = 0, sensorValue = 0, local_visibility = 0;


unsigned long tini, tactual, tdelta;
bool R = false, A = false, V = false;

void setup() {
  Serial.begin(115200);
  config_display();
  pinModes();
  //connection();  // WiFi y WebSocket
  allOff();
}

void pinModes(){

  // Sensor CO2
  pinMode(CO2_SENSOR_PIN, INPUT);
  
  // Sensor visibilidad
  pinMode(SENSOR_PIN, INPUT);

  // Sensores carril #1
  pinMode(CNY1, INPUT);
  pinMode(CNY2, INPUT);
  pinMode(CNY3, INPUT);

  // Sensores carril #2
  pinMode(CNY4, INPUT);
  pinMode(CNY5, INPUT);
  pinMode(CNY6, INPUT);

  // Semaforo #1
  pinMode(RED_LED_PIN_1, OUTPUT);
  pinMode(YELLOW_LED_PIN_1, OUTPUT);
  pinMode(GREEN_LED_PIN_1, OUTPUT);

  // Semaforo #2
  pinMode(RED_LED_PIN_2, OUTPUT);
  pinMode(YELLOW_LED_PIN_2, OUTPUT);
  pinMode(GREEN_LED_PIN_2, OUTPUT);
}

void config_display(){
  lcd.init();            // Inicializa la pantalla
  lcd.backlight();       // Enciende luz de fondo
  lcd.setCursor(0, 0);
  lcd.print("Smart City EAFIT");
}

void read_CO2_sensor(){
  sensorValue = analogRead(CO2_SENSOR_PIN);
  co2High = (sensorValue > 700);
}

void measure(){
  int sensorValue = analogRead(SENSOR_PIN);
  tactual = millis();
  tdelta = tactual - tini;
  bad = read_visibility();
}

bool read_visibility() {
  local_visibility = analogRead(SENSOR_PIN);

  if (sensor_other_city != 0) {
    return (local_visibility < 600) || (sensor_other_city < 600);
  }

  return (local_visibility < 600);
}

void show_values_display(){
  lcd.setCursor(0, 1);
  lcd.print("CO2:");
  lcd.print(sensorValue);
  lcd.print(" L:");
  lcd.print(local_visibility);
  lcd.print(" O:");
  lcd.print(sensor_other_city);
}

void loop() {
  
 // webSocket.loop();          // Procesar WebSocket
  //sendSignalWebsocket();     // Enviar datos cada intervalo

  measure();                 // Leer visibilidad
  read_CO2_sensor();         // Mostrar CO₂ en LCD

  // Mostras los valores de los sensores
  show_values_display();

  bool VehicleLine1 = lane_in_1();
  bool VehicleLine2 = lane_in_2();

  /*if (co2High || bad) {
    priorityLine2();
  }
  else*/ if (VehicleLine1) {
    priorityLine2();
  }
  else if (VehicleLine2) {
    priorityLine1();
  }
  else {
    NoVehicleMode();
  }
}

void priorityLine1() {
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Paso: Carril #1   ");
  digitalWrite(RED_LED_PIN_2, HIGH);
  digitalWrite(GREEN_LED_PIN_1, HIGH);
  delay(5000);
}

void priorityLine2() {
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Paso: Carril #2   ");
  digitalWrite(RED_LED_PIN_1, HIGH);
  digitalWrite(GREEN_LED_PIN_2, HIGH);
  delay(5000);
}

void NoVehicleMode(){
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Modo sin vehiculos ");
  yellow_flashing_both();;
}

void check(){
    switch (state) {
    case RED:
      if (bad) {
        state = GREEN;
        tini = millis();
        break;
      }

      R = 1;
      A = 0;
      V = 0;

      if (tdelta >= 3000) {
        state = RED_TO_YELLOW;
        Serial.println("Estado 1: Rojo a Amarillo");
        tini = millis();
      }
      break;

    case RED_TO_YELLOW:
      if (bad) {
        state = GREEN;
        tini = millis();
        break;
      }

      R = 0;
      A = 1;
      V = 0;

      if (tdelta >= 2000) {
        state = GREEN;
        Serial.println("Estado 2: Verde");
        tini = millis();
      }
      break;

    case GREEN:
      R = 0;
      A = 0;
      V = 1;

      if (bad) {
        tini = millis();
        break;
      }

      if (tdelta >= 6000) {
        state = GREEN_TO_YELLOW  ;
        Serial.println("Estado 3: Verde a Amarillo");
        tini = millis();
      }
      break;

    case GREEN_TO_YELLOW:
      if (bad) {
        state = GREEN;
        tini = millis();
        break;
      }

      R = 0;
      A = 1;
      V = 0;

      if (tdelta >= 2000) {
        state = RED;
        Serial.println("Estado 0: Rojo");
        tini = millis();
      }
      break;

    default:
      break;
  }
}

void act_line_2() {
  digitalWrite(RED_LED_PIN_2, R);
  digitalWrite(YELLOW_LED_PIN_1, A);
  digitalWrite(GREEN_LED_PIN_1, V);
}

bool lane_in_1(){
  return digitalRead(CNY4) == LOW || digitalRead(CNY5) == LOW  || digitalRead(CNY6) == LOW ;
}

bool lane_in_2() {
  return digitalRead(CNY1) == LOW || digitalRead(CNY2) == LOW || digitalRead(CNY3) == LOW;
}

void yellow_flashing_line_2(){
  digitalWrite(YELLOW_LED_PIN_1, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED_PIN_1, LOW);
  delay(500);
}

void allOff(){

  // Semaforo 1
  digitalWrite(RED_LED_PIN_1, LOW);
  digitalWrite(YELLOW_LED_PIN_1, LOW);
  digitalWrite(GREEN_LED_PIN_1, LOW);

  // Semaforo 2
  digitalWrite(RED_LED_PIN_2, LOW);
  digitalWrite(YELLOW_LED_PIN_2, LOW);
  digitalWrite(GREEN_LED_PIN_2, LOW);
}

void yellow_flashing_both() {
  digitalWrite(YELLOW_LED_PIN_1, HIGH);
  digitalWrite(YELLOW_LED_PIN_2, HIGH);
  delay(500);
  digitalWrite(YELLOW_LED_PIN_1, LOW);
  digitalWrite(YELLOW_LED_PIN_2, LOW);
  delay(500);
}

/*void connection(){
  // Wifi connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");

  // Setup secure WebSocket client (wss)
  webSocket.beginSSL("ws.davinsony.com", 443, "/city_sebas");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}*/

// Handle incoming WebSocket messages
/*void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Connected to WebSocket server");
      break;
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");
      break;
    case WStype_TEXT:
      {
        Serial.printf("Received: %s\n", payload);
        // Parse the incoming JSON message
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print("JSON parsing failed: ");
          Serial.println(error.c_str());
          break;
        }
        const char* msg = doc["msg"];
        sensor_other_city = int(doc["sensor"]);

        Serial.println(sensor_other_city);

        Serial.print("msg: ");
        Serial.println(msg);
      }
      break;
    default:
      break;
  }
}*/

/*void sendSignalWebsocket() {
  webSocket.loop();

  unsigned long now = millis();
  if (now - lastSent > 2000 && webSocket.isConnected()) {
    lastSent = now;
    int sensorValue = analogRead(SENSOR_PIN);

    // Optional: use JSON format
    StaticJsonDocument<100> doc;
    doc["sensor"] = sensorValue;
    doc["msg"] = sensorValue;
    doc["to"] = "city_2";
    String json;
    serializeJson(doc, json);

    webSocket.sendTXT(json);  // send sensor reading
  }
}*/
