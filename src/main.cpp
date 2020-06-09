#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config/mqtt.conf.h"
#include <EEPROM.h>

#define EEPROM_ID_ADDR 0
#define TRIG_PIN D7

WiFiClient espClient;
PubSubClient client(espClient);
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
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp((const char *)payload, "emit") == 0)
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
      client.subscribe((String("/base_stations/") + String(id)).c_str());
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

byte currentId;
void setup()
{
  currentId = EEPROM.read(EEPROM_ID_ADDR);
  Serial.begin(115200);
  Serial.println(String("ID: ") + String(currentId));
  Serial.println("Use change_id command to change it.");
  setup_wifi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(callback);
}

void loop()
{
  if (Serial.available() > 0)
  {
    if (Serial.readString() == "change_id")
    {
      currentId = Serial.readString().toInt();
      Serial.print("New ID: ");
      EEPROM.write(currentId, EEPROM_ID_ADDR);
    }
  }

  if (!client.connected())
    reconnect(currentId);
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;
    ++value;
    snprintf(msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
  }
}