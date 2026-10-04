#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =============================================================================
// CẤU HÌNH PHẦN CỨNG ESP32 (SƠ ĐỒ 2 ĐÃ CHUẨN HÓA - KHÔNG XUNG ĐỘT)
// =============================================================================
// 1. Khối Cảm biến (ADC1 an toàn tuyệt đối khi bật Wi-Fi)
#define PIN_ONE_WIRE_BUS 4   // Cảm biến nhiệt độ DS18B20 (1-Wire)
#define PIN_POT_DO       34  // Biến trở giả lập Oxy hòa tan DO (ADC1_CH6)
#define PIN_POT_PH       35  // Biến trở giả lập độ pH (ADC1_CH7)
#define PIN_POT_SAL      32  // Biến trở giả lập Độ mặn Salinity (ADC1_CH4)
#define PIN_POT_CT       33  // Biến trở giả lập Dòng tải quạt nước (ADC1_CH5)

// Cảm biến siêu âm HC-SR04 đo mực nước
#define PIN_TRIG         5   // Chân phát xung Trig
#define PIN_ECHO         18  // Chân nhận phản hồi Echo

// 2. Khối Hiển thị I2C (LCD 20x4 PCF8574: SDA=21, SCL=22)
#define LCD_I2C_ADDR     0x27 

// 3. Khối Chấp hành & Cảnh báo tại ao
#define PIN_RELAY1       19  // Relay quạt nước 1 (Quạt chính)
#define PIN_RELAY2       25  // Relay quạt nước 2 (Quạt tăng cường)
#define PIN_SERVO        26  // Động cơ Servo cơ cấu máy cho ăn
#define PIN_BUZZER       27  // Còi báo động tại bờ ao
#define PIN_LED_GREEN    16  // Đèn LED trạng thái Bình thường
#define PIN_LED_RED      14  // Đèn LED trạng thái Báo động sự cố

// CẤU HÌNH KẾT NỐI WI-FI VÀ MQTT BROKER (HIVEMQ PUBLIC CLOUD)
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;

// Cây Topic MQTT chuẩn hóa định danh nhóm
const char* TOPIC_STATUS    = "nhom2_quanhoicanh/farm/pond_01/status";    // LWT & Online status (Retained)
const char* TOPIC_TELEMETRY = "nhom2_quanhoicanh/farm/pond_01/telemetry"; // Dữ liệu định kỳ (QoS 0)
const char* TOPIC_ALERT     = "nhom2_quanhoicanh/farm/pond_01/alert";     // Báo động sự cố khẩn cấp (QoS 1)
const char* TOPIC_CONTROL   = "nhom2_quanhoicanh/farm/pond_01/control";   // Nhận lệnh điều khiển (Quạt/Cho ăn)
const char* TOPIC_SANDBOX   = "nhom2_quanhoicanh/farm/pond_01/sandbox";   // Nhận thông số Digital Twin Testbench

// KHAI BÁO CÁC ĐỐI TƯỢNG VÀ BIẾN TOÀN CỤC
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 20, 4);
OneWire oneWire(PIN_ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Servo feederServo;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Biến lưu thông số môi trường thực tế
float tempWater = 28.5;
float doValue   = 5.2;   // mg/L (nguy hiểm < 3.5 mg/L)
float phValue   = 7.8;   // an toàn 7.5 - 8.5
float salValue  = 18.0;  // ppt (độ mặn)
float fanAmp    = 9.5;   // Ampe (bình thường 8-12A, quá tải > 16A)
float waterLvl  = 120.0; // cm

// Chế độ Digital Twin Sandbox (Mục 4.3.3)
bool  sandboxMode = false;
float sbTemp      = 28.5;
float sbDO        = 5.2;
float sbPH        = 7.8;
float sbSal       = 18.0;
float sbFanAmp    = 9.5;
float sbWater     = 120.0;

// Biến trạng thái điều khiển
bool isAlertMode    = false;
bool lastAlertState = false;
bool relay1State    = true;
bool relay2State    = false;
bool feederMoving   = false;

// Quản lý thời gian chạy không chặn (Non-blocking timing)
unsigned long lastSampleTime   = 0;
unsigned long lastFeederTime   = 0;
unsigned long lastTelemetryPub = 0;
unsigned long lastMqttRetry    = 0;

// HÀM ĐỌC MỰC NƯỚC TỪ CẢM BIẾN SIÊU ÂM HC-SR04
float readDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // Timeout 30ms (~5m)
  if (duration == 0) return 120.0; // Giá trị dự phòng nếu không nhận xung
  float distance = duration * 0.034 / 2.0;
  return distance;
}

