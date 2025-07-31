// Entradas
// 2 Sensores de luz (Fotoceldas que se activan cuando superan el umbral de 600)
// 3 Sensores infrarojos a cada lado (6 Sensores)
// 1 Sensor de CO2

// Salidas
// 2 Semaforos 
// 1 Pantalla

// Modo normal: Parpadeo Amarillo
// Si un carro llega por un carril: El carril se pone en verde y el otro se pone en rojo
// Si dos carros llegan, el primer carro en llegar tiene prioridad y funciona normal hasta que no se detecten mas carros

// Si hay carros en un carril y se oscurece, se pone en verde y el otro en rojo hasta que no hayan carros y cambia a rojo
// Si ambos se oscurecen, ambos se ponen en parpadeo amarillo
// Si está de noche y llega un carro, se le da prioridad y el otro se pone en rojo

// Si el sensor de CO2 está alto y hay carros el carril se pone en verde y el otro en rojo hasta que los carros 


// Estados
// - Parpadeo Amarillo:   (S1 & S2 == 0  &&  veh_c1 === 0 & veh_c2 == 0)
// - Parpadeo Normal:     (S1 && S2 && !CO2 && !B1 && B2)
// - Prioridad carril  1  (1 en verde --> 2 en rojo): (CNY1 && !S1) || B2
// - Prioridad carrilo 2  (2 en verde --> 1 en rojo): (CNY2 && (!S2 || CO2)) || B1

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

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
#define SENSOR_PIN_S1     12  // Visibilidad
#define SENSOR_PIN_S2     13  // Visibilidad
#define CO2_SENSOR_PIN    14  // CO₂

// Estados del semáforo
#define RED              0
#define RED_TO_YELLOW    1
#define GREEN            2
#define GREEN_TO_YELLOW  3

// Variables que cambian:
int YELLOW_LED_PIN_1_STATE = LOW;
int YELLOW_LED_PIN_2_STATE = LOW;

const char* ssid = "AirportAgent01";
const char* password = "15427605";

WebSocketsClient webSocket;

unsigned long previousMillis = 0;
const unsigned long interval = 100;

int state = 0;
bool vlS1 = false, vlS2 = false;
bool co2High = false;
int sensor_other_city = 0, sensorValueS1 = 0, sensorValueS2 = 0, sensorValueCO2 = 0, veh_c1 = 0, veh_c2 = 0;


unsigned long tini, tactual, tdelta;
bool R1 = false, Y1 = false, V1 = false;
bool R2 = false, Y2 = false, V2 = false;

void setup() {
  Serial.begin(115200);
  config_display();
  pinModes();
  connection();  // WiFi y WebSocket
  //allOff();
}

void pinModes() {

  // Sensor CO2
  pinMode(CO2_SENSOR_PIN, INPUT);

  // Sensor visibilidad
  pinMode(SENSOR_PIN_S1, INPUT);
  pinMode(SENSOR_PIN_S2, INPUT);

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

void config_display() {
  lcd.init();            // Inicializa la pantalla
  lcd.backlight();       // Enciende luz de fondo
  lcd.setCursor(0, 0);
  lcd.print("Smart City Capibaras");
  lcd.clear();
}

void read_CO2_sensor() {
  sensorValueCO2 = analogRead(CO2_SENSOR_PIN);
  co2High = (sensorValueCO2 > 700);
}

void read_sensor_S1() {
  sensorValueS1 = analogRead(SENSOR_PIN_S1);
  vlS1 = (sensorValueS1 < 600);
}

void read_sensor_S2() {
  sensorValueS2 = analogRead(SENSOR_PIN_S2);
  vlS2 = read_visibility(sensorValueS2);
}

bool read_visibility(int sensorValue) {

  if (sensor_other_city != 0) {
    return (sensorValue < 600) || (sensor_other_city < 600);
  }

  return (sensorValue < 600);
}

void show_values_display() {
  lcd.setCursor(0, 0);
  lcd.print("CO2:");
  lcd.print(sensorValueCO2);
  lcd.setCursor(0, 1);
  lcd.print("V2:");
  lcd.print(sensorValueS2);
  lcd.print(" VR:");
  lcd.print(sensor_other_city);
}

void loop() {

  webSocket.loop();       // Procesar WebSocket
  sendSignalWebsocket();   // Enviar datos cada intervalo

  read_CO2_sensor();         // Mostrar CO₂ en LCD
  read_sensor_S1();           // Leer valor de los sensores semaforo 1
  read_sensor_S2();           // Leer valor de los sensores semaforo 2

  // Mostras los valores de los sensores
  show_values_display();

  // Modo normal
  checkState();
  updateLights();

  // bool VehicleLine1 = lane_in_1();
  //bool VehicleLine2 = lane_in_2();

  /*if (co2High || bad) {
    priorityLine2();
    }
    else if (VehicleLine1) {
    priorityLine2();
    }
    else if (VehicleLine2) {
    priorityLine1();
    }
    else {
    NoVehicleMode();
    }*/
}

void priorityLine1() {
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Paso: Carril #1 #V:");
  lcd.print(veh_c1);

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(RED_LED_PIN_2, HIGH);
    digitalWrite(GREEN_LED_PIN_1, HIGH);
  }
}

