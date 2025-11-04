#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "ThingSpeak.h"

// ---------- OLED SETUP ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- MAX30102 SENSOR ----------
MAX30105 particleSensor;

// ---------- ESP32 I2C PINS ----------
#define I2C_SDA 21
#define I2C_SCL 22

// ---------- BUFFER AND ALGORITHM SETTINGS ----------
#define BUFFER_SIZE 100
#define AVG_SIZE 5
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

// ---------- RESULTS ----------
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHR;

// ---------- MOVING AVERAGE ----------
int hrHistory[AVG_SIZE];
int spo2History[AVG_SIZE];
int hrIndex = 0;
int spo2Index = 0;

// ---------- WiFi + ThingSpeak ----------
const char* ssid = "WIFI SSID";       // 🔹 Change this
const char* password = "PASSWORD"; // 🔹 Change this
unsigned long myChannelNumber =CHANEL ID;     // 🔹 Change this
const char* myWriteAPIKey = "API KEY";  // 🔹 Change this

WiFiClient client;

// ---------- Function to calculate average ----------
int getAverage(int* array, int size) {
  long sum = 0;
  int count = 0;
  for (int i = 0; i < size; i++) {
    if (array[i] > 0) {
      sum += array[i];
      count++;
    }
  }
  return (count > 0) ? (sum / count) : 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL, 400000);

  // OLED initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED not found!");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.println("Initializing...");
  display.display();

  // MAX30102 initialization
  if (!particleSensor.begin(Wire, 400000)) {
    Serial.println("❌ MAX30102 not found!");
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println("Sensor not found!");
    display.display();
    while (1);
  }

  // Configure sensor
  particleSensor.setup(200, 8, 2, 100, 411, 16384);

  // OLED display labels
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print("HR:");
  display.setCursor(0, 35);
  display.print("SpO2:");
  display.display();

  // WiFi connection
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.println("WiFi Connecting...");
  display.display();

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    display.setCursor(0, 55);
    display.print("WiFi OK ");
    display.display();
  } else {
    Serial.println("\n⚠️ WiFi Connection Failed!");
    display.setCursor(0, 55);
    display.print("WiFi Fail ");
    display.display();
  }

  ThingSpeak.begin(client);
  delay(1000);
}

void loop() {
  const int measureTime = 20000; // 20 seconds per cycle
  unsigned long startTime = millis();

  long sumHR = 0;
  long sumSpO2 = 0;
  int countValidHR = 0;
  int countValidSpO2 = 0;

  for (int i = 0; i < AVG_SIZE; i++) {
    hrHistory[i] = 0;
    spo2History[i] = 0;
  }
  hrIndex = 0;
  spo2Index = 0;

  while (millis() - startTime < measureTime) {
    // Shift buffer
    for (int i = 0; i < BUFFER_SIZE - 1; i++) {
      redBuffer[i] = redBuffer[i + 1];
      irBuffer[i] = irBuffer[i + 1];
    }

  }

  // Compute final averages
  int avgHR = (countValidHR > 0) ? (sumHR / countValidHR) : -1;
  int avgSpO2 = (countValidSpO2 > 0) ? (sumSpO2 / countValidSpO2) : -1;

  Serial.print("Average HR: ");
  if (avgHR > 0) Serial.print(avgHR);
  else Serial.print("---");
  Serial.print(" bpm | Average SpO2: ");
  if (avgSpO2 > 0) Serial.print(avgSpO2);
  else Serial.print("---");
  Serial.println(" %");

  // Update OLED display
  display.fillRect(40, 10, 80, 20, SSD1306_BLACK);
  display.setCursor(40, 10);
  if (avgHR > 0) display.print(avgHR);
  else display.print("--");

  display.fillRect(60, 35, 80, 20, SSD1306_BLACK);
  display.setCursor(60, 35);
  if (avgSpO2 > 0) display.print(avgSpO2);
  else display.print("--");

  display.display();

  // ---------- Upload to ThingSpeak ----------
  if (WiFi.status() == WL_CONNECTED) {
    ThingSpeak.setField(1, avgHR);
    ThingSpeak.setField(2, avgSpO2);

   
  } else {
    Serial.println("⚠️ Skipped upload (No WiFi)");
  }

  delay(15000); // Wait before next reading cycle
}
