/*
    This sketch shows the Ethernet event usage

*/

// Important to be defined BEFORE including ETH.h for ETH.begin() to work.
// Example RMII LAN8720 (Olimex, etc.)
#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_PHY_ADDR         0
#define ETH_PHY_MDC         23
#define ETH_PHY_MDIO        18
#define ETH_PHY_POWER       -1
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN
#endif

#include <Arduino.h>
#include <ETH.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <PubSubClient.h>

const char* ssid     = "romaska:з";
const char* password = "VEHI2019";
static bool eth_connected = false;


// Your MQTT broker ID
const char *mqttBroker = "178.91.129.235";
const int mqttPort = 1883;
const char *MQTT_TOKEN = "GRSXe2UPEFNvuMXDAAxS";
// MQTT topics
const char *publishTopic = "v1/devices/me/attributes";
const char *subscribeTopic = "v1/devices/me/telemetry";

WiFiClient espClient;
PubSubClient clientMQTT(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (5)
char msg[MSG_BUFFER_SIZE];

// Callback function whenever an MQTT message is received
void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++)
  {
    Serial.print(message += (char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if 'ON' was received
  if (message == "ON")
  {
    Serial.println("Turning ON Built In LED..");
    digitalWrite(2, HIGH);
  }
  else
  {
    Serial.println("Turning OFF Built In LED..");
    digitalWrite(2, LOW);
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!clientMQTT.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID

    // Attempt to connect
    if (clientMQTT.connect(MQTT_TOKEN))
    {
      Serial.println("connected");
      // Subscribe to topic
      clientMQTT.subscribe(subscribeTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(clientMQTT.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// WARNING: WiFiEvent is called from a separate FreeRTOS task (thread)!
void WiFiEvent(WiFiEvent_t event)
{
  int numberOfTries = 20;
  switch(WiFi.status()) {
          case WL_NO_SSID_AVAIL:
            Serial.println("[WiFi] SSID not found");
            break;
          case WL_CONNECT_FAILED:
            Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
            return;
            break;
          case WL_CONNECTION_LOST:
            Serial.println("[WiFi] Connection was lost");
            break;
          case WL_SCAN_COMPLETED:
            Serial.println("[WiFi] Scan is completed");
            break;
          case WL_DISCONNECTED:
            Serial.println("[WiFi] WiFi is disconnected");
            break;
          case WL_CONNECTED:
            Serial.println("[WiFi] WiFi is connected!");
            Serial.print("[WiFi] IP address: ");
            Serial.println(WiFi.localIP());
            return;
            break;
          default:
            Serial.print("[WiFi] WiFi Status: ");
            Serial.println(WiFi.status());
            break;
        }
        
        if(numberOfTries <= 0){
          Serial.print("[WiFi] Failed to connect to WiFi!");
          // Use disconnect function to force stop trying to connect
          WiFi.disconnect();
          return;
        } else {
          numberOfTries--;
        }
}

void ETHEvent(WiFiEvent_t event)
{
  int numberOfTries = 20;
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // The hostname must be set after the interface is started, but needs
      // to be set before DHCP, so set it from the event handler thread.
      ETH.setHostname("esp32-ethernet");
      WiFi.begin();
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP");
      Serial.println(ETH.localIP());
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void testClient(const char * host, uint16_t port)
{
  Serial.print("\nconnecting to ");
  Serial.println(host);

  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }
  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("closing connection\n");
  client.stop();
}

void setupMQTT(){
   // setup the mqtt server and callback
  clientMQTT.setServer(mqttBroker, mqttPort);
  clientMQTT.setCallback(callback);
  Serial.println("MQTT server sets");
}

void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  pinMode(5, INPUT_PULLUP); // Установите пин как вход с подтягивающим резистором
  int pinState = digitalRead(5); // Читаем состояние пина
  if (pinState == LOW) { // Если пин замкнут
    Serial.println("WIFI AP STARTED");
    WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.
    WiFi.begin(ssid, password);
    setupMQTT();
  } else { // Если пин разомкнут
    Serial.println("ETH STARTED");
    WiFi.onEvent(ETHEvent);  // Will call WiFiEvent() from another thread.
    ETH.begin();
    setupMQTT();
  }
}

void loop()
{
  if (!clientMQTT.connected())
  {
    reconnect();
  }
  clientMQTT.loop();
  unsigned long now = millis();
  if (now - lastMsg > 10000)
  {
    lastMsg = now;
    // Read the Hall Effect sensor value
    int hallEffectValue = hallRead();

    snprintf(msg, MSG_BUFFER_SIZE, "%d", hallEffectValue);
    Serial.print("Publish message: ");
    Serial.println(msg);
    clientMQTT.publish(publishTopic, msg);
  }
  delay(1000);
}