void priorityLine2() {
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Paso: Carril #2 #V:");
  lcd.print(veh_c2);

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(RED_LED_PIN_1, HIGH);
    digitalWrite(GREEN_LED_PIN_2, HIGH);
  }
}

void NoVehicleMode() {
  allOff();
  lcd.setCursor(0, 2);
  lcd.print("Modo sin vehiculos ");
  yellow_flashing_both();;
}

void checkState() {
  tactual = millis();
  unsigned long tdelta = tactual - tini;

  // Si hay una condición "mala", y el estado lo permite, transicionar a GREEN
  if (vlS1 && (state == RED || state == RED_TO_YELLOW || state == GREEN_TO_YELLOW)) {
    state = GREEN;
    tini = millis();
    return;
  }

  switch (state) {
    case RED:
      if (tdelta >= 3000) {
        changeState(RED_TO_YELLOW, "Estado 1: Rojo a Amarillo");
      }
      break;

    case RED_TO_YELLOW:
      if (tdelta >= 2000) {
        changeState(GREEN, "Estado 2: Verde");
      }
      break;

    case GREEN:
      if (vlS1) {
        tini = millis();
        break;
      }
      if (tdelta >= 6000 && !vlS1) {
        changeState(GREEN_TO_YELLOW, "Estado 3: Verde a Amarillo");
      }
      break;

    case GREEN_TO_YELLOW:
      if (tdelta >= 2000) {
        changeState(RED, "Estado 0: Rojo");
      }
      break;

    default:
      break;
  }
}

void changeState(int newState, const char* message) {
  state = newState;
  tini = millis();
  Serial.println(message);
}

void updateLights() {
  switch (state) {
    case RED:
      R1 = 0; Y1 = 0; V1 = 1; // Semáforo 1: Verde
      R2 = 1; Y2 = 0; V2 = 0; // Semáforo 2: Rojo
      break;

    case RED_TO_YELLOW:
      R1 = 0; Y1 = 1; V1 = 0;  // Semáforo 1: Amarillo
      R2 = 0; Y2 = 1; V2 = 0; // Semáforo 2: Amarillo
      break;

    case GREEN:
      R1 = 1; Y1 = 0; V1 = 0; // Semáforo 1: Rojo
      R2 = 0; Y2 = 0; V2 = 1; // Semáforo 2: Verde
      break;

    case GREEN_TO_YELLOW:
      R1 = 0; Y1 = 1; V1 = 0; // Semáforo 1: Amarillo
      R2 = 0; Y2 = 1; V2 = 0; // Semáforo 2: Amarillo
      break;
  }

  // Actualiza pines físicos
  digitalWrite(RED_LED_PIN_1, R1);
  digitalWrite(YELLOW_LED_PIN_1, Y1);
  digitalWrite(GREEN_LED_PIN_1, V1);

  digitalWrite(RED_LED_PIN_2, R2);
  digitalWrite(YELLOW_LED_PIN_2, Y2);
  digitalWrite(GREEN_LED_PIN_2, V2);
}

bool lane_in_1() {
  veh_c1 = 0;
  if (digitalRead(CNY4) == LOW) veh_c1++;
  if (digitalRead(CNY5) == LOW) veh_c1++;
  if (digitalRead(CNY6) == LOW) veh_c1++;
  return veh_c1 > 0;
}

bool lane_in_2() {
  veh_c2 = 0;
  if (digitalRead(CNY1) == LOW) veh_c2++;
  if (digitalRead(CNY2) == LOW) veh_c2++;
  if (digitalRead(CNY3) == LOW) veh_c2++;
  return veh_c2 > 0;
}

void allOff() {

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

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (YELLOW_LED_PIN_1_STATE == LOW) {
      YELLOW_LED_PIN_1_STATE = HIGH;
    } else {
      YELLOW_LED_PIN_1_STATE = LOW;
    }

    if (YELLOW_LED_PIN_2_STATE == LOW) {
      YELLOW_LED_PIN_2_STATE = HIGH;
    } else {
      YELLOW_LED_PIN_2_STATE = LOW;
    }

    digitalWrite(YELLOW_LED_PIN_1, YELLOW_LED_PIN_1_STATE);
    digitalWrite(YELLOW_LED_PIN_2, YELLOW_LED_PIN_2_STATE);
  }
}

void connection(){
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
  }

// Handle incoming WebSocket messages
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
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
  }

void sendSignalWebsocket() {
  webSocket.loop();

  unsigned long now = millis();
  if (now - previousMillis > 2000 && webSocket.isConnected()) {
    previousMillis = now;
    int sensorValue = analogRead(SENSOR_PIN_S2);

    // Optional: use JSON format
    StaticJsonDocument<100> doc;
    doc["sensor"] = sensorValue;
    doc["msg"] = sensorValue;
    doc["to"] = "city_2";
    String json;
    serializeJson(doc, json);

    webSocket.sendTXT(json);  // send sensor reading
    }
  }