// CALLBACK XỬ LÝ LỆNH TỪ CLOUD (CONTROL & SANDBOX)
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.println("\n-------------------------------------------");
  Serial.printf("[MQTT NHẬN ĐƯỢC] Topic: %s\n", topic);
  Serial.printf("[NỘI DUNG]: %s\n", message.c_str());

  // 1. XỬ LÝ LỆNH TỪ DIGITAL TWIN TESTBENCH (Mục 4.3.3)
  if (String(topic) == TOPIC_SANDBOX) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, message);
    if (!err) {
      if (doc.containsKey("sandbox")) {
        sandboxMode = doc["sandbox"];
      }
      if (doc.containsKey("temp"))    sbTemp    = doc["temp"];
      if (doc.containsKey("do"))      sbDO      = doc["do"];
      if (doc.containsKey("ph"))      sbPH      = doc["ph"];
      if (doc.containsKey("sal"))     sbSal     = doc["sal"];
      if (doc.containsKey("fan_amp")) sbFanAmp  = doc["fan_amp"];
      if (doc.containsKey("water"))   sbWater   = doc["water"];

      Serial.printf(">> [SANDBOX] Che do: %s | DO=%.2f | pH=%.2f | T=%.1f | I=%.1fA\n",
                    sandboxMode ? "BẬT (Ghi đè Web)" : "TẮT (Dùng Cảm biến thật)",
                    sbDO, sbPH, sbTemp, sbFanAmp);
    }
  }

  // 2. XỬ LÝ LỆNH ĐIỀU KHIỂN THỦ CÔNG TỪ WEB DASHBOARD (KB4 - Mục 5.2.4)
  else if (String(topic) == TOPIC_CONTROL) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, message);
    if (!err) {
      const char* device = doc["device"];
      const char* action = doc["action"];

      // Khi đang trong chế độ Khẩn cấp (Sụt Oxy/Quá tải), KHÔNG cho phép tắt quạt thủ công
      // để bảo vệ an toàn sinh học (Interlock Enforcement)
      if (isAlertMode && (strcmp(action, "OFF") == 0)) {
        Serial.println(">> [TỪ CHỐI]: Khóa Interlock đang bật! Không thể tắt quạt khi sự cố.");
      } else {
        if (strcmp(device, "relay1") == 0) {
          relay1State = (strcmp(action, "ON") == 0);
          digitalWrite(PIN_RELAY1, relay1State ? HIGH : LOW);
          Serial.printf(">> Lệnh: Quạt 1 -> %s\n", relay1State ? "BẬT" : "TẮT");
        } else if (strcmp(device, "relay2") == 0) {
          relay2State = (strcmp(action, "ON") == 0);
          digitalWrite(PIN_RELAY2, relay2State ? HIGH : LOW);
          Serial.printf(">> Lệnh: Quạt 2 -> %s\n", relay2State ? "BẬT" : "TẮT");
        } else if (strcmp(device, "feeder") == 0) {
          if (!isAlertMode && strcmp(action, "FEED") == 0) {
            feederMoving = true;
            lastFeederTime = millis();
            feederServo.write(90);
            Serial.println(">> Lệnh: Kích hoạt xả thức ăn thủ công (2 giây)");
          }
        }
      }
    }
  }
  Serial.println("-------------------------------------------");
}

// KẾT NỐI WI-FI
void setupWiFi() {
  Serial.print("\n[WIFI] Đang kết nối tới: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(300);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Đã kết nối thành công!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WIFI] Chưa có kết nối. Sẽ thử lại trong vòng lặp.");
  }
}

