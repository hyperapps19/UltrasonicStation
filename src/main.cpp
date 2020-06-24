#include <Arduino.h>
#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "GyverFilters.h"

#include "config/mqtt.conf.h"

#define VERSION "1.1"

#define EEPROM_ID_ADDR 0

#define TRIG_PIN D1
#define ECHO_PIN D2
#define SYNC_PIN D8

WiFiClient espClient;
PubSubClient client(espClient);
uint16_t currentId;

#define DEBUGGING_ENABLED
#ifdef DEBUGGING_ENABLED
#define DEBUG(msg) Serial.println(msg);
#else
#define DEBUG(msg)
#endif

#define SHELLSTART_STR "$ "

GKalman filter(40, 40, 0.5);

void ICACHE_RAM_ATTR Trigger_US()
{
  // Fake trigger the US sensor
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
}

bool ICACHE_RAM_ATTR checkDurationValidity(unsigned long duration)
{
  return duration <= 20000 && duration >= 20;
}

unsigned long ICACHE_RAM_ATTR getDuration(void)
{
  Trigger_US();
  while (digitalRead(ECHO_PIN) == HIGH)
    ;
  delay(2);
  Trigger_US();
  return pulseIn(ECHO_PIN, HIGH, 100000UL);
}

bool ICACHE_RAM_ATTR checkSignal(void)
{
  return checkDurationValidity(getDuration());
}

void ICACHE_RAM_ATTR onStartUS(void)
{
  if (client.connected())
  {
    uint64_t timeBegin = micros64();
    uint32_t tries = 0;
    while (!checkSignal() && tries < 100000)
    {
      tries++;
    }
    uint32_t delayUS = micros64() - timeBegin;
    if (delayUS < 30000)
      client.publish(("distances/" + String(currentId)).c_str(), String(filter.filtered(delayUS)).c_str());
  }
}

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  DEBUG();
  DEBUG(String("Connecting to ") + WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  DEBUG();
  DEBUG("WiFi connected");
  DEBUG("IP address: ");
  DEBUG(WiFi.localIP());
}

void reconnect(int id)
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    DEBUG("Attempting to connect to MQTT server...");
    String clientId = "BaseStation-";
    clientId += String(id);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      DEBUG("Connected to MQTT server.");
    }
    else
    {
      DEBUG("failed, rc=");
      DEBUG(client.state());
      DEBUG("try again in 5 seconds");
      delay(5000);
    }
  }
}

String help = "Commands:\n\
      \tchange_id - change station ID\n\
      \tget_id    - get station ID\n\
      \thelp      - print this information";

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(sizeof(uint16_t));
  currentId = EEPROM.get(EEPROM_ID_ADDR, currentId);

  pinMode(SYNC_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SYNC_PIN), onStartUS, FALLING);

  Serial.println();
  Serial.println("SmartRescuer Project --- Node Firmware v" + String(VERSION));
  Serial.println("Type \"help\" in shell for commands list");
  Serial.println();

  setup_wifi();

  client.setServer(MQTT_HOST, MQTT_PORT);
}

void loop()
{
  if (!client.connected())
    reconnect(currentId);
  client.loop();
  if (Serial.available())
  {
    String cmd = Serial.readString();
    Serial.print(SHELLSTART_STR);
    Serial.println(cmd);
    if (cmd.startsWith("change_id"))
    {
      Serial.println("Please enter new ID (0..65535): ");
      while (!Serial.available())
        ;
      currentId = Serial.readString().toInt();
      Serial.println(String("New ID: ") + currentId);
      EEPROM.put(EEPROM_ID_ADDR, currentId);
      EEPROM.commit();
      Serial.println("Written to EEPROM.");
    }
    else if (cmd.startsWith("get_id"))
      Serial.println(String("Current ID: ") + currentId);
    else if (cmd.startsWith("help"))
      Serial.println(help);
    else
      Serial.println("No such command. Type \"help\" in shell for commands list.");
    Serial.print(SHELLSTART_STR);
  }
}