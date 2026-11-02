/*
 ============================================================
                    SMART FARM - ESP32
 ============================================================
*/

// -------------------- LIBRARIES --------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ============================================================
//                    WIFI SETTINGS
// ============================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ============================================================
//                  TELEGRAM SETTINGS
// ============================================================
// I have removed your token for security. Please paste your active token here.
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
String CHAT_ID = "YOUR_CHAT_ID";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ============================================================
//                       PIN SETUP
// ============================================================
#define DHT_PIN       4
#define DHT_TYPE      DHT11

#define LDR_PIN       32
#define SOIL1_PIN     34
#define SOIL2_PIN     35
#define WATER_PIN     33
#define PH_PIN        39
#define RELAY_PIN     26

// ============================================================
//                       DHT OBJECT
// ============================================================
DHT dht(DHT_PIN, DHT_TYPE);

// ============================================================
//                    RELAY SETTINGS
// ============================================================
#define RELAY_ON      LOW
#define RELAY_OFF     HIGH

// ============================================================
//                  PUMP / AUTO SETTINGS
// ============================================================
int SOIL_PUMP_ON_LEVEL = 35;
int SOIL_PUMP_OFF_LEVEL = 60;

// ============================================================
//                 SENSOR CALIBRATION
// ============================================================
// Normal (Resistive) Soil sensors usually read around 4095 when dry, 
// and around 2000-2500 when wet in actual soil.
int SOIL1_DRY = 4095;
int SOIL1_WET = 2200; 

int SOIL2_DRY = 4095;
int SOIL2_WET = 2200;

// LDR calibration
int LDR_DARK = 4095;
int LDR_BRIGHT = 500;

// Water tank calibration
int WATER_EMPTY = 0;
int WATER_FULL = 4095;

// pH calibration
float PH_OFFSET = 0.00;

// ============================================================
//                    VARIABLES
// ============================================================
bool autoMode = true;
bool pumpState = false;
unsigned long lastSensorSend = 0;
const unsigned long SENSOR_INTERVAL = 60000; // 1 minute
unsigned long lastTelegramCheck = 0;
const unsigned long TELEGRAM_INTERVAL = 1000;

// ============================================================
//                  SENSOR VALUES
// ============================================================
float temperature = 0;
float humidity = 0;
int soil1 = 0;
int soil2 = 0;
float phValue = 0;
int sunlight = 0;
String brightness = "LOW";
int tankLevel = 0;

// ============================================================
//                 RELAY / PUMP CONTROL
// ============================================================
void pumpON() {
  digitalWrite(RELAY_PIN, RELAY_ON);
  pumpState = true;
}

void pumpOFF() {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pumpState = false;
}

// ============================================================
//              CONVERT ADC TO PERCENTAGE
// ============================================================
int convertPercentage(int value, int dryValue, int wetValue) {
  int percent = map(value, dryValue, wetValue, 0, 100);
  percent = constrain(percent, 0, 100);
  return percent;
}

// ============================================================
//                  READ SOIL SENSOR 1
// ============================================================
int readSoil1() {
  int raw = analogRead(SOIL1_PIN);
  return convertPercentage(raw, SOIL1_DRY, SOIL1_WET);
}

// ============================================================
//                  READ SOIL SENSOR 2
// ============================================================
int readSoil2() {
  int raw = analogRead(SOIL2_PIN);
  return convertPercentage(raw, SOIL2_DRY, SOIL2_WET);
}

// ============================================================
//                       READ LDR
// ============================================================
int readLDR() {
  int raw = analogRead(LDR_PIN);
  int percentage = map(raw, LDR_DARK, LDR_BRIGHT, 0, 100);
  return constrain(percentage, 0, 100);
}

// ============================================================
//                 BRIGHTNESS DESCRIPTION
// ============================================================
String getBrightness(int value) {
  if (value >= 70) return "HIGH";
  else if (value >= 35) return "MEDIUM";
  else return "LOW";
}

// ============================================================
//                    READ WATER LEVEL
// ============================================================
int readWaterLevel() {
  int raw = analogRead(WATER_PIN);
  int percentage = map(raw, WATER_EMPTY, WATER_FULL, 0, 100);
  return constrain(percentage, 0, 100);
}

// ============================================================
//                       READ pH
// ============================================================
float readPH() {
  const int samples = 20;
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(PH_PIN);
    delay(5);
  }
  float raw = total / (float)samples;
  float voltage = raw * 3.3 / 4095.0;
  float ph = 7.0 + ((2.50 - voltage) / 0.18);
  ph += PH_OFFSET;
  return constrain(ph, 0, 14);
}