// KẾT NỐI MQTT BROKER VÀ ĐĂNG KÝ TOPIC (KÈM LWT - MỤC 4.2.3)
void reconnectMQTT() {
  if (mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttRetry < 4000) return; // Thử lại mỗi 4 giây (Non-blocking)
  lastMqttRetry = now;

  Serial.print("[MQTT] Đang kết nối tới HiveMQ Broker (broker.hivemq.com)... ");
  String clientId = "ESP32_ShrimpFarm_Pond01_" + String(random(0xffff), HEX);

  // Cấu hình LWT (Last Will and Testament - Mục 4.2.3):
  // Nếu ESP32 đứt mạng hoặc mất nguồn đột ngột, Broker sẽ tự động phát "Offline" (Retain = true)
  const char* willTopic   = TOPIC_STATUS;
  int         willQos     = 1;
  bool        willRetain  = true;
  const char* willMessage = "Offline";

  if (mqttClient.connect(clientId.c_str(), nullptr, nullptr, willTopic, willQos, willRetain, willMessage)) {
    Serial.println("THÀNH CÔNG!");

    // Phát trạng thái Online khi kết nối thành công (Retained)
    mqttClient.publish(TOPIC_STATUS, "Online", true);

    // Đăng ký nhận lệnh từ Web Dashboard và Digital Twin
    mqttClient.subscribe(TOPIC_CONTROL);
    mqttClient.subscribe(TOPIC_SANDBOX);
    Serial.printf("[MQTT] Đã Subscribe: %s\n", TOPIC_CONTROL);
    Serial.printf("[MQTT] Đã Subscribe: %s\n", TOPIC_SANDBOX);
  } else {
    Serial.printf("Thất bại, mã lỗi rc=%d. Tiếp tục vận hành cục bộ.\n", mqttClient.state());
  }
}

// HÀM KHỞI TẠO (SETUP)
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=======================================================");
  Serial.println("  HE THONG QUAN LY TRANG TRAI NUOI TOM THONG MINH");
  Serial.println("  Nhom 2 - De tai 16");
  Serial.println("  Firmware: Edge Node + MQTT + Digital Twin Sandbox");
  Serial.println("=======================================================\n");

  // Cấu hình chân I/O
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RELAY1, OUTPUT);
  pinMode(PIN_RELAY2, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);

  // Cấu hình ADC an toàn (12-bit, 0 - 3.3V)
  analogReadResolution(12);

  // Trạng thái khởi tạo mặc định: Quạt 1 bật, Quạt 2 tắt
  digitalWrite(PIN_RELAY1, HIGH);
  digitalWrite(PIN_RELAY2, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);

  // Khởi tạo Servo máy cho ăn (cơ cấu van quay 0 - 90 độ)
  feederServo.attach(PIN_SERVO);
  feederServo.write(0);

  // Khởi tạo Cảm biến nhiệt độ DS18B20
  sensors.begin();

  // Khởi tạo Màn hình LCD 20x4 I2C
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" SMART SHRIMP FARM  ");
  lcd.setCursor(0, 1);
  lcd.print(" TRAM AO NUOI SO 01 ");
  lcd.setCursor(0, 2);
  lcd.print(" KET NOI CLOUD MQTT ");
  lcd.setCursor(0, 3);
  lcd.print(" NHOM 2 - DE TAI 16 ");
  delay(1200);
  lcd.clear();

  // Khởi tạo Wi-Fi và MQTT Client
  setupWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  mqttClient.setBufferSize(512); // Tăng kích thước bộ đệm nhận JSON
}

