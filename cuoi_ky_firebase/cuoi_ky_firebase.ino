#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>
#include <DHT.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <RTClib.h>

// Chân kết nối
#define DHTPIN 2
#define DHTTYPE DHT11
#define RAIN_SENSOR_PIN 14
#define SERVO_PIN 12
#define LED_PIN 4
#define SDA_PIN 0 // D3 trên ESP8266
#define SCL_PIN 5 // D1 trên ESP8266

// Thông tin WiFi
const char* ssid = "TP-LINK_7ED0";
const char* password = "86205245";

// Cấu hình Firebase
#define FIREBASE_HOST "https://btl-he-nhung-ngoquanganh-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "eVMIYK0VbtN2y4LX4xBVINtW4dlqzs0DO9OwiyJ6"

// Khởi tạo RTC
RTC_DS1307 rtc;

// Firebase
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbData;

DHT dht(DHTPIN, DHTTYPE);
Servo servo;
AsyncWebServer server(80);

// Biến để theo dõi trạng thái mưa trước đó
bool lastRainState = false;
// Biến để theo dõi thời gian đã điều khiển
bool scheduledPhơi = false;
bool scheduledThu = false;
// Biến để lưu thời gian hẹn giờ
String phoiTime = "06:00";
String thuTime = "18:00";

void logData(float temperature, float humidity, bool isRaining) {
  String path = "/history/" + String(millis());
  FirebaseJson json;
  json.set("temperature", temperature);
  json.set("humidity", humidity);
  json.set("rain", isRaining);
  json.set("timestamp", String(millis()));
  if (!Firebase.pushJSON(fbData, path, json)) {
    Serial.println("Lỗi lưu lịch sử: " + fbData.errorReason());
  } else {
    Serial.println("Lưu lịch sử thành công: Nhiệt độ: " + String(temperature) + "°C, Độ ẩm: " + String(humidity) + "%, Mưa: " + String(isRaining ? "Có" : "Không"));
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Khởi động I2C và RTC
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!rtc.begin()) {
    Serial.println("Không tìm thấy RTC!");
    while (1);
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC không chạy, cài đặt thời gian!");
    rtc.adjust(DateTime(2025, 7, 28, 10, 0, 0));
  }

  dht.begin();
  delay(2000);
  servo.attach(SERVO_PIN);
  servo.write(0);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
    Serial.println("Đang kết nối WiFi...");
  }
  Serial.println("WiFi đã kết nối!");
  Serial.println("Địa chỉ IP của ESP8266: " + WiFi.localIP().toString());
  digitalWrite(LED_PIN, HIGH);

  // Cấu hình Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Khởi tạo giá trị ban đầu cho /servo/manual và /servo/position
  if (!Firebase.setBool(fbData, "/servo/manual", false)) {
    Serial.println("Lỗi khởi tạo /servo/manual: " + fbData.errorReason());
  }
  if (!Firebase.setInt(fbData, "/servo/position", 0)) {
    Serial.println("Lỗi khởi tạo /servo/position: " + fbData.errorReason());
  }

  // Khởi tạo giá trị ban đầu cho lịch hẹn giờ
  if (!Firebase.setString(fbData, "/schedule/phoi", "06:00")) {
    Serial.println("Lỗi khởi tạo /schedule/phoi: " + fbData.errorReason());
  }
  if (!Firebase.setString(fbData, "/schedule/thu", "18:00")) {
    Serial.println("Lỗi khởi tạo /schedule/thu: " + fbData.errorReason());
  }

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    bool isRaining = digitalRead(RAIN_SENSOR_PIN) == LOW;

    if (isnan(temperature) || isnan(humidity)) {
      temperature = -1;
      humidity = -1;
    }

    DateTime now = rtc.now();
    String currentTime = String(now.hour(), DEC) + ":" + 
                         (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC) + ":" + 
                         (now.second() < 10 ? "0" : "") + String(now.second(), DEC);

    Firebase.setFloat(fbData, "/sensor/temperature", temperature);
    Firebase.setFloat(fbData, "/sensor/humidity", humidity);
    Firebase.setBool(fbData, "/sensor/rain", isRaining);
    Firebase.setString(fbData, "/time/current", currentTime);

    String json = "{";
    json += "\"temperature\": " + String(temperature) + ",";
    json += "\"humidity\": " + String(humidity) + ",";
    json += "\"rain\": " + String(isRaining ? "true" : "false") + ",";
    json += "\"time\": \"" + currentTime + "\"";
    json += "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  });

  server.begin();
  Serial.println("Web server đã khởi động!");
}

