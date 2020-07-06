#ifndef ESP32
#error This file is for ESP32 only (Wifi Events)
#endif

// **************************************** compiler ****************************************
#define APPNAME "Watchdog for MQTT Sensor/+/Action messages"
#include <version.h> // include BUILD_NUMBER, VERSION, VERSION_SHORT

// #define DEBUG_TO_SERIAL 1           // General parameter for debugging
// #define DEBUG_ASYNCMQTTCLIENT 1
// #define DEBUG_NTP 1
// #define MBEDTLS_ERROR_C

// ------------------------------------ DEBUG ----------------------------------

#ifdef DEBUG_TO_SERIAL
const long SERIAL_BAUD = 115200;
#define DEBUG_BEGIN() Serial.begin(SERIAL_BAUD)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...)           \
  {                                 \
    Serial.printf("%lu", millis()); \
    Serial.print("mS L");           \
    Serial.print(__LINE__);         \
    Serial.print(" ");              \
    Serial.printf(__VA_ARGS__);     \
  }
#define DEBUG_FLUSH() Serial.flush()
#else
#define DEBUG_BEGIN()
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#define DEBUG_FLUSH()
#endif

// **************************************** include ****************************************

#include <time.h>             // Arduino library - Secure connections need the right time to know validity of certificate.
#include <Arduino.h>          // Arduino library - https://github.com/espressif/arduino-esp32/tree/master/cores/esp32
#include "secrets.h"          // Credentials
#include <WiFi.h>             // Arduino library - https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/src
#include <ESP32_MailClient.h> // https://github.com/mobizt/ESP32-Mail-Client
// #include <Time.h>
#include <NtpClientLib.h>    // https://github.com/gmag11/NtpClient
#include <TimeLib.h>         // https://github.com/PaulStoffregen/Time
#include <Timezone.h>        // https://github.com/JChristensen/Timezone
#include <AsyncMqttClient.h> // https://github.com/marvinroger/async-mqtt-client

// **************************************** CONSTANTS ****************************************

// WiFi
#ifndef SECRETS_H
#define SECRETS_H
const char WIFI_SSID[] = "*** WIFI SSID ***";
const char WIFI_PASSWORD[] = "*** WIFI PASSWORD ***";
const char MQTT_HOST[] = "192.168.1.x";
const int MQTT_PORT = 1883;
#endif

// NTP
// const char* NTPSERVER = "nl.pool.ntp.org"; // default pool.ntp.org
const char *NTPSERVER = "nl.pool.ntp.org";
const uint16_t NTP_TIMEOUT = 1500;

#define MQTT_TOPIC_SENSOR_ACTION "sensor/+/action"
#ifndef EMAIL_ADDRESS
#define EMAIL_ADDRESS "name@host.org"
#endif
#ifndef HOSTDOMAIN
#define HOSTDOMAIN "@host.org"
#endif

//**************************************** VARIABLES ****************************************

// time
time_t noww;
time_t prevDisplay = 0; // when the digital clock was displayed

// Timezone: Central Europe
TimeChangeRule CEST = {/*abbrev*/ "CEST", /*week*/ Last, /*dow*/ Sun, /*month*/ Mar, /*hour*/ 2, /*offset*/ 120}; // Central European Summer Time
TimeChangeRule CET = {/*abbrev*/ "CET", /*week*/ Last, /*dow*/ Sun, /*month*/ Oct, /*hour*/ 3, /*offset*/ 60};    // Central European Standard Time
Timezone CE(/*dstStart*/ CEST, /*stdStart*/ CET);

// WiFi
bool _wifiGotIPTriggered = false;

// NTP
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent;            // Last triggered event

// AsyncMqttClient
char _hostname[12]; // esp-xxxxxx
AsyncMqttClient mqttClient;
bool _MqttConnected = false;

//The Email Sending data object contains config and data to send
SMTPData smtpData;
char emailid[21];

// **************************************** FUNCTIONS ****************************************

// Send payload to email
void sendActionEmail(String payload);

