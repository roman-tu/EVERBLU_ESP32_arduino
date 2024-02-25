/*
Project: My Project
Author: Your Name
Date: February 24, 2024

Description:
This project is based on the github project of the user psykokwak-com (https://github.com/psykokwak-com/everblu-meters-esp8266.git ), 
written to collect data from the EVERBLU radio module via the CC1101 radio module (sorry for the tautology). And then sending the collected data via MQTT.
Flexible network settings allow you to set up data sending correctly.

Functions:
1. Reading data from sensors CC1101
2. Connect to WIFI or ETHERNET network
3. Send json with data on mqtt server

Usage:
To use this project, you will need an WT32-ETH01 or compatible board.
*/

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
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "everblu_meters.h"

//yours param network
#define STATIC_IP_ADDR "192.168.1.177"
#define GATEWAY_ADDR "192.168.1.1"
#define SUBNET_ADDR "255.255.255.0"
#define DNS_ADDR "8.8.8.8"

// buffer for json data
char buffer[512];

// wifi params
const char* ssid     = "romaska:з";
const char* password = "VEHI2019";
//identification ethernet
static bool eth_connected = false;


// Your MQTT broker ID
const char *mqttBroker = "iot.tocloud.kz";
const int mqttPort = 1883;
const char *MQTT_TOKEN = "GRSXe2UPEFNvuMXDAAxS";
// MQTT topics
const char *publishTopic = "v1/devices/me/attributes";
const char *subscribeTopic = "v1/devices/me/attributes";
const char *telemetryTopic = "v1/devices/me/telemetry";

// Создание объекта JSON
DynamicJsonDocument doc(1024);

WiFiClient espClient;
PubSubClient clientMQTT(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (5)
char msg[MSG_BUFFER_SIZE];

void blinkLed() {
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);   // включить светодиод
  delay(1000);                   // ждать секунду
  digitalWrite(15, HIGH);    // выключить светодиод
}

struct NetworkParams {
  int parts[4];
};

NetworkParams parseIP(const String& ip) {
  NetworkParams result;
  int start = 0;
  for (int i = 0; i < 4; i++) {
    int end = ip.indexOf('.', start);
    if (end == -1) {
      end = ip.length();
    }
    result.parts[i] = ip.substring(start, end).toInt();
    start = end + 1;
  }
  return result;
}
//split string "."
NetworkParams Staticip = parseIP(STATIC_IP_ADDR);
NetworkParams Gateway = parseIP(GATEWAY_ADDR);
NetworkParams Subnet = parseIP(SUBNET_ADDR);
NetworkParams Dns = parseIP(DNS_ADDR);

void setStaticParamNetwork(){
  // set yours net param
  IPAddress staticIP(Staticip.parts[0], Staticip.parts[1], Staticip.parts[2], Staticip.parts[3]); // Статический IP-адрес
  IPAddress gateway(Gateway.parts[0], Gateway.parts[1], Gateway.parts[2], Gateway.parts[3]);    // Шлюз
  IPAddress subnet(Subnet.parts[0], Subnet.parts[1], Subnet.parts[2], Subnet.parts[3]);   // Маска подсети
  IPAddress dns(Dns.parts[0], Dns.parts[1], Dns.parts[2], Dns.parts[3]);            // DNS-сервер
}

// void blinkLedWhileSearch() {
//   pinMode(LED_PIN, OUTPUT);
//   digitalWrite(LED_PIN, HIGH);   // включить светодиод
//   delay(30);                   // ждать секунду
//   digitalWrite(LED_PIN, LOW);    // выключить светодиод
//   delay(30);
//   digitalWrite(LED_PIN, HIGH);   // включить светодиод
//   delay(30);                   // ждать секунду
//   digitalWrite(LED_PIN, LOW);    // выключить светодиод
//   delay(50);
// }

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
    if (clientMQTT.connect(MQTT_TOKEN, MQTT_TOKEN, NULL))
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
  pinMode(15, OUTPUT);
  pinMode(5, INPUT_PULLUP); // Установите пин как вход с подтягивающим резистором
  setStaticParamNetwork();
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
  // digitalWrite(15, HIGH);
}

void loop()
{
  if (!clientMQTT.connected())
  {
    reconnect();
  }
  clientMQTT.loop();
  unsigned long now = millis();
  
  // for (float i = 433.76f; i < FREQUENCY; i += 0.0005f) {
  Serial.printf("----Test frequency\n");
  cc1101_init(EVBM21_473874);
      
  struct tmeter_data meter_data;
  meter_data = get_meter_data();
  
  if (meter_data.reads_counter != 0 || meter_data.liters != 0) {
    Serial.printf("\n------------------------------\nGot frequency\n------------------------------\n");
    
    Serial.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);
    doc["Liters"] = meter_data.liters;
    doc["Battery (in months)"] = meter_data.battery_left;
    doc["Counter"] = meter_data.reads_counter;
    blinkLed();
    // digitalWrite(15, HIGH); // turned on
    // while (42);
  }else{
    Serial.printf("Skip ------------\n");
  }
  // }
  if (now - lastMsg > 10000)
  {
    lastMsg = now;
    serializeJson(doc, buffer);
    // Read the Hall Effect sensor value
    int hallEffectValue = hallRead();
    clientMQTT.publish(telemetryTopic, buffer);
    snprintf(msg, MSG_BUFFER_SIZE, "%d", hallEffectValue);
    Serial.print("Publish message: ");
    Serial.println(msg);
    clientMQTT.publish(publishTopic, msg);
  }
  delay(1000);
}