// ============================================================
//                  READ ALL SENSORS (UPDATED FOR CALIBRATION)
// ============================================================
void readAllSensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  soil1 = readSoil1();
  soil2 = readSoil2();
  phValue = readPH();
  sunlight = readLDR();
  brightness = getBrightness(sunlight);
  tankLevel = readWaterLevel();

  // ----------------------------------------------------------
  // Serial monitor
  // ----------------------------------------------------------
  Serial.println();
  Serial.println("==============================");
  Serial.println("      SMART FARM DATA");
  Serial.println("==============================");

  Serial.print("Temperature : ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity    : ");
  Serial.print(humidity);
  Serial.println(" %");

  // PRINTING RAW VALUES SO YOU CAN CALIBRATE
  Serial.print("Soil 1      : ");
  Serial.print(soil1);
  Serial.print(" %   (Raw ADC: ");
  Serial.print(analogRead(SOIL1_PIN));
  Serial.println(")");

  Serial.print("Soil 2      : ");
  Serial.print(soil2);
  Serial.print(" %   (Raw ADC: ");
  Serial.print(analogRead(SOIL2_PIN));
  Serial.println(")");

  Serial.print("pH          : ");
  Serial.println(phValue);

  Serial.print("Sunlight    : ");
  Serial.print(sunlight);
  Serial.println(" %");

  Serial.print("Brightness  : ");
  Serial.println(brightness);

  Serial.print("Tank Level  : ");
  Serial.print(tankLevel);
  Serial.print(" %   (Raw ADC: ");
  Serial.print(analogRead(WATER_PIN));
  Serial.println(")");

  Serial.print("Pump        : ");
  if (pumpState) Serial.println("ON");
  else Serial.println("OFF");

  Serial.print("Mode        : ");
  if (autoMode) Serial.println("AUTO");
  else Serial.println("MANUAL");

  Serial.println("==============================");
}

// ============================================================
//                  AUTOMATIC PUMP CONTROL
// ============================================================
void automaticPumpControl() {
  if (!autoMode) return;

  int averageSoil = (soil1 + soil2) / 2;

  Serial.print("Average Soil Moisture: ");
  Serial.print(averageSoil);
  Serial.println(" %");

  if (tankLevel <= 5) {
    pumpOFF();
    Serial.println("Tank almost empty - Pump OFF");
    return;
  }

  if (averageSoil <= SOIL_PUMP_ON_LEVEL) {
    pumpON();
    Serial.println("Soil DRY - Pump ON");
  } 
  else if (averageSoil >= SOIL_PUMP_OFF_LEVEL) {
    pumpOFF();
    Serial.println("Soil moisture reached target - Pump OFF");
  }
}

// ============================================================
//                  CREATE TELEGRAM STATUS
// ============================================================
String createStatusMessage() {
  String message = "🌱 SMART FARM STATUS\n\n";

  message += "🌡 Temperature : ";
  if (isnan(temperature)) message += "ERROR";
  else message += String(temperature, 1) + " °C";
  message += "\n  வெப்பநிலை : " + (isnan(temperature) ? "ERROR" : String(temperature, 1) + " °C");

  message += "\n\n💧 Humidity : ";
  if (isnan(humidity)) message += "ERROR";
  else message += String(humidity, 1) + " %";
  message += "\n  ஈரப்பதம் : " + (isnan(humidity) ? "ERROR" : String(humidity, 1) + " %");

  message += "\n\n🌱 Soil Moisture 1 : " + String(soil1) + " %";
  message += "\n  மண் ஈரப்பதம் 1 : " + String(soil1) + " %";

  message += "\n\n🌱 Soil Moisture 2 : " + String(soil2) + " %";
  message += "\n  மண் ஈரப்பதம் 2 : " + String(soil2) + " %";

  message += "\n\n🧪 pH Level : " + String(phValue, 2);
  message += "\n  மண்ணின் pH : " + String(phValue, 2);

  message += "\n\n☀️ Sunlight : " + String(sunlight) + " %";
  message += "\n  சூரிய ஒளி : " + String(sunlight) + " %";

  message += "\n\n💡 Brightness : " + brightness;
  message += "\n  வெளிச்சம் : ";
  if (brightness == "HIGH") message += "அதிகம்";
  else if (brightness == "MEDIUM") message += "நடுத்தரம்";
  else message += "குறைவு";

  message += "\n\n🚰 Tank Level : " + String(tankLevel) + " %";
  message += "\n  தொட்டி நீர் அளவு : " + String(tankLevel) + " %";

  message += "\n\n💦 Pump : " + String(pumpState ? "ON" : "OFF");
  message += "\n  மோட்டார் : " + String(pumpState ? "ON" : "OFF");

  message += "\n\n⚙️ Mode : " + String(autoMode ? "AUTO" : "MANUAL");
  message += "\n  செயல் முறை : " + String(autoMode ? "AUTO" : "MANUAL");

  message += "\n\n⏱ Updated : " + String(millis() / 1000) + " sec";

  return message;
}