// **************************************** WiFi ****************************************

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  DEBUG_PRINTF("WiFi connected. IP address: ");
  DEBUG_PRINTLN(IPAddress(info.got_ip.ip_info.ip.addr));
  _wifiGotIPTriggered = true;
}

void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  DEBUG_PRINT("WiFi lost connection. Reason: ");
  DEBUG_PRINTLN(info.disconnected.reason);
}

// **************************************** AsyncMqttClient ****************************************

void onMqttConnect(bool sessionPresent)
{
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTF("Connected to MQTT. Session present: %s\n", ((sessionPresent) ? "true" : "false"));
#endif
  _MqttConnected = true;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTLN("Disconnected from MQTT.");
#endif
  // ERROR
  _MqttConnected = false;
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTLN("Subscribe acknowledged.");
  DEBUG_PRINT("  packetId: ");
  DEBUG_PRINTLN(packetId);
  DEBUG_PRINT("  qos: ");
  DEBUG_PRINTLN(qos);
#endif
}

void onMqttUnsubscribe(uint16_t packetId)
{
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTLN("Unsubscribe acknowledged.");
  DEBUG_PRINT("  packetId: ");
  DEBUG_PRINTLN(packetId);
#endif
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTF("Publish received.\n");
  DEBUG_PRINTF("  topic : %s\n", topic);
  DEBUG_PRINTF("  qos   : %d\n", properties.qos);
  DEBUG_PRINTF("  dup   : %s\n", properties.dup ? "true" : "false");
  DEBUG_PRINTF("  retain: %s\n", properties.retain ? "true" : "false");
  DEBUG_PRINTF("  len   : %d\n", len);
  DEBUG_PRINTF("  index : %d\n", index);
  DEBUG_PRINTF("  total : %d\n", total);
#endif
  sendActionEmail(payload);
}

void onMqttPublish(uint16_t packetId)
{
  // lastAcknPackageId = packetId;
#if defined(DEBUG_ASYNCMQTTCLIENT)
  DEBUG_PRINTF("Publish packetid %d acknowledged.\n", packetId);
#endif
}

void MqttConnect()
{
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  mqttClient.connect();
}

// **************************************** NTP Time Timezone ****************************************

void processSyncEvent(NTPSyncEvent_t ntpEvent)
{
#ifdef DEBUG_NTP
  if (ntpEvent < 0)
  {
    DEBUG_PRINTF("NTP Time Sync error %d:", ntpEvent);
    if (ntpEvent == noResponse)
      DEBUG_PRINTLN("No response from server");
    else if (ntpEvent == invalidAddress)
      DEBUG_PRINTLN("Address not reachable");
    else if (ntpEvent == errorSending)
      DEBUG_PRINTLN("An error happened while sending the request");
    else if (ntpEvent == responseError)
      DEBUG_PRINTLN("Wrong response received");
    else
      DEBUG_PRINTLN("Unknown error. Check source code");
  }
  else if (!ntpEvent)
  {
    DEBUG_PRINTF("Got NTP time: ");
    DEBUG_PRINTLN(NTP.getTimeDateString(NTP.getLastNTPSync()));
  }
  else
  {
    DEBUG_PRINTF("NTP request sent, waiting for response.\n");
  }
#endif
}

// Print local time given a Timezone object, UTC and an extra string description
void DateTimeLog(const char *description, bool localtime = true)
{
  time_t lt;
  lt = (localtime) ? CE.toLocal(now()) : now();
  DEBUG_PRINTF("%.4d-%.2d-%.2d %.2d:%.2d:%.2d %s", year(lt), month(lt), day(lt), hour(lt), minute(lt), second(lt), description);
}

// **************************************** Email ****************************************

//Callback function to get the Email sending status
void sendCallback(SendStatus msg)
{
  DEBUG_PRINTF("Email sent. Status : %s : %s\n", (msg.success() ? "success" : "error"), msg.info().c_str());
}

// Generate random string in "str" of length "len" chars
String get_random_string(size_t len)
{
  char dummies[21];
  size_t i;
  // reseed the random number generator
  for (i = 0; i < len; i++)
  {
    // Add random printable ASCII char
    dummies[i] = (rand() % ('{' - 'a')) + 'a';
  }
  dummies[i] = '\0';
  return String(dummies);
}

