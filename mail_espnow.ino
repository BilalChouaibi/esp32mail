#include <Arduino.h>
#include <ESP_Mail_Client.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPIFFS.h>

// Constantes
#define BATTERY_PIN 35
#define TRIGGER_PIN 14
#define ECHO_PIN 12
#define MEASURE_TIMEOUT 25000UL
#define SOUND_SPEED 340.0 / 1000
#define WIFI_SSID "UFI-SPRINT"
#define WIFI_PASSWORD "SvCn5%pE92@GvL"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587
#define AUTHOR_EMAIL "sigmasnec@gmail.com"
#define AUTHOR_PASSWORD "hsonlwhmaaoemkid"
#define RECIPIENT_EMAIL "95500.BD@gmail.com"

// Structures
esp_now_peer_info_t receiverInfo;
typedef struct {
  int id;
  int value;
  int battery;
} SensorData;

// Variables
bool sending = false;
SMTPSession smtp;
SensorData myData;

// Adresse MAC du récepteur
uint8_t MAC_recepteur[] = {0x08, 0x3A, 0x8D, 0x2F, 0x14, 0x20};

// Variables pour l'envoi du mail
bool emailSent = false; // Indicateur de l'envoi du courrier électronique
int emailInterval = 4320; // nombre de reboot min pour envoi email (équivalent 1 jour)
RTC_DATA_ATTR int lastEmailTime = 4320; // nombre de reboot sans envoi de mail

// Fonctions
void onSendData(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast packet sent:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed to send to server");
}

void initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  esp_now_register_send_cb(onSendData);
  memcpy(receiverInfo.peer_addr, MAC_recepteur, 6);
  receiverInfo.channel = 0;
  receiverInfo.encrypt = false;

  if (esp_now_add_peer(&receiverInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

int measureDistance() {
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
  long measure = pulseIn(ECHO_PIN, HIGH, MEASURE_TIMEOUT);
  float distance_mm = measure / 2.0 * SOUND_SPEED;
  Serial.print(F("Distance: "));
  int distance_cm = distance_mm / 10.0;
  Serial.print(distance_mm / 10.0, 2);
  Serial.print(F("cm, "));
  delay(500);
  return distance_cm;
}

void sendEmail(const char *webpage) {
  MailClient.networkReconnect(true);
  Session_Config config;
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = F("mydomain.net");
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  smtp.connect(&config);
  SMTP_Message email;
  email.sender.name = F("ESP Mail");
  email.sender.email = AUTHOR_EMAIL;
  email.subject = F("Sprint");
  email.addRecipient(F("Someone"), RECIPIENT_EMAIL);
  email.html.content = webpage; // Utilisation de la propriété html.content pour inclure une page web au lieu du texte
  email.html.charSet = F("utf-8");
  email.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!MailClient.sendMail(&smtp, &email)) {
    ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
  }
}

void setup() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  pinMode(BATTERY_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(115200);

  connectToWiFi();
  initESPNow();
}

void loop() {
  myData.battery = analogRead(BATTERY_PIN) * 100 / 2500;

  if (myData.battery >= 100) {
    myData.battery = 100;
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println(myData.battery);

  myData.id = 1;
  myData.value = measureDistance();

  esp_err_t result = esp_now_send(MAC_recepteur, (uint8_t *)&myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("Value update successful");
    Serial.print("board id:");
    Serial.println(myData.id);
  } else {
    Serial.println("Failed to send to server");
  }
  
  if (myData.value >= 35 && !emailSent || lastEmailTime >= emailInterval)  {
    Serial.println("**********envoi du mail**********");
    String htmlContent = loadHTMLFromFile("/index.html");
    sendEmail(htmlContent.c_str());
    emailSent = true;
    lastEmailTime = 0;
  }
  else {
    emailSent = false;
    }
  Serial.println("**********reboot**********");
  Serial.println(lastEmailTime);
  delay(2500);

  // Mettre en veille profonde pendant 20 secondes
  lastEmailTime += 1;
  esp_deep_sleep(20000000);
}

String loadHTMLFromFile(const char* filename) {
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return String();
  }

  String content;
  while (file.available()) {
    content += file.readString();
  }

  file.close();
  return content;
}
