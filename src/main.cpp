#include "SH1106Brzo.h"
#include "SH1106Wire.h"
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <WiFiUdp.h>

// PIN mapping
static const uint8_t D0 = 16;
static const uint8_t D1 = 5;
static const uint8_t D2 = 4;
static const uint8_t D3 = 0;
static const uint8_t D4 = 2;
static const uint8_t D5 = 14;
static const uint8_t D6 = 12;
static const uint8_t D7 = 13;
static const uint8_t D8 = 15;
static const uint8_t D9 = 3;
static const uint8_t D10 = 1;

// Display
// D4 -> SDA
// D3 -> SCL
SH1106Brzo display(0x3c, D4, D3);

// WIFI
const char *ssid = "";
const char *password = "";

// NTP time server
// const char *ntpServerName = "time.nist.gov";
// const char *ntpServerName = "cn.ntp.org.cn";
const char *ntpServerName = "1.asia.pool.ntp.org";

// UDP for ntp server
WiFiUDP Udp;
unsigned int localPort = 8888; // local port to listen for UDP packets

// Time Zone
const int timeZone = +8;

// NTP var
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing
                                    // packets
// return lastNtpTime if unable to get the time
time_t lastNtpTime = 0;
unsigned long lastNtpTimeFix = 0;

// Baud
const int baud = 115200;

// Date mapping
String weekDay[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

String Months[] = {
    "Jan",  "Feb", "Mar",  "Apr", "May", "June",
    "July", "Aug", "Sept", "Oct", "Nov", "Dec",
};

// Time Since
unsigned long timeSinceLastClock = 0;

void sendNTPpacket(IPAddress &address);
time_t getNtpTime();
void OLEDDisplayCtl();

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.onStart([]() { Serial.println("Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // init NTP UDP
  Udp.begin(localPort);
  // Serial.println(Udp.localPort());
  // Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(600);

  Serial.println("Ready...");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.drawString(0, 0, "Hello world");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 10, "Hello world");
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 26, "Hello world");
  display.display();
}

void loop() {
  ArduinoOTA.handle();

  if (millis() - timeSinceLastClock > 1000) {
    OLEDDisplayCtl();
    timeSinceLastClock = millis();
  }
}

void OLEDDisplayCtl() {

  display.clear();

  // weather & temperature & humidity
  // display.setTextAlignment(TEXT_ALIGN_LEFT);
  // display.setFont(Meteocons_Plain_21);
  // display.drawString(0, 0, weatherImgMapping[weatherImg]);

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Roboto_14);
  display.drawString(0, 3, Months[month() - 1] + ". " + (String)day() + ", " +
                               (String)year());

  // date
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(Roboto_14);
  display.drawString(128, 3, weekDay[weekday() - 1]);

  // clock
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Roboto_Black_48);
  time_t hourInt = hour();
  String hourStr;
  if (10 > hourInt) {
    hourStr = "0" + (String)hourInt;
  } else {
    hourStr = (String)hourInt;
  }
  display.drawString(0, 19, hourStr);

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(Roboto_Black_18);
  display.drawString(64, 27, ":");

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(Roboto_14);
  display.drawString(64, 47, (String)second());

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(Roboto_Black_48);
  time_t minuteInt = minute();
  String minuteStr;
  if (10 > minuteInt) {
    minuteStr = "0" + (String)minuteInt;
  } else {
    minuteStr = (String)minuteInt;
  }
  display.drawString(128, 19, minuteStr);

  display.display();
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 5000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      lastNtpTime = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      lastNtpTimeFix = millis();
      return lastNtpTime;
    }
  }
  Serial.println("No NTP Response :-(");
  return lastNtpTime + ((int)(millis() - lastNtpTimeFix) /
                        1000); // return lastNtpTime if unable to get the time
}
