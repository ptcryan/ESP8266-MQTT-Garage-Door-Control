#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <SimpleTimer.h>

void cancelLeftSwitch(void);
void cancelRightSwitch(void);
void publishSwitchState(void);
void setSwitchState(void);
void callback(char* p_topic, byte* p_payload, unsigned int p_length);
void reconnect(void);
void setup(void);
void loop(void);

#define MQTT_VERSION MQTT_VERSION_3_1_1
#define SWITCH_DURATION 2000
#define MAX_SRV_CLIENTS 1

const char* ssid = _WIFI_SSID_;
const char* password = _WIFI_PASS_;

// MQTT: ID, server IP, port, username and password
const PROGMEM char* MQTT_CLIENT_ID = _MQTT_CLIENT_ID_;
const PROGMEM char* MQTT_SERVER_IP = _MQTT_SERVER_IP_;
const PROGMEM uint16_t MQTT_SERVER_PORT = _MQTT_SERVER_PORT_;
const PROGMEM char* MQTT_USER = _MQTT_USER_;
const PROGMEM char* MQTT_PASSWORD = _MQTT_PASSWORD_;

// MQTT: topics
const char* MQTT_LEFT_DOOR_STATE_TOPIC = "home/main_floor/garage/left_door/switch/status";
const char* MQTT_RIGHT_DOOR_STATE_TOPIC = "home/main_floor/garage/right_door/switch/status";
const char* MQTT_LEFT_DOOR_COMMAND_TOPIC = "home/main_floor/garage/left_door/switch/set";
const char* MQTT_RIGHT_DOOR_COMMAND_TOPIC = "home/main_floor/garage/right_door/switch/set";

// left switch state, initially false (not pressed)
boolean state_left_switch = false;

// right switch state, initially false (not pressed)
boolean state_right_switch = false;

// payloads by default (button pressed/released)
const char* BUTTON_PRESSED = "1";
const char* BUTTON_RELEASED = "0";

const PROGMEM uint8_t LEFT_SWITCH = 4;
const PROGMEM uint8_t RIGHT_SWITCH = 5;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

SimpleTimer leftSwitchTimer;
SimpleTimer rightSwitchTimer;

WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];

void cancelLeftSwitch() {
  state_left_switch = false;
  setSwitchState();
  publishSwitchState();
}

void cancelRightSwitch() {
  state_right_switch = false;
  setSwitchState();
  publishSwitchState();
}

// This function is called each time we need to report status of switches
void publishSwitchState() {
  if (state_left_switch) {
    client.publish(MQTT_LEFT_DOOR_STATE_TOPIC, BUTTON_PRESSED, true);
  } else {
    client.publish(MQTT_LEFT_DOOR_STATE_TOPIC, BUTTON_RELEASED, true);
  }

  if (state_right_switch) {
    client.publish(MQTT_RIGHT_DOOR_STATE_TOPIC, BUTTON_PRESSED, true);
  } else {
    client.publish(MQTT_RIGHT_DOOR_STATE_TOPIC, BUTTON_RELEASED, true);
  }
}

// This function sets the state of the switches
void setSwitchState() {
  if (state_left_switch) {
    digitalWrite(LEFT_SWITCH, HIGH);
    Serial.println("Set left switch high");
  } else {
    digitalWrite(LEFT_SWITCH, LOW);
    Serial.println("Set left switch low");
  }

  if (state_right_switch) {
    digitalWrite(RIGHT_SWITCH, HIGH);
    Serial.println("Set right switch high");
  } else {
    digitalWrite(RIGHT_SWITCH, LOW);
    Serial.println("Set right switch low");
  }
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  Serial.println(p_topic);
  Serial.print("Processing payload: ");
  Serial.println(payload);

  // handle message topic
  if (String(MQTT_LEFT_DOOR_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(BUTTON_PRESSED))) {
      state_left_switch = true;
      setSwitchState();
      publishSwitchState();
      leftSwitchTimer.setTimeout(SWITCH_DURATION, cancelLeftSwitch);
    }
  }

  // handle message topic
  if (String(MQTT_RIGHT_DOOR_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(BUTTON_PRESSED))) {
      state_right_switch = true;
      setSwitchState();
      publishSwitchState();
      leftSwitchTimer.setTimeout(SWITCH_DURATION, cancelRightSwitch);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(_MQTT_CLIENT_ID_, _MQTT_USER_, _MQTT_PASSWORD_)) {
      Serial.println("INFO: connected");
      // Once connected, publish an announcement...
      publishSwitchState();
      // ... and resubscribe
      client.subscribe(MQTT_LEFT_DOOR_COMMAND_TOPIC);
      client.subscribe(MQTT_RIGHT_DOOR_COMMAND_TOPIC);
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  server.begin();
  server.setNoDelay(true);

  // Configure swtich pins
  digitalWrite(LEFT_SWITCH, LOW);
  digitalWrite(RIGHT_SWITCH, LOW);
  pinMode(LEFT_SWITCH, OUTPUT);
  pinMode(RIGHT_SWITCH, OUTPUT);

  Serial.print("INFO: Connecting to ");
  WiFi.mode(WIFI_STA);
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Connected.");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(_MQTT_CLIENT_ID_);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    //String type;
    // if (ArduinoOTA.getCommand() == U_FLASH)
      // type = "sketch";
    // else // U_SPIFFS
      // type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    // Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // init the MQTT connection
  client.setServer(_MQTT_SERVER_IP_, _MQTT_SERVER_PORT_);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  uint8_t i;
  //check if there are any new clients
  if (server.hasClient()){
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i].connected()){
        if(serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        Serial1.print("New client: "); Serial1.print(i);
        continue;
      }
    }
    //no free/disconnected spot so reject
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  //check clients for data
  for(i = 0; i < MAX_SRV_CLIENTS; i++){
    if (serverClients[i] && serverClients[i].connected()){
      if(serverClients[i].available()){
        //get data from the telnet client and push it to the UART
        while(serverClients[i].available()) Serial.write(serverClients[i].read());
      }
    }
  }

  //check UART for data
  // To echo local debug information (from Serial.println()) place a
  // jumper wire between TX & RX on the ESP8266.
  if(Serial.available()){
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);
    //push UART data to all connected telnet clients
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        serverClients[i].write(sbuf, len);
        delay(1);
      }
    }
  }

  ArduinoOTA.handle();
  client.loop();
  leftSwitchTimer.run();
  rightSwitchTimer.run();
}
