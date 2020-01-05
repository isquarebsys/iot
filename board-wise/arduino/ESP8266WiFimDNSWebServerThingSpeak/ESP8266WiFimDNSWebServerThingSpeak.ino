// Web server and WiFi specific variables
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h>
#include <Wire.h>
WiFiClient  client;
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
ESP8266WebServer server(80);    //Webserver Object
// mDNS constants
const char* hostName = "i2bsys-temp-sensor";

// Sensor specific variables
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;


// Provider specific variables
#include "ThingSpeak.h"
char thingSpeakAddress[] = "api.thingspeak.com";
String APIKey = "key";             // enter your channel's Write API Key
const int updateThingSpeakInterval = 20 * 1000; // 20 second interval at which to update ThingSpeak
long lastConnectionTime = 0;
boolean lastConnected = false;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("WiFI: Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  initWiFi();
  initSensor();
  initProvider();
  printWifiStatus();
  setupMDNS();

  // Web server logic
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI
  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");

}

void loop() {
  // Handle webserver request
  server.handleClient();
  MDNS.update();
  if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }
  // Disconnect from Provider
  if (!client.connected() && lastConnected) {
    Serial.println("...disconnected");
    Serial.println();
    client.stop();
  }
  // Update Provider
  if (!client.connected() && (millis() - lastConnectionTime > updateThingSpeakInterval)) {
    String temperature = String(bmp.readTemperature());
    String pressure = String(bmp.readPressure());
    String altitude = String(bmp.readAltitude());
    String dataToSend = "field1=" + temperature + "&field2=" + pressure + "&field3=" + altitude;
    sendDataToProvider(dataToSend);
  }
  lastConnected = client.connected();
}

void handleRoot() {
  server.send(200, "text/plain", "My name is " + String(hostName)); // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void sendDataToProvider(String dataToSend) {
  if (client.connect(thingSpeakAddress, 80)) {
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + APIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(dataToSend.length());
    client.print("\n\n");
    client.print(dataToSend);
    lastConnectionTime = millis();

    if (client.connected()) {
      Serial.println("Connecting to Provider...");
      Serial.println();
    }
  }
}

void initProvider() {
  ThingSpeak.begin(client);
}
void initSensor() {
  if (!bmp.begin()) {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    while (1) {}
  }
}

void initWiFi() {
  WiFiManager wifiManager;
  // Reset means deleting the wifi ssid and password saved on the device. This is required for testing purpose alone
  //wifiManager.resetSettings();

  // If you simply use autoConnect without parameter, it will generate default AP point, so dont use it
  wifiManager.autoConnect("isquarebsysc");

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  // Fetches ssid and pass and tries to connect. If it does not connect it starts an access point with the specified name
  if (!wifiManager.autoConnect()) {
    Serial.println("WiFI failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setupMDNS() {
  if (MDNS.begin(hostName))
  {
    Serial.println(F("mDNS responder started"));
    Serial.print(F("I am: "));
    Serial.println(hostName);

    // Add service to MDNS-SD
    MDNS.addService("isquarebsys", "tcp", 80);
    /**
     * Following are not working
     */
    // MDNS.addService("isquarebsys-temp-sensor", "tcp", 80);
    // MDNS.addService(hostName+ESP.getChipId(), "tcp", 80);
  }
  else
  {
    while (1)
    {
      Serial.println(F("Error setting up MDNS responder"));
      delay(1000);
    }
  }
}

/* Returns a semi-unique id for the device. The id is based
   on part of a MAC address or chip ID so it won't be
   globally unique. */
uint16_t GetDeviceId()
{
#if defined(ARDUINO_ARCH_ESP32)
  return ESP.getEfuseMac();
#else
  return ESP.getChipId();
#endif
}

/* Append a semi-unique id to the name template */
String makeMine(const char *NameTemplate)
{
  uint16_t uChipId = GetDeviceId();
  String Result = String(NameTemplate) + String(uChipId, HEX);
  return Result;
}
