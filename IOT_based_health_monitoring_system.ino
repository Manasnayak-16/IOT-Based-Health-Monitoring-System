// ESP32 Patient Health Monitoring
// MAX30102 (HR/SpO2) + LM35 (Temp) + LCD + ThingSpeak
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

///// Config /////
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* THINGSPEAK_APIKEY = "YOUR_THINGSPEAK_API_KEY";
const char* THINGSPEAK_HOST = "http://api.thingspeak.com/update";

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const int LM35_PIN = 36;  // GPIO36 (ADC1_CH0) on ESP32
const int BUFFER_SIZE = 100; // recommended for ESP32

///// Hardware objects /////
MAX30105 particleSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2);

///// Algorithm buffers /////
uint16_t irBuffer[BUFFER_SIZE];
uint16_t redBuffer[BUFFER_SIZE];
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

///// Reliable reading wrapper /////
bool getReliableReading(int &outHR, int &outSPO2) {
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    irBuffer[i] = particleSensor.getIR();
    redBuffer[i] = particleSensor.getRed();
    delay(20); // ~50Hz sampling
  }

  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, BUFFER_SIZE, redBuffer,
    &spo2, &validSPO2, &heartRate, &validHeartRate
  );

  if (validHeartRate && validSPO2) {
    outHR = (int)heartRate;
    outSPO2 = (int)spo2;
    return true;
  }
  return false;
}

void wifiConnect() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500); Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi connect failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // I2C and LCD init
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Starting...");

  // start MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found. Check wiring.");
    lcd.setCursor(0,1); lcd.print("MAX30102 fail");
    while (1) delay(1000);
  }
  particleSensor.setup(); // default setup
  particleSensor.setPulseAmplitudeRed(0x1F);
  particleSensor.setPulseAmplitudeIR(0x1F);

  wifiConnect();
  lcd.clear();
  lcd.print("Ready");
  delay(500);
}

void loop() {
  // read LM35 temperature
  int raw = analogRead(LM35_PIN);      
  float voltage = (raw / 4095.0) * 3.3; // ESP32 ADC 12-bit
  float tempC = voltage * 100.0;       

  // get HR & SpO2
  int hr = 0, sp = 0;
  bool ok = getReliableReading(hr, sp);

  // LCD output
  lcd.setCursor(0,0);
  if (ok) {
    lcd.printf("HR:%3d SpO2:%2d", hr, sp);
  } else {
    lcd.print("Measuring...     ");
  }
  lcd.setCursor(0,1);
  lcd.printf("T:%4.1fC           ", tempC);

  // Serial monitor
  Serial.print("Temp: "); Serial.print(tempC,1);
  Serial.print(" C, HR: "); Serial.print(ok?hr:-1);
  Serial.print(" bpm, SpO2: "); Serial.println(ok?sp:-1);

  // ThingSpeak upload
  if (WiFi.status() == WL_CONNECTED && ok) {
    HTTPClient http;
    String url = String(THINGSPEAK_HOST) + "?api_key=" + THINGSPEAK_APIKEY +
                 "&field1=" + String(hr) +
                 "&field2=" + String(sp) +
                 "&field3=" + String(tempC,1);
    http.begin(url);
    int code = http.GET();
    Serial.print("ThingSpeak HTTP code: "); Serial.println(code);
    http.end();
  }

  delay(15000); // ThingSpeak requires ≥15s between updates
}
