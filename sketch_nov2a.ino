/*
  ============================================================
                  SMART FARM - ESP32-CAM
  ============================================================
  FEATURES:
  - Listens to the same Telegram Bot as the main ESP32
  - Captures and sends photos on /photo command
  - Controls onboard flash LED on /light_on and /light_off
  ============================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ============================================================
//                    WIFI & BOT SETTINGS
// ============================================================
const char* WIFI_SSID     = "Hacker hidden  Network1";
const char* WIFI_PASSWORD = "smkr2010";

// Use the exact same token and Chat ID as your sensor ESP32
#define BOT_TOKEN "8762069596:AAH3IAHXxZeikw5YlNYijzy4KznwhvaXT4s"
String CHAT_ID = "8729191870";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

const unsigned long BOT_MTBS = 1000; 
unsigned long bot_lasttime = 0;

// ============================================================
//                 CAMERA PIN CONFIGURATION 
//                 (AI-THINKER MODEL)
// ============================================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED pin
#define FLASH_LED_PIN      4
bool flashState = false;

// ============================================================
//                  SEND PHOTO TO TELEGRAM
// ============================================================
String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  // Turn on flash momentarily if taking a dark photo (optional)
  // digitalWrite(FLASH_LED_PIN, HIGH);
  // delay(200);

  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }

  // digitalWrite(FLASH_LED_PIN, flashState ? HIGH : LOW); // Restore flash state

  Serial.println("Connecting to Telegram to send photo...");
  
  if (secured_client.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--SmartFarmBoundary\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--SmartFarmBoundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--SmartFarmBoundary--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    secured_client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
    secured_client.println("Host: " + String(myDomain));
    secured_client.println("Content-Length: " + String(totalLen));
    secured_client.println("Content-Type: multipart/form-data; boundary=SmartFarmBoundary");
    secured_client.println();
    secured_client.print(head);

    // Send image data in chunks to prevent memory crashes
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        secured_client.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        secured_client.write(fbBuf, remainder);
      }
    }  
    secured_client.print(tail);
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
      delay(100);      
      while (secured_client.available()) {
        char c = secured_client.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    secured_client.stop();
    Serial.println(getBody);
    return "Photo sent successfully.";
  }
  else {
    esp_camera_fb_return(fb);
    Serial.println("Connected to api.telegram.org failed.");
    return "Connection to Telegram failed.";
  }
}

// ============================================================
//                  HANDLE TELEGRAM COMMANDS
// ============================================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    
    if (chat_id != CHAT_ID) continue;

    Serial.print("CAM Node received: ");
    Serial.println(text);

    if (text == "/photo") {
      bot.sendMessage(chat_id, "📸 Capturing image...", "");
      sendPhotoTelegram();
    } 
    else if (text == "/light_on") {
      flashState = true;
      digitalWrite(FLASH_LED_PIN, HIGH);
      bot.sendMessage(chat_id, "💡 Camera Flash ON", "");
    } 
    else if (text == "/light_off") {
      flashState = false;
      digitalWrite(FLASH_LED_PIN, LOW);
      bot.sendMessage(chat_id, "💡 Camera Flash OFF", "");
    }
    // The ESP32-CAM silently ignores commands like /pump_on or /auto
    // because the main ESP32 will handle those.
  }
}

// ============================================================
//                           SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected.");

  secured_client.setInsecure();

  // Initialize Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // UXGA = 1600x1200
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  Serial.println("ESP32-CAM is Ready!");
}

// ============================================================
//                           LOOP
// ============================================================
void loop() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}