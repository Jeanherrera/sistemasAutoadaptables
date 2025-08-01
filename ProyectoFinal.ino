#include <WiFi.h>
#include <WebSocketsClient_Generic.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <math.h> //Mathematics library for pow function (CO2 computation)

// Pinout
#define LR1 5
#define LY1 4
#define LG1 6

#define LR2 7
#define LY2 15
#define LG2 16

// Rail 1 vehicles Sensors
#define CNY1 42
#define CNY2 41
#define CNY3 40
// Rail 2 vehicles Sensors
#define CNY4 39
#define CNY5 38
#define CNY6 37
// Vehicle count
int VC1, VC2;

// Buttons
#define B1 1
#define B2 2
bool button1, button2;

// Light Dependent Resistors (LDRs)
#define LDR1 13
#define LDR2 12
#define UMBRAL_LDR 2000
bool lowLightR1, lowLightR2;

// CO₂ Sensor
#define CO2_SENSOR_PIN 14
#define UMBRAL_CO2 1000
bool highCO2;
int CO2ppm = 0;

const char *ssid = "AirportAgent01";
const char *password = "15427605";

// Websocket value reference and interval to send data through websocket
WebSocketsClient webSocket;
unsigned long lastSentInterval = 0;
const unsigned long interval = 2000;

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4);

bool R1 = 0;
bool Y1 = 0;
bool G1 = 0;

bool R2 = 0;
bool Y2 = 0;
bool G2 = 0;
int state = 0;

long unsigned tini, tactual, tdelta;

void measure()
{
  lowLightR1 = analogRead(LDR1) < UMBRAL_LDR;
  lowLightR2 = analogRead(LDR2) < UMBRAL_LDR;

  CO2ppm = analogRead(CO2_SENSOR_PIN);
  highCO2 = CO2ppm > UMBRAL_CO2;

  button1 = digitalRead(B1);
  button2 = digitalRead(B2);
  VC1 = !digitalRead(CNY1) + !digitalRead(CNY2) + !digitalRead(CNY3);
  VC2 = !digitalRead(CNY4) + !digitalRead(CNY5) + !digitalRead(CNY6);
  tactual = millis();
  tdelta = tactual - tini;
  printSensors();
}

void setTrafficLight1(bool r1, bool y1, bool g1)
{
  R1 = r1;
  Y1 = y1;
  G1 = g1;
}

void setTrafficLight2(bool r2, bool y2, bool g2)
{
  R2 = r2;
  Y2 = y2;
  G2 = g2;
}

void setState(int newState)
{
  state = newState;
  tini = millis();
}

void setYellowFlashing()
{
  static unsigned long lcdLastTime = 0;

  bool flashing = (millis() / 500) % 2; // Toggle every 500ms
  if (flashing)
  {
    setTrafficLight1(0, 1, 0);
    setTrafficLight2(0, 1, 0);
  }
  else
  {
    setTrafficLight1(0, 0, 0);
    setTrafficLight2(0, 0, 0);
  }
}