void loop() {
  // Đọc thời gian từ RTC
  DateTime now = rtc.now();
  String currentTime = String(now.hour(), DEC) + ":" + 
                       (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC) + ":" + 
                       (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  
  // Gửi thời gian lên Firebase
  if (!Firebase.setString(fbData, "/time/current", currentTime)) {
    Serial.println("Lỗi gửi thời gian: " + fbData.errorReason());
  } else {
    Serial.println("Thời gian hiện tại: " + currentTime);
  }

  // Đọc dữ liệu cảm biến
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool isRaining = digitalRead(RAIN_SENSOR_PIN) == LOW;

  // Gửi dữ liệu lên Firebase và in ra Serial
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.print("Nhiệt độ: ");
    Serial.print(temperature);
    Serial.print(" °C, Độ ẩm: ");
    Serial.print(humidity);
    Serial.print(" %, Trời mưa: ");
    Serial.println(isRaining ? "Có" : "Không");

    if (!Firebase.setFloat(fbData, "/sensor/temperature", temperature)) {
      Serial.println("Lỗi gửi nhiệt độ: " + fbData.errorReason());
    }
    if (!Firebase.setFloat(fbData, "/sensor/humidity", humidity)) {
      Serial.println("Lỗi gửi độ ẩm: " + fbData.errorReason());
    }
    if (!Firebase.setBool(fbData, "/sensor/rain", isRaining)) {
      Serial.println("Lỗi gửi trạng thái mưa: " + fbData.errorReason());
    }

    logData(temperature, humidity, isRaining);
  } else {
    Serial.println("Lỗi: Không đọc được nhiệt độ hoặc độ ẩm!");
  }

  // Kiểm tra trạng thái điều khiển thủ công
  bool isManual = false;
  if (Firebase.getBool(fbData, "/servo/manual")) {
    isManual = fbData.boolData();
  } else {
    Serial.println("Lỗi đọc /servo/manual: " + fbData.errorReason());
  }

  // Đọc thời gian hẹn giờ từ Firebase
  if (Firebase.getString(fbData, "/schedule/phoi")) {
    phoiTime = fbData.stringData();
  } else {
    Serial.println("Lỗi đọc /schedule/phoi: " + fbData.errorReason());
  }

  if (Firebase.getString(fbData, "/schedule/thu")) {
    thuTime = fbData.stringData();
  } else {
    Serial.println("Lỗi đọc /schedule/thu: " + fbData.errorReason());
  }

  // Kiểm tra thời gian để hẹn giờ
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();

  // Chuyển đổi thời gian hẹn giờ từ chuỗi (HH:MM) thành giờ và phút
  int phoiHour = phoiTime.substring(0, 2).toInt();
  int phoiMinute = phoiTime.substring(3, 5).toInt();
  int thuHour = thuTime.substring(0, 2).toInt();
  int thuMinute = thuTime.substring(3, 5).toInt();

  // Tính thời gian hiện tại và thời gian hẹn giờ dưới dạng phút để so sánh
  int currentMinutes = currentHour * 60 + currentMinute;
  int phoiMinutes = phoiHour * 60 + phoiMinute;
  int thuMinutes = thuHour * 60 + thuMinute;

  // Chế độ tự động: Điều khiển servo dựa trên lịch và trạng thái mưa
  if (!isManual) {
    // Trong khoảng thời gian phơi (từ phoiTime đến thuTime)
    if (currentMinutes >= phoiMinutes && currentMinutes < thuMinutes) {
      if (isRaining && !lastRainState) {
        // Trời mưa: Thu quần áo
        if (!Firebase.setInt(fbData, "/servo/position", 180)) {
          Serial.println("Lỗi gửi vị trí servo (trời mưa): " + fbData.errorReason());
        } else {
          Serial.println("Trời mưa: Cập nhật vị trí servo trên Firebase: Góc = 180");
        }
      } else if (!isRaining && lastRainState) {
        // Trời tạnh: Phơi lại quần áo
        if (!Firebase.setInt(fbData, "/servo/position", 0)) {
          Serial.println("Lỗi gửi vị trí servo (trời tạnh): " + fbData.errorReason());
        } else {
          Serial.println("Trời tạnh: Cập nhật vị trí servo trên Firebase: Góc = 0");
        }
      }
      lastRainState = isRaining;
    }
    // Hẹn giờ phơi quần áo
    if (currentHour == phoiHour && currentMinute == phoiMinute && currentSecond <= 2 && !scheduledPhơi) {
      if (!Firebase.setInt(fbData, "/servo/position", 0)) {
        Serial.println("Lỗi gửi vị trí servo (hẹn giờ phơi): " + fbData.errorReason());
      } else {
        Serial.println("Hẹn giờ " + phoiTime + ": Phơi quần áo - Cập nhật vị trí servo trên Firebase: Góc = 0");
        scheduledPhơi = true;
      }
    }
    // Hẹn giờ thu quần áo
    if (currentHour == thuHour && currentMinute == thuMinute && currentSecond <= 2 && !scheduledThu) {
      if (!Firebase.setInt(fbData, "/servo/position", 180)) {
        Serial.println("Lỗi gửi vị trí servo (hẹn giờ thu): " + fbData.errorReason());
      } else {
        Serial.println("Hẹn giờ " + thuTime + ": Thu quần áo - Cập nhật vị trí servo trên Firebase: Góc = 180");
        scheduledThu = true;
      }
    }
  }

  // Reset cờ hẹn giờ khi qua thời điểm hẹn
  if (currentHour != phoiHour || currentMinute != phoiMinute) {
    scheduledPhơi = false;
  }
  if (currentHour != thuHour || currentMinute != thuMinute) {
    scheduledThu = false;
  }

  // Cập nhật vị trí servo từ Firebase
  if (Firebase.getInt(fbData, "/servo/position")) {
    int servoPos = fbData.intData();
    if (servoPos >= 0 && servoPos <= 180) {
      Serial.println("Cập nhật vị trí servo từ Firebase: Góc = " + String(servoPos));
      servo.write(servoPos);
    }
  } else {
    Serial.println("Lỗi đọc vị trí servo từ Firebase: " + fbData.errorReason());
  }

  if (isRaining) {
    Serial.println("Trời đang mưa! Đang thu quần áo...");
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  delay(2000);
}