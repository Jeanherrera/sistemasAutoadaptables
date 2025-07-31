#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

// Pinout
#define LR1 7
#define LA1 15
#define LV1 16
#define LED_PIN 5

#define RED 0
#define YELLOW2GREEN 1
#define GREEN 2
#define YELLOW2RED 3

#define SENSOR_PIN 12

const char *ssid = "AirportAgent01";
const char *password = "15427605";

// Initial Value of External sensor
int externalSensor = -1;

// Websocket value reference and interval to send data through websocket
unsigned long lastSentInterval = 0;
const unsigned long interval = 2000;

WebSocketsClient webSocket;

bool R = 0;
bool A = 0;
bool V = 0;
int state = 0;

long unsigned tini, tactual, tdelta;
bool badVisibility;

void meassure()
{
  int sensorValue = analogRead(SENSOR_PIN);
  tactual = millis();
  tdelta = tactual - tini;
  badVisibility = getBadVisibility();
}

bool getBadVisibility()
{
  int localSensor = analogRead(SENSOR_PIN);
  if (externalSensor != -1)
    return (localSensor < 600) || (externalSensor < 600);
  return (localSensor < 600);
}

void controlar()
{
  switch (state)
  {
  case RED:
    if (badVisibility)
    {
      state = GREEN;
      tini = millis();
      break;
    }
    R = 1;
    A = 0;
    V = 0;
    if (tdelta >= 3000)
    {
      state = YELLOW2GREEN;
      tini = millis();
    }
    break;

  case YELLOW2GREEN:
    if (badVisibility)
    {
      state = GREEN;
      tini = millis();
      break;
    }
    R = 0;
    A = 1;
    V = 0;
    if (tdelta >= 2000)
    {
      state = GREEN;
      tini = millis();
    }
    break;

  case GREEN:
    R = 0;
    A = 0;
    V = 1;
    if (badVisibility)
    {
      tini = millis();
      break;
    }
    if (tdelta >= 6000 && !badVisibility)
    {
      state = YELLOW2RED;
      tini = millis();
    }
    break;

  case YELLOW2RED:
    if (badVisibility)
    {
      state = GREEN;
      tini = millis();
      break;
    }
    R = 0;
    A = 1;
    V = 0;
    if (tdelta >= 2000)
    {
      state = RED;
      tini = millis();
    }
    break;

  default:
    break;
  }
}

void actuar()
{
  digitalWrite(LR1, R);
  digitalWrite(LA1, A);
  digitalWrite(LV1, V);
}

// Handle incoming WebSocket messages
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.println("Connected to WebSocket serGREEN");
    break;
  case WStype_DISCONNECTED:
    Serial.println("Disconnected from WebSocket serGREEN");
    break;
  case WStype_TEXT:
  {
    Serial.printf("Received: %s\n", payload);
    // Parse the incoming JSON message
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      break;
    }
    const char *msg = doc["msg"];
    externalSensor = int(doc["sensor"]);

    Serial.println(externalSensor);

    Serial.print("msg: ");
    Serial.println(msg);
  }
  break;
  default:
    break;
  }
}

void sendSignalWebsocket()
{
  webSocket.loop();
  unsigned long now = millis();
  if (now - lastSentInterval > interval && webSocket.isConnected())
  {
    lastSentInterval = now;
    int sensorValue = analogRead(SENSOR_PIN);

    // Optional: use JSON format
    StaticJsonDocument<100> doc;
    doc["sensor"] = sensorValue;
    doc["msg"] = sensorValue;
    doc["to"] = "city_2";
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json); // send sensor reading
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  pinMode(LR1, OUTPUT);
  pinMode(LA1, OUTPUT);
  pinMode(LV1, OUTPUT);

  // Wifi connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");

  // Setup secure WebSocket client (wss)
  webSocket.beginSSL("ws.davinsony.com", 443, "/city_capibaras");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  actuar();
}

void loop()
{
  sendSignalWebsocket();
  meassure();
  controlar();
  actuar();
}