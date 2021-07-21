#define SIM800L_IP5306_VERSION_20200811

// Select your modem:
#define TINY_GSM_MODEM_SIM800

#include "utilities.h"
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <esp_task_wdt.h>

// Set serial for AT commands (to the module SIM800L)
#define SerialAT Serial1

// Set timer Watchdog
#define WDT_TIMEOUT 300

//Wind Speed Serial
SoftwareSerial Windspeed_Serial;
// Wind Direction Serial
SoftwareSerial Winddirection_Serial;

//Define TX and RX Pin for connnected Wind Speed Sensor
#define TXWindspeed 4
#define RXWindspeed 5
//Define TX and RX Pin for connnected Wind Direction Sensor 
#define TXWinddirection 18
#define RXWinddirection 19

// Address for calling Data from all sensor
byte request[] = {0x02, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x39};

String Direction;
float DirectionDegree;

// Define how you're planning to connect to the internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// Network Detail
const char apn[]      = "True internet";
const char gprsUser[] = "true";
const char gprsPass[] = "true";

// MQTT details
const char* broker = "electsut.ddns.net";
const char* topicOut = "windstation01/Node1Sub";
const char* topicIn = "windstation01/Node1Pub";

StaticJsonDocument<256> doc;
char message[256];

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  setupModem();

  Serial.println("wait . . .");

//Serial begin Module SIM800L 
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

//Serial begin Wind Speed Sensor
  Windspeed_Serial.begin(9600, SWSERIAL_8N1, RXWindspeed, TXWindspeed);
  
//Serial begin Wind Direction Sensor
  Winddirection_Serial.begin(9600, SWSERIAL_8N1, RXWinddirection, TXWinddirection);
  
  delay(5000);
  Serial.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);

  while(!modem.waitForNetwork()){
    Serial.println("Fail, Try to connected");
    delay(1000);
    ESP.restart();
  }
  
  Serial.print("Connected to telecom : ");
  Serial.println("Signal Quality: " + String(modem.getSignalQuality()));
  
  Serial.println("Connecting to GPRS network.");
  while(!modem.gprsConnect(apn, gprsUser, gprsPass)){
    Serial.println("Fail, Try to connected");
    delay(500);
    continue;
  }
  Serial.println("Connected to GPRS: " + String(apn));
  mqtt.setServer(broker, 8883);
  mqtt.setCallback(mqttCallback);
  Serial.println("Connecting to MQTT Broker: " + String(broker));
  while(mqttConnect()==false){
    Serial.println("Try connected to MQTT Broker !!!");
    continue;
  }
  Serial.println();

//Watchdog init
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
}

void loop() {

//Count Time for restart ESP
  if(millis() >= 3600000){
    ESP.restart();
  }
  
//Wind Speed Coding
  Windspeed_Serial.write(request, sizeof(request));
  Windspeed_Serial.flush();

  byte windspeeddatacallback[8];
  Windspeed_Serial.readBytes(windspeeddatacallback, 8);

//  Serial.print("Wind Calculation : ");
  int WindSpeedData = (windspeeddatacallback[3] + windspeeddatacallback[4]);
  // Wind speed unit m/s
  float WindSpeed = WindSpeedData / 10.0;
  // Wind speed unit km/hr
  float WindSpeed_hr = WindSpeed * 3.6;
//  Serial.print(WindSpeed);
//  Serial.println(" m/s");
  
//Wind Direction Coding
  Winddirection_Serial.write(request, sizeof(request));
  Winddirection_Serial.flush();

  byte winddirectiondatacallback[8];
  Winddirection_Serial.readBytes(winddirectiondatacallback, 8);

//  Serial.print("Wind Direction : ");
  int WinddirectionDEC = (winddirectiondatacallback[3] + winddirectiondatacallback[4]);
//  Serial.print(WinddirectionDEC, DEC);
//  Serial.print(" : ");

  switch(WinddirectionDEC){
    case 0:
      Direction = "North";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 1:
      Direction = "Northeast by north";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 2:
      Direction = "Northeast";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 3:
      Direction = "Northeast by east";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 4:
      Direction = "East";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 5:
      Direction = "Southeast by east";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 6:
      Direction = "Southeast";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 7:
      Direction = "Southeast by south";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 8:
      Direction = "South";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 9:
      Direction = "Southwest by south";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 10:
      Direction = "Southwest";
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 11:
      Direction = "Southwest by west";    
      DirectionDegree = WinddirectionDEC * 22.5; 
      break;
    case 12:
      Direction = "West";     
      DirectionDegree = WinddirectionDEC * 22.5;
      break;
    case 13:
      Direction = "Northwest by west";
      DirectionDegree = WinddirectionDEC * 22.5;     
      break;
    case 14:
      Direction = "Northwest";
      DirectionDegree = WinddirectionDEC * 22.5;     
      break;
    default:
      Direction = "Northwest by north";
      DirectionDegree = WinddirectionDEC * 22.5;     
      break;
  }
  
//  Serial.println(Direction);
  doc["wind"] = WindSpeed;
  doc["windHR"] = WindSpeed_hr;
  doc["winddirection"] = Direction;
  doc["winddirectionDegree"] = DirectionDegree;
  doc["lat"] = 17.69852;
  doc["long"] = 102.78207;
  doc["Signal"] = modem.getSignalQuality();
  
  serializeJson(doc, message);

//Check Status of GPRS
  while(!modem.isGprsConnected()){
    modem.gprsConnect(apn, gprsUser, gprsPass);
    continue;
  }

//Check status of mqtt
  if(!mqtt.connected()){
    while(mqttConnect() == false) continue;
  }

//Check status of mqtt
  if(mqtt.connected()){
    mqtt.loop();
  }

//MQTT and GPRS is connected
  if(modem.isGprsConnected() && mqtt.connected()){
    Serial.print("MQTT pub = ");
    Serial.print(mqtt.publish(topicOut, message, sizeof(message)));
    Serial.print(" MQTT con = ");
    Serial.print(mqtt.connected());
    Serial.print(" MQTT st = ");
    Serial.print(mqtt.state());
    Serial.print(" GPRS = ");
    Serial.println(modem.isGprsConnected());
    Serial.println(message);
//mqtt disconnected
    mqtt.disconnect();
  }

  delay(10000);
  
// Reset Timer Watchdog
  esp_task_wdt_reset();
  
}

boolean mqttConnect()
{
  if(!mqtt.connect("device01"))
  {
    Serial.print(".");
    return false;
  }
  mqtt.subscribe(topicIn);
  return mqtt.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int len)
{
  Serial.print("Message receive: ");
  Serial.write(payload, len);
  Serial.println();
}