void control()
{
  switch (state)
  {
  // RED
  case 0:
    setTrafficLight1(0, 0, 1);
    setTrafficLight2(1, 0, 0);
    if (lowLightR2)
      setState(2);
    else if (tdelta >= 1000)
      setState(1);
    break;

  // YELLOW
  case 1:
    setTrafficLight1(0, 1, 0);
    setTrafficLight2(0, 1, 0);
    if (lowLightR2)
      setState(2);
    else if (tdelta >= 500)
      setState(2);
    break;

  // GREEN
  case 2:
    setTrafficLight1(1, 0, 0);
    setTrafficLight2(0, 0, 1);
    if (lowLightR2)
      tini = millis();
    else if (tdelta >= 1000 && !lowLightR2)
      setState(3);
    break;

  // YELLOW
  case 3:
    setTrafficLight1(0, 1, 0);
    setTrafficLight2(0, 1, 0);
    if (lowLightR2)
      setState(2);
    else if (tdelta >= 500)
      setState(0);
    break;

  // YELLOW FLASHING
  case 4:
    setYellowFlashing();
    if (!lowLightR2 && !lowLightR1)
    {
      state = 0;
      tini = millis();
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

// // Handle incoming WebSocket messages
// void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
// {
//   switch (type)
//   {
//   case WStype_CONNECTED:
//     Serial.println("Connected to WebSocket ser2");
//     break;
//   case WStype_DISCONNECTED:
//     Serial.println("Disconnected from WebSocket ser2");
//     break;
//   case WStype_TEXT:
//   {
//     Serial.printf("Received: %s\n", payload);
//     // Parse the incoming JSON message
//     StaticJsonDocument<200> doc;
//     DeserializationError error = deserializeJson(doc, payload);
//     if (error)
//     {
//       Serial.print("JSON parsing failed: ");
//       Serial.println(error.c_str());
//       break;
//     }
//     const char *msg = doc["msg"];
//     externalSensor = int(doc["msg"]);
//   }
//   break;
//   default:
//     break;
//   }
// }

// void sendSignalWebsocket()
// {
//   webSocket.loop();
//   unsigned long now = millis();
//   if (now - lastSentInterval > interval && webSocket.isConnected())
//   {
//     lastSentInterval = now;
//     int sensorValue = analogRead(LDR2);

//     // Optional: use JSON format
//     StaticJsonDocument<100> doc;
//     doc["sensor"] = sensorValue;
//     doc["msg"] = sensorValue;
//     doc["to"] = "city_2";
//     String json;
//     serializeJson(doc, json);
//     webSocket.sendTXT(json); // send sensor reading
//   }
// }

void setup()
{
  Serial.begin(115200);
  pinMode(LDR1, INPUT);
  pinMode(LDR2, INPUT);
  pinMode(CO2_SENSOR_PIN, INPUT);

  pinMode(CNY1, INPUT);
  pinMode(CNY2, INPUT);
  pinMode(CNY3, INPUT);
  pinMode(CNY4, INPUT);
  pinMode(CNY5, INPUT);
  pinMode(CNY6, INPUT);

  pinMode(B1, INPUT_PULLUP);
  pinMode(B2, INPUT_PULLUP);

  pinMode(LR1, OUTPUT);
  pinMode(LY1, OUTPUT);
  pinMode(LG1, OUTPUT);

  pinMode(LR2, OUTPUT);
  pinMode(LY2, OUTPUT);
  pinMode(LG2, OUTPUT);

  lcd.init();
  lcd.backlight();

  // // Wifi connection
  // WiFi.begin(ssid, password);
  // Serial.print("Connecting to WiFi");
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   Serial.print(".");
  //   delay(1000);
  // }
  // Serial.println("\nConnected to WiFi");

  // // Setup secure WebSocket client (wss)
  // webSocket.beginSSL("ws.davinsony.com", 443, "/city_capibaras");
  // webSocket.onEvent(webSocketEvent);
  // webSocket.setReconnectInterval(5000);

  // Initialize the display
  act();
}

void printSensors()
{
  static unsigned long lcdLastTime = 0;
  if (millis() - lcdLastTime < 250)
    return;

  lcdLastTime = millis();

  int s1 = analogRead(LDR1);
  int s2 = analogRead(LDR2);

  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print(s1);
  lcd.print(" L:");
  lcd.print(lowLightR1 ? "Y" : "N");

  lcd.setCursor(0, 1);
  lcd.print("S2:");
  lcd.print(s2);
  lcd.print(" L:");
  lcd.print(lowLightR2 ? "Y" : "N");

  lcd.setCursor(0, 2);
  lcd.print("CO2:");
  lcd.print(CO2ppm);
  lcd.print(" H:");
  lcd.print(highCO2 ? "Y" : "N");

  lcd.setCursor(0, 3);
  lcd.print("B1:");
  lcd.print(button1);
  lcd.print(" C1:");
  lcd.print(VC1);
  lcd.print(" B2:");
  lcd.print(button2);
  lcd.print(" C2:");
  lcd.print(VC2);
}

void loop()
{
  // sendSignalWebsocket();
  measure();
  control();
  act();
}