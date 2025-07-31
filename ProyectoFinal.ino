#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>

// Pinout
#define LR1 5
#define LY1 4
#define LG1 6

#define LR2 7
#define LY2 15
#define LG2 16

#define SENSOR_PIN 12

const char *ssid = "AirportAgent01";
const char *password = "15427605";

// Initial Value of External sensor
int externalSensor = -1;

// Websocket value reference and interval to send data through websocket
unsigned long lastSentInterval = 0;
const unsigned long interval = 2000;
WebSocketsClient webSocket;

bool R1 = 0;
bool Y1 = 0;
bool G1 = 0;

bool R2 = 0;
bool Y2 = 0;
bool G2 = 0;
int state = 0;

long unsigned tini, tactual, tdelta;
bool badVisibility;

void measure()
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

void control()
{
  switch (state)
  {
  // RED
  case 0:
    R1 = 0;
    Y1 = 0;
    G1 = 1;

    R2 = 1;
    Y2 = 0;
    G2 = 0;
    if (badVisibility)
    {
      state = 2;
      tini = millis();
      Serial.println("Sensor covered");
    }
    else if (tdelta >= 2000)
    {
      state = 1;
      tini = millis();
      Serial.println("Moving to yellow");
    }
    break;

  // YELLOW
  case 1:
    R1 = 0;
    Y1 = 1;
    G1 = 0;

    R2 = 0;
    Y2 = 1;
    G2 = 0;
    if (badVisibility)
    {
      state = 2;
      tini = millis();
      Serial.println("Sensor covered");
    }
    else if (tdelta >= 1000)
    {
      state = 2;
      tini = millis();
      Serial.println("Moving to 2");
    }
    break;

  // GREEN
  case 2:
    R1 = 1;
    Y1 = 0;
    G1 = 0;

    R2 = 0;
    Y2 = 0;
    G2 = 1;
    if (badVisibility)
    {
      tini = millis();
      Serial.println("Sensor covered");
    }
    else if (tdelta >= 2000 && !badVisibility)
    {
      state = 3;
      tini = millis();
      Serial.println("Moving to yellow");
    }
    break;

  // YELLOW
  case 3:
    R1 = 0;
    Y1 = 1;
    G1 = 0;

    R2 = 0;
    Y2 = 1;
    G2 = 0;
    if (badVisibility)
    {
      state = 2;
      tini = millis();
      Serial.println("Sensor covered");
    }
    else if (tdelta >= 1000)
    {
      state = 0;
      tini = millis();
      Serial.println("Moving to 0");
    }
    break;

  default:
    break;
  }
}

void act()
{
  digitalWrite(LR1, R1);
  digitalWrite(LY1, Y1);
  digitalWrite(LG1, G1);

  digitalWrite(LR2, R2);
  digitalWrite(LY2, Y2);
  digitalWrite(LG2, G2);
}

// Handle incoming WebSocket messages
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    Serial.println("Connected to WebSocket ser2");
    break;
  case WStype_DISCONNECTED:
    Serial.println("Disconnected from WebSocket ser2");
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
  pinMode(SENSOR_PIN, INPUT);

  pinMode(LR1, OUTPUT);
  pinMode(LY1, OUTPUT);
  pinMode(LG1, OUTPUT);

  pinMode(LR2, OUTPUT);
  pinMode(LY2, OUTPUT);
  pinMode(LG2, OUTPUT);

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

  act();
}

void loop()
{
  sendSignalWebsocket();
  measure();
  control();
  act();
}