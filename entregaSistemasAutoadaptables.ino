#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

#define LR1 7   // Red traffic light 1 connected in pin 7
#define LA1 15  // Yellow traffic light 1 connected in pin 15
#define LV1 16  // Green traffic light 1 connected in pin 16
#define LED_PIN 5

#define ROJO 0
#define AMAR2VER 1
#define VER 2
#define AMAR2ROJ 3

#define SENSOR_PIN 12  // Local visibility sensor
int sensor_sierra = -1;

const char* ssid = "AirportAgent01";
const char* password = "15427605";

unsigned long lastSent = 0;
const unsigned long interval = 2000;

WebSocketsClient webSocket;


bool R = 0;
bool A = 0;
bool V = 0;
int estado = 0;
long unsigned tini, tactual, tdelta;
bool bad;

void medir() {
  int sensorValue = analogRead(SENSOR_PIN);
  tactual = millis();
  tdelta = tactual - tini;
  bad = read_visibility();
}

bool read_visibility() {
  int local_visibility = analogRead(SENSOR_PIN);

  if (sensor_sierra != -1) {
    return (local_visibility < 600) || (sensor_sierra < 600);
  }

  return (local_visibility < 600);
}

void controlar() {
  switch (estado) {
    case ROJO:
      if (bad) {
        estado = VER;
        tini = millis();
        break;
      }

      R = 1;
      A = 0;
      V = 0;

      if (tdelta >= 3000) {
        estado = AMAR2VER;
        Serial.println("Estado 1: Amarillo a Verde");
        tini = millis();
      }
      break;

    case AMAR2VER:
      if (bad) {
        estado = VER;
        tini = millis();
        break;
      }

      R = 0;
      A = 1;
      V = 0;

      if (tdelta >= 2000) {
        estado = VER;
        Serial.println("Estado 2: Verde");
        tini = millis();
      }
      break;

    case VER:
      R = 0;
      A = 0;
      V = 1;

      if (bad) {
        tini = millis();
        break;
      }

      if (tdelta >= 6000) {
        estado = AMAR2ROJ;
        Serial.println("Estado 3: Amarillo a Rojo");
        tini = millis();
      }
      break;

    case AMAR2ROJ:
      if (bad) {
        estado = VER;
        tini = millis();
        break;
      }

      R = 0;
      A = 1;
      V = 0;

      if (tdelta >= 2000) {
        estado = ROJO;
        Serial.println("Estado 0: Rojo");
        tini = millis();
      }
      break;

    default:
      break;
  }
}

void actuar() {
  digitalWrite(LR1, R);
  digitalWrite(LA1, A);
  digitalWrite(LV1, V);
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
        sensor_sierra = int(doc["sensor"]);

        Serial.println(sensor_sierra);

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
  if (now - lastSent > interval && webSocket.isConnected()) {
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
    // Serial.println("Sent: " + json);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  pinMode(LR1, OUTPUT);
  pinMode(LA1, OUTPUT);
  pinMode(LV1, OUTPUT);

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


  actuar();
}

void loop() {
  sendSignalWebsocket();

  medir();
  controlar();
  actuar();
}