// ============================================================
//                 SEND STATUS TO TELEGRAM
// ============================================================
void sendStatus() {
  readAllSensors();
  automaticPumpControl();
  String message = createStatusMessage();
  bot.sendMessage(CHAT_ID, message, "");
}

// ============================================================
//                  TELEGRAM KEYBOARD
// ============================================================
String telegramKeyboard() {
  String keyboard = "[";
  keyboard += "[{\"text\":\"💦 Pump ON\",\"callback_data\":\"pump_on\"},";
  keyboard += "{\"text\":\"🛑 Pump OFF\",\"callback_data\":\"pump_off\"}],";
  keyboard += "[{\"text\":\"⚙️ AUTO\",\"callback_data\":\"auto\"},";
  keyboard += "{\"text\":\"🔧 MANUAL\",\"callback_data\":\"manual\"}],";
  keyboard += "[{\"text\":\"📊 STATUS\",\"callback_data\":\"status\"}]";
  keyboard += "]";
  return keyboard;
}

// ============================================================
//                    SEND MAIN MENU
// ============================================================
void sendMainMenu() {
  String text = "🌱 SMART FARM\n\nSelect an option:";
  bot.sendMessageWithInlineKeyboard(CHAT_ID, text, "", telegramKeyboard());
}

// ============================================================
//                 HANDLE TELEGRAM COMMANDS
// ============================================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "❌ Unauthorized user", "");
      continue;
    }

    Serial.print("Telegram command: ");
    Serial.println(text);

    if (text == "/start") {
      sendMainMenu();
    } 
    else if (text == "/status" || text == "/Status") {
      sendStatus();
    } 
    else if (text == "/pump_on" || text == "pump_on") {
      autoMode = false;
      if (tankLevel > 5) {
        pumpON();
        bot.sendMessage(CHAT_ID, "💦 Pump ON\nமோட்டார் இயக்கப்பட்டது", "");
      } else {
        pumpOFF();
        bot.sendMessage(CHAT_ID, "⚠️ Tank water is too low!\nதொட்டியில் தண்ணீர் குறைவாக உள்ளது.", "");
      }
    } 
    else if (text == "/pump_off" || text == "pump_off") {
      pumpOFF();
      bot.sendMessage(CHAT_ID, "🛑 Pump OFF\nமோட்டார் நிறுத்தப்பட்டது", "");
    } 
    else if (text == "/auto" || text == "auto") {
      autoMode = true;
      bot.sendMessage(CHAT_ID, "⚙️ AUTO MODE ON\nதானியங்கி முறை இயக்கப்பட்டது", "");
      readAllSensors();
      automaticPumpControl();
    } 
    else if (text == "/manual" || text == "manual") {
      autoMode = false;
      bot.sendMessage(CHAT_ID, "🔧 MANUAL MODE ON\nகைமுறை முறை இயக்கப்பட்டது", "");
    } 
    else {
      bot.sendMessage(CHAT_ID, "❓ Unknown command\n\n/status\n/pump_on\n/pump_off\n/auto\n/manual", "");
    }
  }
}

// ============================================================
//                  CONNECT WIFI
// ============================================================
void connectWiFi() {
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
//                         SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RELAY_PIN, OUTPUT);
  pumpOFF();

  analogReadResolution(12);
  dht.begin();
  connectWiFi();
  secured_client.setInsecure();

  Serial.println("\n==============================");
  Serial.println("      SMART FARM READY");
  Serial.println("==============================");

  readAllSensors();
  automaticPumpControl();

  bot.sendMessage(CHAT_ID, "🌱 SMART FARM STARTED\nஸ்மார்ட் ஃபார்ம் தொடங்கப்பட்டது", "");
  sendMainMenu();
}

// ============================================================
//                          LOOP
// ============================================================
void loop() {
  if (millis() - lastTelegramCheck > TELEGRAM_INTERVAL) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTelegramCheck = millis();
  }

  if (millis() - lastSensorSend >= SENSOR_INTERVAL) {
    lastSensorSend = millis();
    readAllSensors();
    automaticPumpControl();
    String message = createStatusMessage();
    bot.sendMessage(CHAT_ID, message, "");
  }

  if (autoMode) {
    static unsigned long lastAutoCheck = 0;
    if (millis() - lastAutoCheck >= 5000) {
      lastAutoCheck = millis();
      readAllSensors();
      automaticPumpControl();
    }
  }

  delay(10);
}