#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ==== WiFi & ThingSpeak Config ====
const char* WIFI_SSID = "iPhone";
const char* WIFI_PASS = "Password";
const char* THINGSPEAK_APIKEY = "UAR6CMB8N6TO63JN";
const char* THINGSPEAK_HOST = "http://api.thingspeak.com/update";

// ==== Pin Setup ====
#define SDA_PIN 21
#define SCL_PIN 22
#define LM35_PIN 36  // Analog input pin for LM35

// ==== LCD Setup ====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==== MAX30102 Sensor ====
MAX30105 particleSensor;
#define BUFFER_SIZE 100
uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

// ==== WiFi Connection ====
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi Connection Failed!");
  }
}

// ==== Accurate Sensor Reading ====
bool getSensorReading(int &outHR, int &outSpO2) {
  for (int i = 0; i < BUFFER_SIZE; i++) {
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i]  = particleSensor.getIR();
    delay(20); // smoother data
  }

  int32_t spo2;
  int8_t validSPO2;
  int32_t heartRate;
  int8_t validHeartRate;

  maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer,
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);

  if (validHeartRate && validSPO2) {
    outHR = heartRate;
    outSpO2 = spo2;
    return true;
  }
  return false;
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // MAX30102 Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found. Check wiring!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MAX30102 Error!");
    while (1);
  }
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);

  connectWiFi();

  lcd.clear();
  lcd.print("System Ready");
  delay(1000);
}

// ==== Main Loop ====
void loop() {
  // Temperature from LM35
  int raw = analogRead(LM35_PIN);
  float voltage = (raw / 4095.0) * 3.3;
  float tempC = voltage * 100.0;  // 10mV/°C

  // Heart Rate & SpO₂
  int hr = 0, sp = 0;
  bool ok = getSensorReading(hr, sp);

  // LCD Display
  lcd.clear();
  if (ok) {
    lcd.setCursor(0, 0);
    lcd.print("HR:");
    lcd.print(hr);
    lcd.print(" SpO2:");
    lcd.print(sp);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Reading Sensor...");
  }

  lcd.setCursor(0, 1);
  lcd.print("Temp:");
  lcd.print(tempC, 1);
  lcd.print((char)223);
  lcd.print("C");

  // Serial Monitor
  Serial.print("HR: ");
  Serial.print(hr);
  Serial.print(" | SpO2: ");
  Serial.print(sp);
  Serial.print(" | Temp: ");
  Serial.println(tempC);

  // Upload to ThingSpeak (only if valid data)
  if (WiFi.status() == WL_CONNECTED && ok) {
    HTTPClient http;
    String url = String(THINGSPEAK_HOST) + "?api_key=" + THINGSPEAK_APIKEY +
                 "&field1=" + String(hr) +
                 "&field2=" + String(sp) +
                 "&field3=" + String(tempC, 1);
    http.begin(url);
    int code = http.GET();
    Serial.println("ThingSpeak Response: " + String(code));
    http.end();
  }

  delay(15000); // 15 sec update interval
}
