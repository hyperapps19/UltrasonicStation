#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config/mqtt.conf.h"
#include <EEPROM.h>

#define VERSION "0.3"

#define EEPROM_ID_ADDR 0
#define TRIG_PIN D7

WiFiClient espClient;
PubSubClient client(espClient);
uint16_t currentId;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

#define DEBUGGING_ENABLED
#ifdef DEBUGGING_ENABLED
#define DEBUG(msg) Serial.println(msg);
#else
#define DEBUG(msg)
#endif

#define SHELLSTART_STR "$ "

void emitUltrasoundSignal()
{
  DEBUG("Emitting ultrasound signal...");
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
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

void callback(char *topic, byte *payload, unsigned int length)
{
  if (atoi(std::string((const char *)payload, length).c_str()) == currentId)
    emitUltrasoundSignal();
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
      client.subscribe("ultrasound_emit");
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
  EEPROM.begin(sizeof(uint16_t));
  currentId = EEPROM.get(EEPROM_ID_ADDR, currentId);
  Serial.begin(115200);
  Serial.println();
  Serial.println("SmartRescuer Project --- Node Firmware v" + String(VERSION));
  Serial.println("Type \"help\" in shell for commands list");
  Serial.println();
  Serial.println(SHELLSTART_STR);
  setup_wifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
}

void loop()
{
  if (Serial.available())
  {
    String cmd = Serial.readString();
    Serial.println(cmd);
    if (cmd == "change_id")
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
    else if (cmd == "get_id")
      Serial.println(String("Current ID: ") + currentId);
    else if (cmd == "help")
      Serial.println(help);
    else
      Serial.println("No such command. Type \"help\" in shell for commands list.");
    Serial.print(SHELLSTART_STR);
  }

  if (!client.connected())
    reconnect(currentId);
  client.loop();
}