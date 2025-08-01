#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pines semáforo 1
#define RED_LED_PIN_1 5
#define YELLOW_LED_PIN_1 4
#define GREEN_LED_PIN_1 6

// Pines semáforo 2
#define RED_LED_PIN_2 7
#define YELLOW_LED_PIN_2 15
#define GREEN_LED_PIN_2 16

// Sensores infrarrojos carril 1 (CNY4–CNY6)
#define CNY4 39
#define CNY5 38
#define CNY6 37

// Sensores infrarrojos carril 2 (CNY1–CNY3)
#define CNY1 42
#define CNY2 41
#define CNY3 40

// Sensores ambientales
#define SENSOR_PIN_S1 13   // Visibilidad
#define SENSOR_PIN_S2 12   // Visibilidad
#define CO2_SENSOR_PIN 14  // CO₂

// Botones semaforos
#define BTN_PEATONAL_S1 1  // Traffic light 1 button connected in pin 1
#define BTN_PEATONAL_S2 2  // Traffic light 2 button connected in pin 2

// Estados del semáforo
#define RED 0
#define RED_TO_YELLOW 1
#define GREEN 2
#define GREEN_TO_YELLOW 3
#define PEATONAL 4

// Variables que cambian:
int YELLOW_LED_PIN_1_STATE = LOW;
int YELLOW_LED_PIN_2_STATE = LOW;

const char *ssid = "iPhone de Jean";
const char *password = "12345679";

WebSocketsClient webSocket;

unsigned long previousMillisPriority1 = 0;
unsigned long previousMillisPriority2 = 0;
unsigned long previousMillisFlashing = 0;
unsigned long previousMillisSocket = 0;

const unsigned long interval = 1000;

int state = 0;
bool vlS1 = false, vlS2 = false;
bool co2High = false;
int sensor_other_city = 0, sensorValueS1 = 0, sensorValueS2 = 0, sensorValueCO2 = 0, mode = 0;

unsigned long tini, tactual, tdelta;
bool R1 = false, Y1 = false, V1 = false;
bool R2 = false, Y2 = false, V2 = false;

volatile bool peatonalRequested = false;
unsigned long peatonalStartedAt = 0;
unsigned long peatonalRequestTime = 0;
bool peatonalReady = false;

bool lastModeBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

enum Mode {
  MODE_DAY,
  MODE_NIGHT
};

Mode currentMode = MODE_DAY;  // Empieza por defecto en modo día

void setup() {
  Serial.begin(115200);
  config_display();
  pinModes();
  connection();  // WiFi y WebSocket
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

  // Pasos peatonales
  pinMode(BTN_PEATONAL_S1, INPUT_PULLUP);
  pinMode(BTN_PEATONAL_S2, INPUT_PULLUP);
}

void config_display() {
  lcd.init();       // Inicializa la pantalla
  lcd.backlight();  // Enciende luz de fondo
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
  vlS1 = read_visibility(sensorValueS1);
}

void read_sensor_S2() {
  sensorValueS2 = analogRead(SENSOR_PIN_S2);
  vlS2 = (sensorValueS2 < 600);
  ;
}

bool read_visibility(int sensorValue) {

  if (sensor_other_city != 0) {
    return (sensorValue < 600) || (sensor_other_city < 600);
  }

  return (sensorValue < 600);
}

void show_values_display() {
  lcd.setCursor(0, 0);
  lcd.print("Modo: ");
  lcd.print(currentMode == MODE_DAY ? "DIA" : "NOCHE");
  lcd.print(" CO2:");
  lcd.print(sensorValueCO2);
  lcd.setCursor(0, 1);
  lcd.print("VS2:");
  lcd.print(sensorValueS2);
  lcd.print(" VR:");
  lcd.print(sensor_other_city);
}

void loop() {

  webSocket.loop();       // Procesar WebSocket
  sendSignalWebsocket();  // Enviar datos cada intervalo

  read_CO2_sensor();  // Mostrar CO₂ en LCD
  read_sensor_S1();   // Mostrar valor de los sensores semaforo 1
  read_sensor_S2();   // Mostrar valor de los sensores semaforo 2
  read_lane_in_1();   // Mostrar valor de los cantidad de vehiculos en el carril 1
  read_lane_in_2();   // Mostrar valor de los cantidad de vehiculos en el carril 2

  // Mostras los valores de los sensores
  show_values_display();

  checkModeSwitch();

  if (currentMode == MODE_DAY)
  {
  mode_day();
  }
  else
  {
    mode_night(); // Aquí colocas tu lógica de detección/prioridad
  }
}

void checkModeSwitch() {
  int reading = mode;
  if (reading != lastModeBtnState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == HIGH) {
      currentMode = (currentMode == MODE_DAY) ? MODE_NIGHT : MODE_DAY;
      Serial.print("Cambiado a modo: ");
      Serial.println(currentMode == MODE_DAY ? "DIA" : "NOCHE");
    }
  }

  lastModeBtnState = reading;
}

void mode_day() {

  // Paso peatonal
  pedestrian_crossing();

  checkState();
  updateLights();
}

void mode_night() {

  if (co2High || vlS1 || vlS2) {
    priorityLine2();
  } else if (read_lane_in_1() > 0) {
    priorityLine2();
  } else if (read_lane_in_2() > 0) {
    priorityLine1();
  } else {
    NoVehicleMode();
  }
}

