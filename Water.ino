#define BLYNK_TEMPLATE_ID "TMPL3zx1VUat9"
#define BLYNK_TEMPLATE_NAME "Plant Watering System"
#define BLYNK_AUTH_TOKEN "hq49JAaHmFxMDJnjzM_79uY-NPL5MhPQ"
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Auth token
char auth[] = "hq49JAaHmFxMDJnjzM_79uY-NPL5MhPQ";

// Wi-Fi credentials for two networks
char ssid1[] = "CampusHouse TN";
char pass1[] = "password";

char ssid2[] = "Abhilash’s iPhone";
char pass2[] = "aaaaaaaa";

BlynkTimer timer;
bool Relay = 0;
bool isAutomatic = true;         // Default mode: automatic
bool watering = false;
bool motorJustTurnedOff = false; // To track motor state changes

// Define component pins
#define sensor A0       // Pin for capacitive soil moisture sensor
#define waterPump D5    // Pin for water pump relay

// Variables for automatic watering
unsigned long lastPumpTime = 0;
const unsigned long pumpInterval = 10000;        // 10 seconds
const unsigned long dataUpdateInterval = 120000; // 2 minutes
unsigned long lastDataUpdateTime = 0;
int moisturePercent = 0;

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid1, pass1);
    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nPrimary Wi-Fi failed. Connecting to secondary Wi-Fi...");
      WiFi.begin(ssid2, pass2);
      startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to Wi-Fi!");
      Blynk.config(auth, "blynk.cloud", 80);
      while (Blynk.connect() == false) {}
    } else {
      Serial.println("\nFailed to connect to Wi-Fi!");
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(waterPump, OUTPUT);
  digitalWrite(waterPump, LOW); // Ensure pump is off initially

  connectToWiFi();
  // Call the function to read soil moisture periodically
  timer.setInterval(3000L, soilMoistureSensor);
  timer.setInterval(100L, checkAutomaticWatering);
}

// Handle manual control of the pump from the Blynk app
BLYNK_WRITE(V1) {
  Relay = param.asInt();

  if (Relay == 1) {
    digitalWrite(waterPump, HIGH);
    Serial.println("Motor is ON");
  } else {
    digitalWrite(waterPump, LOW);
    Serial.println("Motor is OFF");
  }
}

// Toggle automatic/manual mode from the Blynk app
BLYNK_WRITE(V2) {
  isAutomatic = param.asInt();
  if (!isAutomatic) {
    watering = false;
    digitalWrite(waterPump, LOW);
    Serial.println("Switched to Manual Mode.");
  } else {
    Serial.println("Switched to Automatic Mode.");
  }
}

// Handle refresh button to send immediate soil moisture data
BLYNK_WRITE(V3) {
  int refresh = param.asInt(); // Read button state
  if (refresh == 1) {         // Button pressed
    int rawValue = readSoilMoistureMedian();
    int refreshMoisturePercent = map(rawValue, 770, 355, 0, 100);
    refreshMoisturePercent = constrain(refreshMoisturePercent, 0, 100);

    // Send the moisture data to Blynk immediately
    Blynk.virtualWrite(V0, refreshMoisturePercent);

    Serial.print("Refreshed Moisture: ");
    Serial.print(refreshMoisturePercent);
    Serial.println("%");
    Blynk.virtualWrite(V3, 0);
  }
}

// Read soil moisture values
void soilMoistureSensor() {
  int rawValue = readSoilMoistureMedian();
  moisturePercent = map(rawValue, 770, 355, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  unsigned long currentTime = millis();

  // Only send data to Blynk if 3 minutes have passed or the motor just turned off
  if (currentTime - lastDataUpdateTime >= dataUpdateInterval || motorJustTurnedOff) {
    Blynk.virtualWrite(V0, moisturePercent);
    Serial.print("Uploaded Moisture: ");
    Serial.print(moisturePercent);
    Serial.println("%");

    lastDataUpdateTime = currentTime;
    motorJustTurnedOff = false; // Reset the flag after update
  } else {
    Serial.print("Moisture (Local): ");
    Serial.print(moisturePercent);
    Serial.println("%");
  }
}

int readSoilMoistureMedian() {
  const int numReadings = 5;  // Number of readings for the median
  int readings[numReadings];

  for (int i = 0; i < numReadings; i++) {
    readings[i] = analogRead(sensor);
    delay(10);
  }

  // Sort the readings
  for (int i = 0; i < numReadings - 1; i++) {
    for (int j = i + 1; j < numReadings; j++) {
      if (readings[i] > readings[j]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  // Return the median value
  return readings[numReadings / 2];
}

// Non-blocking automatic watering check
void checkAutomaticWatering() {
  if (isAutomatic && !Relay && moisturePercent < 60 && !watering) {
    watering = true;
    lastPumpTime = millis();
    Serial.println("Automatic watering started.");
  }

  if (watering) {
    unsigned long currentTime = millis();
    if (moisturePercent >= 80) {
      watering = false;
      digitalWrite(waterPump, LOW);
      motorJustTurnedOff = true; // Set flag to trigger data upload
      Serial.println("Automatic watering stopped. Soil moisture level sufficient.");
    } else if (currentTime - lastPumpTime >= pumpInterval) {
      digitalWrite(waterPump, HIGH);
      delay(1000);
      digitalWrite(waterPump, LOW);
      Serial.println("Pump activated. Rechecking moisture...");
      lastPumpTime = currentTime;
    }
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    connectToWiFi();
  }

  Blynk.run();
  timer.run();
}