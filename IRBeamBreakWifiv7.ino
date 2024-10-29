#include <WiFi.h>
#include <SPI.h>
#include <NetworkUdp.h>
#include <ArduinoJson.h>

//status empty

// IR sensor pin
#define LEDPIN 13
#define IRLED1 22  // IR LED pin 1
#define IRLED2 23  // IR LED pin 2
#define SENSORPIN 18  // IR Receiver Pin
#define STATIONPIN // Station Pin

// WiFi network name and password:
const char *networkName = "ENGG2K3K";

// Replace these with ip and port
const char *udpAddress = "10.20.30.1"; // change this to the IP you are sending too
const int udpPort = 2000; // change this to the port you are sending too
const char *statIP = "10.20.30.59"; // change this to the IP of the ESP32
const int localPort = 5009; // change this to the port of the ESP32
const char *clientID = "ST09"; // change this to the name of the station
char incomingPacket[255]; 
int packetSize;

boolean connected = false;
boolean connecting = false;
boolean handshake = false;
boolean handshaking = false;

//The udp library class
NetworkUDP udp;
JsonDocument doc;

// variables will change:
int sensorState = 0, lastState = 0;

const char *status = "OFF";
const char *action = "ON";
int seqNum = random(1000, 30001); 
const long interval = 2000;
unsigned long previousMillis = 0;

struct MCP {
  const char* client_type;
  const char* message;
  const char* client_id;
  int sequence_number;
  const char* action = "ON";
};

void setup() {
  Serial.begin(115200);

  // Initialise the LEDs and Sensors
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(IRLED1, OUTPUT);
  pinMode(IRLED2, OUTPUT);
  pinMode(SENSORPIN, INPUT);
  digitalWrite(SENSORPIN, HIGH);
  digitalWrite(IRLED1, HIGH);
  digitalWrite(IRLED2, HIGH);
  digitalWrite(12, HIGH);
  digitalWrite(14, HIGH);
  randomSeed(analogRead(0));
  sensorState = digitalRead(SENSORPIN);
  connectToWiFi(networkName);

}

void loop() {
  unsigned long currentMillis = millis();
  if (connected) {
    if (handshake == false){
      if (handshaking == false){
        sensorState = digitalRead(SENSORPIN);
        if (sensorState == LOW && sensorState != lastState) {
          digitalWrite(13, HIGH);
          lastState = sensorState;
        }
        else if (sensorState == HIGH && sensorState != lastState) {
          digitalWrite(13, LOW);
          lastState = sensorState;
        }
        if (currentMillis - previousMillis >= interval) {
          // save the last time you blinked the LED
          previousMillis = currentMillis;
          doc["client_type"] = "STC";
          doc["message"] = "STIN";
          doc["client_id"] = clientID;
          doc["sequence_number"] = seqNum;
          serializeJson(doc, Serial);
          udp.beginPacket(udpAddress, udpPort);
          serializeJson(doc, udp);
          udp.endPacket();
          Serial.printf("CPIN SENT");
          seqNum++;
        }
      }
    }
    // Read the IR sensor status (HIGH = unbroken, LOW = broken)
    // Check if the sensor beam is broken
    sensorState = digitalRead(SENSORPIN);
    if (sensorState == LOW && sensorState != lastState) {
      // turn LED on
        //digitalWrite(13, HIGH);
        if (handshake) {
          digitalWrite(13, HIGH);
          doc["client_type"] = "STC";
          doc["message"] = "TRIP";
          doc["client_id"] = clientID;
          doc["sequence_number"] = seqNum;
          status = "ON";
          doc["status"] = status;
          serializeJson(doc, Serial);
          udp.beginPacket(udpAddress, udpPort);
          serializeJson(doc, udp);
          udp.endPacket();
          seqNum++;
          Serial.println("Broken");
          lastState = sensorState;
      }
    }
    if (sensorState == HIGH && sensorState != lastState) {
        // turn LED off
        //digitalWrite(13, LOW);
        if (handshake) {
          digitalWrite(13, LOW);
          doc["client_type"] = "STC";
          doc["message"] = "TRIP";
          doc["client_id"] = clientID;
          doc["sequence_number"] = seqNum;
          status = "OFF";
          doc["status"] = status;      
          serializeJson(doc, Serial);
          udp.beginPacket(udpAddress, udpPort);
          serializeJson(doc, udp);
          udp.endPacket();
          seqNum++;
          Serial.println("Unbroken");
          lastState = sensorState;
      }
    }
    packetSize = udp.parsePacket();
    if (packetSize) {
      // Read incoming UDP packet
      int len = udp.read(incomingPacket, 255);
      if (len > 0) {
        incomingPacket[len] = '\0';  // Null-terminate the string
      }
      Serial.printf("Received packet: %s\n", incomingPacket);
      // Parse and deserialize the JSON
      MCP data = parseJSON(incomingPacket);
      // Print the deserialized data
      Serial.printf("Client Type: %s, Message: %s, Client ID: %s, Sequence Number: %d, Action: %s\n", 
      data.client_type, data.message, data.client_id, data.sequence_number, action);
    } 
  }
  else if (connecting == false){
    connectToWiFi(networkName);
  }
}


void connectToWiFi(const char *ssid) {
  connecting = true;
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);  // Will call WiFiEvent() from another thread.

  //Initiate connection
  WiFi.begin(ssid);
  WiFi.config(statIP);

  Serial.println("Waiting for WIFI connection...");
}

// WARNING: WiFiEvent is called from a separate FreeRTOS task (thread)!
void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      //When connected set
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      //initializes the UDP state
      //This initializes the transfer buffer
      udp.begin(WiFi.localIP(), localPort);
      connected = true;
      connecting = false;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      connecting = false;
      break;
    default: break;
  }
}

MCP parseJSON(const char* json) {
  handshaking = true;
  StaticJsonDocument<256> doc;
  MCP data;

  // Deserialize JSON
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return data; // Return empty data on error
  }

  // Assign values from the parsed JSON
  data.client_type = doc["client_type"];
  data.message = doc["message"];
  if (doc["message"] == "STRQ"){
    doc["client_type"] = "STC";
    doc["message"] = "STAT";
    doc["client_id"] = clientID;
    doc["sequence_number"] = seqNum;
    doc["status"] = status;   
    serializeJson(doc, Serial);
    udp.beginPacket(udpAddress, udpPort);
    serializeJson(doc, udp);
    udp.endPacket();
    seqNum++;
  }
  else if (doc["message"] == "AKIN"){
    handshake = true; 
  }
  data.client_id = doc["client_id"];
  data.sequence_number = doc["sequence_number"];
  if (doc.containsKey("action")) {
    if (doc["action"] == "OFF"){
      digitalWrite(12, LOW);
      digitalWrite(IRLED2, LOW);
      action = "OFF";
    }
    else{
      digitalWrite(12, HIGH);
      digitalWrite(IRLED2, HIGH);
      action = "ON";
    }
  }
  handshaking = false;
  return data;
}