// Send payload
void sendActionEmail(String payload)
{
  smtpData.setDebug(true);
  //Set the Email host, port, account and password
  smtpData.setLogin(/* host */ SMTP_HOST, /* port */ SMTP_PORT, /* loginEmail */ LOGIN_EMAIL, /* loginPassword */ LOGIN_PASSWORD);

  //For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be
  //enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
  smtpData.setSTARTTLS(true);

  //Set the sender name and Email
  smtpData.setSender("ESP32", EMAIL_ADDRESS);

  //Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("Normal");

  //Set the subject
  smtpData.setSubject("MQTT (" + String(MQTT_TOPIC_SENSOR_ACTION) + ") message.");

  //Set the message - normal text or html format
  String buffer = "MQTT message received from " + String(_hostname) + ".\n\n";
  buffer += "Payload: " + payload + "\n";
  buffer += "\nGreetings, IOT\n";
  smtpData.setMessage(buffer, false);

  //Add recipients, can add more than one recipient
  smtpData.addRecipient(EMAIL_ADDRESS);

  // ******** HIER IETS VAN TIJD INVOEGEN **************
  String temp = "Message=ID: <" + String(_hostname) + "." + get_random_string(10) + HOSTDOMAIN + ">";
  smtpData.addCustomMessageHeader(temp);

  //Set the storage types to read the attach files (SD is default)
  //smtpData.setFileStorageType(MailClientStorageType::SPIFFS);
  // smtpData.setFileStorageType(MailClientStorageType::SD);
  smtpData.setSendCallback(sendCallback);

  //Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData))
    DEBUG_PRINTF("Error sending Email, %s ", MailClient.smtpErrorReason().c_str());

  //Clear all data from Email object to free memory
  smtpData.empty();
}

// hostname of device
char *Hostname(char hostname[])
{
#ifdef ESP32
  sprintf(hostname, "esp-%06x", (uint32_t)ESP.getEfuseMac() & 0x00ffffff);
#elif defined(ESP8266)
  sprintf(hostname, "esp-%06x", ESP.getChipId());
#endif
  return hostname;
}

// **************************************** setup ****************************************

void setup()
{
  // serial
  DEBUG_BEGIN();
  DEBUG_PRINTLN();
  DEBUG_PRINTLN(APPNAME);
  DEBUG_PRINTLN(VERSION);
  DEBUG_PRINTLN();
  
  DEBUG_PRINTF("ESP named '%s' started\n", Hostname(_hostname));

  // WiFi
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // NTP
  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });
}

// **************************************** loop ****************************************

void loop()
{
  // WiFi -> NTP + MQTT
  if (_wifiGotIPTriggered)
  {
    _wifiGotIPTriggered = false;
    DEBUG_PRINTF("WiFi got IP event: triggers NTP and MQTT\n");

    NTP.setInterval(63); // 60 minutes + 3 seconds
    NTP.setNTPTimeout(NTP_TIMEOUT);
    // (String ntpServerName = DEFAULT_NTP_SERVER, int8_t timeOffset = DEFAULT_NTP_TIMEZONE, bool daylight = false, int8_t minutes = 0, AsyncUDP* udp_conn = NULL);
    NTP.begin(NTPSERVER, 0, false, 0);

    MqttConnect(); // AsyncMQTTClient, client = "esp32xxxxxx",xxxxxx=ChipId
  }

  // NTP -> Randomize
  if (syncEventTriggered)
  {
    syncEventTriggered = false;

    processSyncEvent(ntpEvent);
    if (NTP.getInterval() < 300)
    {
      NTP.setInterval(3603); // 1 hour + 3 seconds
      srand(now()); // Initialize random number generator
    }
  }

  // MQTT -> Subscribe
  if (_MqttConnected)
  {
    mqttClient.subscribe(MQTT_TOPIC_SENSOR_ACTION, 0);
    _MqttConnected = false;
  }

}
