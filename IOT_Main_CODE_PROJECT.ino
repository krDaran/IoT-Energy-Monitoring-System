// ===== Blynk Credentials =====
#define BLYNK_TEMPLATE_ID "TMPL3Wyp6t_dW"
#define BLYNK_TEMPLATE_NAME "Energy Monitor"
#define BLYNK_AUTH_TOKEN "bDptAaQYu-PPldQLzMuqaeoC_JJ4hO0a"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ===== WiFi =====
char ssid[] = "Yuvan Karthick's S24";
char pass[] = "17102008";

// ===== Pins =====
#define CURRENT_PIN 34
#define VOLTAGE_PIN 35
#define RELAY_PIN 26

// ===== Calibration =====
float offset = 2.35; // negating resting voltage
float sensitivity = 0.185; // how much sensor output voltage change per ampere of current
float calibrationFactor = 0.14; // negating noise in current sensor
float voltageFactor = 6.7; // negating noise in voltage sensor

// ===== Energy + Cost =====
float energy_Wh = 0;
float realEnergy_Wh = 0;
float tariff = 200.0;

// ===== Budget =====
float budget = 10.0;

// ===== Prediction =====
unsigned long startTime = 0;

// ===== Alerts =====
bool alert50 = false;
bool alert80 = false;
bool alert100 = false;

// ===== Manual Control =====
bool manualRelayState = true;

// ===== Timer =====
BlynkTimer timer; //creating a clock object


// ===== Read Current =====
float readCurrent() {
  int samples = 200;
  float sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(2);
  }

  float adc = sum / samples;
  float voltage = (adc / 4095.0) * 3.3;

  float current = (voltage - offset) / sensitivity;
  current = current * 1000;
  current = current * calibrationFactor;

  if (current < 0) current = -current; // to prevent reverse wiring mishap and turn opp connection to right one
  if (current < 10) current = 0;

  return current;
}


// ===== Read Voltage =====
float readVoltage() {
  int samples = 200;
  float sum = 0;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(VOLTAGE_PIN);
    delay(2);
  }

  float adc = sum / samples;
  float voltage = (adc / 4095.0) * 3.3;

  return voltage * voltageFactor;
}


// ===== Receive Budget =====
BLYNK_WRITE(V5) {
  budget = param.asFloat();

  alert50 = false;
  alert80 = false;
  alert100 = false;

  startTime = millis();
  energy_Wh = 0;
  realEnergy_Wh = 0;
}


// ===== Manual Relay Button =====
BLYNK_WRITE(V7) {
  manualRelayState = param.asInt();
}


// ===== Main Logic =====
void sendData() {

  // ===== COST CALCULATION =====
  float energy_kWh = energy_Wh / 1000.0;
  float cost = energy_kWh * tariff;

  // ===== RELAY CONTROL =====
  bool relayON;

  // Priority 1: Budget cutoff
  if (budget > 0 && cost >= budget) {
    relayON = false;
  }
  // Priority 2: Manual control
  else {
    relayON = manualRelayState;
  }

  // Apply relay (ACTIVE LOW)
  if (relayON) {
    digitalWrite(RELAY_PIN, LOW);   // ON
  } else {
    digitalWrite(RELAY_PIN, HIGH);  // OFF
  }

  // ===== SENSOR READ =====
  float current_mA = relayON ? readCurrent() : 0;
  float voltage = relayON ? readVoltage() : 0;

  float currentA = current_mA / 1000.0;
  float power = voltage * currentA;

  // ===== ENERGY =====
  energy_Wh += (power / 3600.0) * 300;   // fast demo
  realEnergy_Wh += (power / 3600.0);

  // Update cost again after new energy
  energy_kWh = energy_Wh / 1000.0;
  cost = energy_kWh * tariff;

  // ===== Prediction =====
  float elapsedHours = (millis() - startTime) / 3600000.0;
  if (elapsedHours < 0.01) elapsedHours = 0.01;

  float usageRate = realEnergy_Wh / elapsedHours;
  float dailyUsage = usageRate * 24;

  float maxEnergy = (budget / tariff) * 1000.0;
  float remainingEnergy = maxEnergy - realEnergy_Wh;

  float daysLeft = remainingEnergy / dailyUsage;

  if (daysLeft > 31) daysLeft = 31;
  if (daysLeft < 0) daysLeft = 0;

  // ===== Alerts =====
  if (budget > 0) {
    float usagePercent = (cost / budget) * 100.0;

    if (usagePercent >= 50 && !alert50) {
      Blynk.logEvent("alert", "⚠️ 50% budget used");
      alert50 = true;
    }

    if (usagePercent >= 80 && !alert80) {
      Blynk.logEvent("alert", "⚠️ High usage (80%)");
      alert80 = true;
    }

    if (usagePercent >= 100 && !alert100) {
      Blynk.logEvent("alert", "❌ Budget exceeded!");
      alert100 = true;
    }
  }

  // ===== DEBUG =====
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.print(" V | Current: ");
  Serial.print(current_mA);
  Serial.print(" mA | Energy: ");
  Serial.print(energy_Wh, 3);
  Serial.print(" Wh | Cost: ₹");
  Serial.print(cost, 2);
  Serial.print(" | Days Left: ");
  Serial.println(daysLeft, 2);

  // ===== Blynk =====
  Blynk.virtualWrite(V0, current_mA);
  Blynk.virtualWrite(V1, voltage);
  Blynk.virtualWrite(V3, energy_Wh);
  Blynk.virtualWrite(V4, cost);
  Blynk.virtualWrite(V6, daysLeft);
}


// ===== Setup =====
void setup() {
  Serial.begin(115200);

  startTime = millis();

  analogSetAttenuation(ADC_11db);

  pinMode(RELAY_PIN, OUTPUT);

  // FORCE relay ON at startup
  manualRelayState = true;
  digitalWrite(RELAY_PIN, LOW);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  

  timer.setInterval(1000L, sendData);
}


// ===== Loop =====
void loop() {
  Blynk.run();
  timer.run();
}