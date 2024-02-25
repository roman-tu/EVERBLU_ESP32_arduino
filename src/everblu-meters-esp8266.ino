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
#include <cc1101.h>

#define STATIC_NET_ON

  //yours static param network
#ifdef STATIC_NET_ON
  #define STATIC_IP_ADDR  "192.168.1.111"
  #define GATEWAY_ADDR  "192.168.1.1"
  #define SUBNET_ADDR  "255.255.255.0"
  #define DNS_ADDR  "0.0.0.0"
#endif

// buffer for json data
char buffer[1080];

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



WiFiClient espClient;
PubSubClient clientMQTT(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (5)
char msg[MSG_BUFFER_SIZE];

#define SwitchP 5
#define LED1 15
#define LED2 2

struct NetworkParams {
  uint8_t parts[4];
};

NetworkParams parseIP(const String& ip) {
  NetworkParams result;
  int start = 0;
  for (int i = 0; i < 4; i++) {
    int end = ip.indexOf('.', start);
    if (end == -1) {
      end = ip.length();
    }
    result.parts[i] = static_cast<uint8_t>(ip.substring(start, end).toInt());
    start = end + 1;
  }
  return result;
}

void blinkLed() {
  digitalWrite(LED1, LOW);   
  delay(1000);                 
  digitalWrite(LED1, HIGH);   
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
    digitalWrite(LED2, HIGH);
  }
  else
  {
    Serial.println("Turning OFF Built In LED..");
    digitalWrite(LED2, LOW);
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
            Serial.print("[WiFi] Got IP ");
            Serial.println(ETH.localIP());
            Serial.print("[WiFi] Subnet Mask ");
            Serial.println(WiFi.subnetMask());
            Serial.print("[WiFi] Gateway IP ");
            Serial.println(WiFi.gatewayIP());
            Serial.print("[WiFi] Dns IP ");
            Serial.println(WiFi.dnsIP());
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
      Serial.print("ETH Got IP ");
      Serial.println(ETH.localIP());
      Serial.print("ETH Subnet Mask ");
      Serial.println(ETH.subnetMask());
      Serial.print("ETH Gateway IP ");
      Serial.println(ETH.gatewayIP());
      Serial.print("ETH Dns IP ");
      Serial.println(ETH.dnsIP());
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

void setupMQTT(){
   // setup the mqtt server and callback
  clientMQTT.setServer(mqttBroker, mqttPort);
  clientMQTT.setCallback(callback);
  Serial.println("MQTT server sets");
}



void setup()
{
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(SwitchP, INPUT_PULLUP); // Установите пин как вход с подтягивающим резистором

  //split string "."
  NetworkParams Staticip = parseIP(STATIC_IP_ADDR);
  NetworkParams Gateway = parseIP(GATEWAY_ADDR);
  NetworkParams Subnet = parseIP(SUBNET_ADDR);
  NetworkParams Dns = parseIP(DNS_ADDR);

  // // Serial.println(Staticip.parts[0]);
  
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

    IPAddress staticiP(Staticip.parts[0], Staticip.parts[1], Staticip.parts[2], Staticip.parts[3]); // Статический IP-адрес
    IPAddress gateway(Gateway.parts[0], Gateway.parts[1], Gateway.parts[2], Gateway.parts[3]);    // Шлюз
    IPAddress subnet(Subnet.parts[0], Subnet.parts[1], Subnet.parts[2], Subnet.parts[3]);   // Маска подсети
    IPAddress dns(Dns.parts[0], Dns.parts[1], Dns.parts[2], Dns.parts[3]);            // DNS-сервер

    bool success = ETH.config(staticiP, gateway, subnet, dns);
    if (success) {
      // Конфигурация прошла успешно
      Serial.println("Change static param Success");
    } else {
      // Конфигурация не удалась
      Serial.println("Change static param ERROR");
    }
    setupMQTT();
  }
  // digitalWrite(15, HIGH);
}

void loop()
{
  DynamicJsonDocument doc(1024);
  DynamicJsonDocument cc1101_1(1024);
  cc1101_1["id"] = 1;
  cc1101_1["frequency"] = EVBM21_473874;
  cc1101_1["year"] = 20;
  cc1101_1["serialNumber"] = 123;

  DynamicJsonDocument cc1101_2(1024);
  cc1101_2["id"] = 2;
  cc1101_2["frequency"] = EVBM21_473874;
  cc1101_2["year"] = 21;
  cc1101_2["serialNumber"] = 473874;

  if (!clientMQTT.connected())
  {
    reconnect();
  }
  clientMQTT.loop();

  printData(cc1101_2.as<JsonObject>(), doc.as<JsonObject>());
  printData(cc1101_1.as<JsonObject>(), doc.as<JsonObject>());

  int hallEffectValue = hallRead();
  snprintf(msg, MSG_BUFFER_SIZE, "%d", hallEffectValue);
  clientMQTT.publish(publishTopic, msg);
  delay(100);
}

void printData(const JsonObject& data, const JsonObject& buff) {
  DynamicJsonDocument doc(1024);
  Serial.printf("----Testing frequency\n");
  Serial.println((int)data["id"]);
  Serial.print("Год: ");
  Serial.println((int)data["year"]);
  Serial.print("Серийный номер: ");
  Serial.println((int)data["serialNumber"]);
  Serial.print("Частота: ");
  Serial.println((float)data["frequency"]);

  cc1101_init((float)data["frequency"]);
  unsigned long now = millis();

  String Liters = "Liters" + String((int)data["id"]);

  String Battery = "Battery (in months)" + String((int)data["id"]);

  String Counter = "Counter" + String((int)data["id"]);

  struct tmeter_data meter_data;

  meter_data = get_meter_data((int)data["year"], (int)data["serialNumber"]);
  
  if (meter_data.reads_counter != 0 || meter_data.liters != 0) {
    Serial.printf("\n------------------------------\nGot data\n------------------------------\n");
    Serial.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);
    doc[Liters] = meter_data.liters;
    doc[Battery] = meter_data.battery_left;
    doc[Counter] = meter_data.reads_counter;
    blinkLed();
  }else{
    Serial.printf("Skip ------------\n");
    // doc[Liters] = 5345;
    // doc[Battery] = 34634563;
    // doc[Counter] = 777;
  }
  serializeJson(doc, buffer);
  // Read the Hall Effect sensor value
  Serial.print("Telemetry message: ");
  clientMQTT.publish(telemetryTopic, buffer);
  Serial.println(buffer);
  delay(1000);
}