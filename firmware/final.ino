#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi credentials
const char* ssid = "seshan 2.3";
const char* password = "act12345";

// Flask server URL
const char* serverUrl = "https://bus-location-tracker.onrender.com/update";

// Telegram bot credentials
#define BOT_TOKEN "8164816443:AAHpOx5KmA8-1F0_eD2v4P6F_AC_XwIp6Gs"
#define CHAT_ID "1519152273"
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// SIM900A pins
HardwareSerial sim900a(1);
String incomingData = "";
String senderNumber = "";

// GPS pins
#define RXPin 16
#define TXPin 17
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// Variables
float latitude = 0.0;
float longitude = 0.0;
bool wifiConnected = false;
unsigned long lastTelegramUpdate = 0;
bool telegramStartupMessageSent = false;

void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED initialization failed");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  gpsSerial.begin(9600, SERIAL_8N1, RXPin, TXPin);
  sim900a.begin(9600, SERIAL_8N1, 32, 33);
  initSIM900A();

  WiFi.begin(ssid, password);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to WiFi...");
  display.display();
  Serial.println("Connecting to WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
    client.setInsecure();
    String message = "University Bus has started! \xF0\x9F\x9A\x8C\nBe on time to your stop.\nLive location:\nhttps://bus-location-tracker.onrender.com";
    if (bot.sendMessage(CHAT_ID, message, "")) {
      Serial.println("Startup Telegram message sent");
      telegramStartupMessageSent = true;
    } else {
      Serial.println("Failed to send Telegram message at startup");
    }
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void loop() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
        Serial.println("Satellites: " + String(gps.satellites.value()));
        Serial.println("Lat: " + String(latitude, 6) + ", Lon: " + String(longitude, 6));
        updateOLED();

        if (wifiConnected) {
          sendToFlaskServer(latitude, longitude);
        } else {
          checkSMS(latitude, longitude);
        }
      } else {
        updateOLED();
      }
    }
  }

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println("No GPS detected");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("No GPS detected");
    display.display();
  }

  delay(1000);
}

void updateOLED() {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (gps.location.isValid()) {
    display.println("Lat: " + String(latitude, 6));
    display.println("Lon: " + String(longitude, 6));
  } else {
    display.println("Lat: No GPS");
    display.println("Lon: No GPS");
  }
  display.println("WiFi: " + String(wifiConnected ? "Connected" : "Disconnected"));
  display.println("Mode: " + String(wifiConnected ? "WiFi" : "SMS"));
  display.display();
}

void sendToFlaskServer(float lat, float lon) {
  if (lat == 0.0 || lon == 0.0 || lon >= 180.0 || lon <= -180.0) {
    Serial.println("Invalid coordinates, skipping POST: lat=" + String(lat, 6) + ", lon=" + String(lon, 6));
    return;
  }

  HTTPClient http;
  Serial.println("Sending POST to: " + String(serverUrl));
  http.setTimeout(15000);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "lat=" + String(lat, 6) + "&lon=" + String(lon, 6);
  Serial.println("Post Data: " + postData);
  int httpCode = http.POST(postData);

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Location sent to server");
    } else {
      Serial.println("Failed to send location, HTTP code: " + String(httpCode));
      Serial.println("Response: " + http.getString());
    }
  } else {
    Serial.println("Failed to connect to server, error: " + String(httpCode));
  }
  http.end();
}

void initSIM900A() {
  sim900a.println("AT");
  delay(100);
  sim900a.println("AT+CMGF=1");
  delay(100);
  sim900a.println("AT+CNMI=1,2,0,0,0");
  delay(2000);
}

void checkSMS(float lat, float lon) {
  if (sim900a.available()) {
    char c = sim900a.read();
    incomingData += c;

    if (incomingData.endsWith("\r\n")) {
      if (incomingData.indexOf("+CMT:") != -1) {
        int firstQuote = incomingData.indexOf("\"") + 1;
        int secondQuote = incomingData.indexOf("\"", firstQuote);
        if (firstQuote != -1 && secondQuote != -1 && secondQuote > firstQuote) {
          senderNumber = incomingData.substring(firstQuote, secondQuote);
        }
      } else {
        String msg = incomingData;
        msg.trim();
        msg.toLowerCase();

        if (msg.indexOf("get location") != -1 && senderNumber != "") {
          String googleMapsLink = "https://www.google.com/maps?q=" + String(lat, 6) + "," + String(lon, 6);
          String response = "Lat: " + String(lat, 6) + ", Lon: " + String(lon, 6) + "\n" + googleMapsLink;

          sendSMS(senderNumber, response);
          Serial.println("Sent location to " + senderNumber);
          senderNumber = "";
        }
      }

      incomingData = "";
    }
  }
}

void sendSMS(String number, String message) {
  sim900a.println("AT+CMGS=\"" + number + "\"");
  delay(500);
  sim900a.print(message);
  sim900a.write(26);
  delay(3000);
}