void priorityLine1() {

  digitalWrite(YELLOW_LED_PIN_1, LOW);
  digitalWrite(YELLOW_LED_PIN_2, LOW);

  lcd.setCursor(0, 2);
  lcd.print("Paso: Carril #1 #V:");
  lcd.print(read_lane_in_1());

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisPriority1 >= interval) {
    previousMillisPriority1 = currentMillis;
    digitalWrite(RED_LED_PIN_2, HIGH);
    digitalWrite(GREEN_LED_PIN_1, HIGH);
  }
}

void priorityLine2() {

  digitalWrite(YELLOW_LED_PIN_1, LOW);
  digitalWrite(YELLOW_LED_PIN_2, LOW);

  if (read_lane_in_2() > 0) {
    lcd.setCursor(0, 2);
    lcd.print("Paso: Carril #2 #V:");
    lcd.print(read_lane_in_2());
  }

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisPriority2 >= interval) {
    previousMillisPriority2 = currentMillis;
    digitalWrite(RED_LED_PIN_1, HIGH);
    digitalWrite(GREEN_LED_PIN_2, HIGH);
  }
}

void NoVehicleMode() {

  digitalWrite(RED_LED_PIN_1, LOW);
  digitalWrite(GREEN_LED_PIN_1, LOW);
  digitalWrite(RED_LED_PIN_2, LOW);
  digitalWrite(GREEN_LED_PIN_2, LOW);

  yellow_flashing_both();

  lcd.setCursor(0, 2);
  lcd.print("Modo sin vehiculos    ");
}

void checkState() {
  tactual = millis();
  unsigned long tdelta = tactual - tini;

  lcd.setCursor(0, 2);
  lcd.print("Modo ");
  lcd.print((co2High || vlS1 || vlS2) ? "EMERGENCIA    " : "NORMAL     ");

  // Si se solicitó paso peatonal y no hemos cambiado aún
  if (peatonalRequested && !peatonalReady) {
    if (millis() - peatonalRequestTime >= 3000) {
      peatonalReady = true;
    }
  }

  // Se activa el paso peatonal
  if (peatonalReady && state != PEATONAL) {
    changeState(PEATONAL, "Activando paso peatonal con retraso");
    peatonalRequested = false;
    peatonalReady = false;
    return;
  }

  // Si hay una condición "mala" o el CO2 es alto, y el estado lo permite, transicionar a GREEN
  if ((co2High || vlS1 || vlS2) && (state == RED || state == RED_TO_YELLOW || state == GREEN_TO_YELLOW)) {
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
      if (vlS1 || vlS2) {
        tini = millis();
        break;
      }

      if (tdelta >= 6000 && !vlS1 && !vlS2) {
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

void changeState(int newState, const char *message) {
  state = newState;
  tini = millis();

  if (newState == PEATONAL) {
    peatonalStartedAt = millis();
    Serial.println("Paso peatonal activado");
  }
  Serial.println(message);
}

void updateLights() {
  switch (state) {
    case RED:
      R1 = 0;
      Y1 = 0;
      V1 = 1;  // Semáforo 1: Verde
      R2 = 1;
      Y2 = 0;
      V2 = 0;  // Semáforo 2: Rojo
      break;

    case RED_TO_YELLOW:
      R1 = 0;
      Y1 = 1;
      V1 = 0;
      R2 = 1;
      Y2 = 0;
      V2 = 0;
      break;

    case GREEN:
      R1 = 1;
      Y1 = 0;
      V1 = 0;
      R2 = 0;
      Y2 = 0;
      V2 = 1;
      break;

    case GREEN_TO_YELLOW:
      R1 = 1;
      Y1 = 0;
      V1 = 0;
      R2 = 0;
      Y2 = 1;
      V2 = 0;
      break;

    case PEATONAL:
      R1 = 1;
      Y1 = 0;
      V1 = 0;  // Ambos en rojo
      R2 = 1;
      Y2 = 0;
      V2 = 0;

      if (millis() - peatonalStartedAt >= 5000) {
        peatonalStartedAt = 0;
        changeState(GREEN_TO_YELLOW, "Fin paso peatonal");
      }
      break;
  }

  // Actualiza pines físicos normales
  digitalWrite(RED_LED_PIN_1, R1);
  digitalWrite(YELLOW_LED_PIN_1, Y1);
  digitalWrite(GREEN_LED_PIN_1, V1);

  digitalWrite(RED_LED_PIN_2, R2);
  digitalWrite(YELLOW_LED_PIN_2, Y2);
  digitalWrite(GREEN_LED_PIN_2, V2);
}

int read_lane_in_1() {
  int count = 0;
  if (digitalRead(CNY4) == LOW) count++;
  if (digitalRead(CNY5) == LOW) count++;
  if (digitalRead(CNY6) == LOW) count++;
  return count;
}

int read_lane_in_2() {
  int count = 0;
  if (digitalRead(CNY1) == LOW) count++;
  if (digitalRead(CNY2) == LOW) count++;
  if (digitalRead(CNY3) == LOW) count++;
  return count;
}

void pedestrian_crossing() {
  if ((digitalRead(BTN_PEATONAL_S1) == HIGH || digitalRead(BTN_PEATONAL_S2) == HIGH) && !peatonalRequested) {
    peatonalRequested = true;
    peatonalRequestTime = millis();  // Marca el momento de la solicitud
    Serial.println("Solicitud de paso peatonal recibida desde alguno de los semáforos.");
  }
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

  if (currentMillis - previousMillisFlashing >= interval) {
    // save the last time you blinked the LED
    previousMillisFlashing = currentMillis;

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

void connection() {
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
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
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
        const char *msg = doc["msg"];
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
  if (now - previousMillisSocket > 2000 && webSocket.isConnected()) {
    previousMillisSocket = now;
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