// VÒNG LẶP CHÍNH (LOOP)
void loop() {
  unsigned long currentMillis = millis();

  // 1. Duy trì kết nối Wi-Fi & MQTT không làm nghẽn tiến trình
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    } else {
      mqttClient.loop();
    }
  }

  // 2. CHU KỲ LẤY MẪU VÀ XỬ LÝ LOGIC AN TOÀN (Mỗi 1000ms)
  if (currentMillis - lastSampleTime >= 1000) {
    lastSampleTime = currentMillis;

    // --- A. THU THẬP THÔNG SỐ (CHẾ ĐỘ THẬT VS SANDBOX) ---
    if (sandboxMode) {
      // Khi bật Digital Twin Sandbox từ Web: Nhận giá trị do người dùng gạt trên thanh trượt
      tempWater = sbTemp;
      doValue   = sbDO;
      phValue   = sbPH;
      salValue  = sbSal;
      fanAmp    = sbFanAmp;
      waterLvl  = sbWater;
    } else {
      // Chế độ Vận hành bình thường: Đọc trực tiếp từ cảm biến thật và biến trở ADC1
      sensors.requestTemperatures();
      float t = sensors.getTempCByIndex(0);
      if (t != DEVICE_DISCONNECTED_C && t > -50.0) {
        tempWater = t;
      }

      int rawDO  = analogRead(PIN_POT_DO);
      int rawPH  = analogRead(PIN_POT_PH);
      int rawSal = analogRead(PIN_POT_SAL);
      int rawCT  = analogRead(PIN_POT_CT);

      doValue  = (rawDO  / 4095.0) * 10.0; // 0 - 10 mg/L
      phValue  = (rawPH  / 4095.0) * 14.0; // 0 - 14 pH
      salValue = (rawSal / 4095.0) * 35.0; // 0 - 35 ppt
      fanAmp   = (rawCT  / 4095.0) * 20.0; // 0 - 20 A
      waterLvl = readDistanceCM();
    }

    // --- B. THUẬT TOÁN AN TOÀN & KHÓA LIÊN ĐỘNG (INTERLOCK - MỤC 4.1) ---
    // Tiêu chí kích hoạt luồng khẩn cấp:
    // 1. Sụt giảm oxy nghiêm trọng (DO < 3.5 mg/L)
    // 2. Động cơ quạt bị quá tải (I > 16.0 A - nguy cơ kẹt cánh/cháy motor)
    // 3. pH nằm ngoài khoảng an toàn (pH < 6.5 hoặc pH > 9.0)
    if (doValue < 3.5 || fanAmp > 16.0 || phValue < 6.5 || phValue > 9.0) {
      isAlertMode = true;
    } else {
      isAlertMode = false;
    }

    if (isAlertMode) {
      // >>> LUỒNG SỰ CỐ / KHẨN CẤP <<<
      // Bật 100% công suất 2 dàn quạt nước để cứu ao
      relay1State = true;
      relay2State = true;
      digitalWrite(PIN_RELAY1, HIGH);
      digitalWrite(PIN_RELAY2, HIGH);

      // Cảnh báo tại ao
      digitalWrite(PIN_LED_GREEN, LOW);
      digitalWrite(PIN_LED_RED, HIGH);
      digitalWrite(PIN_BUZZER, HIGH);

      // KHÓA CỨNG MÁY CHO ĂN (INTERLOCK): Triệt tiêu nguy cơ thối nước đáy ao
      feederServo.write(0);
      feederMoving = false;

      // Phát cảnh báo khẩn cấp lên Topic Alert (QoS 1) khi mới chuyển trạng thái
      if (!lastAlertState && mqttClient.connected()) {
        StaticJsonDocument<256> alertDoc;
        alertDoc["pond_id"]   = "pond_01";
        alertDoc["alert"]     = true;
        alertDoc["type"]      = (doValue < 3.5) ? "LOW_DO" : ((fanAmp > 16.0) ? "FAN_OVERLOAD" : "ABNORMAL_PH");
        alertDoc["message"]   = (doValue < 3.5) ? "Nguy hiem: Sut giam Oxy cap tinh!" : 
                                ((fanAmp > 16.0) ? "Bao dong: Qua tai dong quat nuoc!" : "Canh bao: Do pH nuoc bat thuong!");
        alertDoc["do"]        = round(doValue * 10.0) / 10.0;
        alertDoc["fan_amp"]   = round(fanAmp * 10.0) / 10.0;
        alertDoc["ph"]        = round(phValue * 10.0) / 10.0;
        alertDoc["uptime_s"]  = currentMillis / 1000;

        char alertBuf[256];
        serializeJson(alertDoc, alertBuf);
        mqttClient.publish(TOPIC_ALERT, alertBuf, true);
        Serial.printf("\n[MQTT ALERT ĐÃ PHÁT]: %s\n", alertBuf);
      }
    } else {
      // >>> LUỒNG BÌNH THƯỜNG <<<
      // Quạt 1 duy trì, Quạt 2 tắt để tiết kiệm điện
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_LED_RED, LOW);
      digitalWrite(PIN_BUZZER, LOW);

      // Xóa thông báo cảnh báo nếu trước đó có sự cố
      if (lastAlertState && mqttClient.connected()) {
        StaticJsonDocument<128> clearDoc;
        clearDoc["pond_id"]  = "pond_01";
        clearDoc["alert"]    = false;
        clearDoc["message"]  = "Moi truong da tro lai trang thai An toan.";
        clearDoc["uptime_s"] = currentMillis / 1000;

        char clearBuf[128];
        serializeJson(clearDoc, clearBuf);
        mqttClient.publish(TOPIC_ALERT, clearBuf);
      }
    }
    lastAlertState = isAlertMode;

    // --- C. HIỂN THỊ MÀN HÌNH LCD 20x4 ---
    char lineBuf[21];
    snprintf(lineBuf, sizeof(lineBuf), "T:%4.1fC   DO:%4.1fmg", tempWater, doValue);
    lcd.setCursor(0, 0);
    lcd.print(lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "pH:%4.1f   Sal:%4.1fpt", phValue, salValue);
    lcd.setCursor(0, 1);
    lcd.print(lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "H2O:%3.0fcm  I_Fan:%4.1fA", waterLvl, fanAmp);
    lcd.setCursor(0, 2);
    lcd.print(lineBuf);

    lcd.setCursor(0, 3);
    if (isAlertMode) {
      if (doValue < 3.5) {
        lcd.print("!! NGUY HIEM: SUT OXY");
      } else if (fanAmp > 16.0) {
        lcd.print("!! QUAT QUA TAI CAP !!");
      } else {
        lcd.print("!! CANH BAO: NUOC XAU ");
      }
    } else {
      snprintf(lineBuf, sizeof(lineBuf), "Q1:%s Q2:%s %s %s", 
               relay1State ? "ON " : "OFF", 
               relay2State ? "ON " : "OFF",
               feederMoving ? "FEED" : "IDLE",
               sandboxMode ? "[SB]" : "[LV]");
      lcd.print(lineBuf);
    }
  }

  // 3. XUẤT BẢN DỮ LIỆU TELEMETRY ĐỊNH KỲ LÊN MQTT (Mỗi 2000ms)
  if (currentMillis - lastTelemetryPub >= 2000) {
    lastTelemetryPub = currentMillis;

    StaticJsonDocument<384> teleDoc;
    teleDoc["pond_id"]    = "pond_01";
    teleDoc["temp"]       = round(tempWater * 10.0) / 10.0;
    teleDoc["do"]         = round(doValue * 10.0) / 10.0;
    teleDoc["ph"]         = round(phValue * 10.0) / 10.0;
    teleDoc["sal"]        = round(salValue * 10.0) / 10.0;
    teleDoc["water_cm"]   = round(waterLvl);
    teleDoc["fan_amp"]    = round(fanAmp * 10.0) / 10.0;
    teleDoc["relay1"]     = relay1State;
    teleDoc["relay2"]     = relay2State;
    teleDoc["feeder_deg"] = feederMoving ? 90 : 0;
    teleDoc["alert_mode"] = isAlertMode;
    teleDoc["sandbox"]    = sandboxMode;
    teleDoc["uptime_s"]   = currentMillis / 1000;

    char teleBuf[384];
    serializeJson(teleDoc, teleBuf);

    // In Serial Monitor
    Serial.printf("[TELEMETRY]: %s\n", teleBuf);

    // Publish lên Cloud MQTT
    if (mqttClient.connected()) {
      mqttClient.publish(TOPIC_TELEMETRY, teleBuf);
    }
  }

  // 4. CƠ CHẾ CHO ĂN TỰ ĐỘNG (CHỈ HOẠT ĐỘNG KHI MÔI TRƯỜNG AN TOÀN)
  if (!isAlertMode) {
    // Mỗi 15 giây giả lập một chu kỳ cho ăn định kỳ
    if (currentMillis - lastFeederTime >= 15000) {
      lastFeederTime = currentMillis;
      feederMoving = true;
      feederServo.write(90); // Mở van thức ăn
      Serial.println("[CHO ĂN] Bắt đầu xả thức ăn định kỳ...");
    }
    // Sau 2 giây thì đóng van
    if (feederMoving && (currentMillis - lastFeederTime >= 2000)) {
      feederServo.write(0);  // Khóa van
      feederMoving = false;
      Serial.println("[CHO ĂN] Đã đóng van thức ăn.");
    }
  }
}
