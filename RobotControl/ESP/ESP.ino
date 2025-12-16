#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// ================= CẤU HÌNH MẠNG =================
const char* ssid = "3629";          // <--- Nhập tên WiFi
const char* password = "999999999"; // <--- Nhập pass WiFi

// HiveMQ Cloud
const char* mqtt_server = "163f01d37e0d400d86666ab2d0034720.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "RobotUser";
const char* mqtt_pass = "Manh0202";

// ================= GIAO TIẾP ARDUINO (SOFTWARE SERIAL) =================
#define RX_PIN 5 
#define TX_PIN 4 
SoftwareSerial SS_Arduino(RX_PIN, TX_PIN);

WiFiClientSecure espClient;
PubSubClient client(espClient);

int currentSpeed = 150; 
unsigned long lastReconnectAttempt = 0; // Biến đếm thời gian để kết nối lại không gây treo

// ================= CALLBACK XỬ LÝ LỆNH =================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  
  Serial.print("MQTT: "); Serial.println(msg); // Debug

  if (String(topic) == "robot/control") {
    SS_Arduino.print(msg.charAt(0)); 
  } 
  else if (String(topic) == "robot/speed") {
    currentSpeed = msg.toInt(); 
    SS_Arduino.print("V"); 
    SS_Arduino.print(msg);
    SS_Arduino.println(); 
  }
}

// ================= HÀM KẾT NỐI (NON-BLOCKING) =================
// Hàm này trả về True/False chứ không treo máy
boolean connectMQTT() {
  Serial.print("Connecting to HiveMQ...");
  
  // Tạo ID ngẫu nhiên để tránh xung đột
  String clientId = "ESP32-Xe-" + String(random(0xffff), HEX);
  
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println("CONNECTED!");
    client.subscribe("robot/control");
    client.subscribe("robot/speed");
    return true;
  } else {
    Serial.print("Failed rc=");
    Serial.println(client.state());
    return false;
  }
}
const char* root_ca = 
"-----BEGIN CERTIFICATE-----\n" \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3uleqGksS8q87lZqVj\n" \
"prreAH+4q4Cs9qvBE+919kEXK0iTPx8/4t0W5F6771k3T0FskT80JzMce3Q96fW9\n" \
"HuZ7sJy28vym7T69n88jCNv8NNs+Gw/xg64fgQ8RumSBkatu1+S581KEJ2M5IudX\n" \
"fnn84CaCPna9aI0n168123123123812381283128381238128312838123812831\n" \
"4658465846584658465846584658465846584658465846584658465846584658\n" \
"agEwDQYJKoZIhvcNAQELBQADggIBAEr22wnaJw06t5qEnm7p0oG+fQoM2D+LSnSX\n" \
"Wl8hMsc7MyaN7l5V1J4t5L4f5L4f5L4f5L4f5L4f5L4f5L4f5L4f5L4f5L4f5L4f\n" \
"-----END CERTIFICATE-----\n";
void setup() {
  Serial.begin(115200);
  SS_Arduino.begin(9600);

  // 1. Kết nối WiFi nhanh hơn
  WiFi.mode(WIFI_STA); // Chế độ Station giúp kết nối ổn định hơn
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print(".");
    retry++;
  }
  Serial.println("\nWiFi Connected");

  // 2. Cấu hình SSL tối ưu
  espClient.setInsecure(); // Bỏ qua chứng chỉ (Tăng tốc độ kết nối)
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // 3. [QUAN TRỌNG] Tăng bộ đệm để chứa gói tin SSL của HiveMQ
  client.setBufferSize(1024); // Mặc định là 256, tăng lên 1024 để không bị rớt
  
  // 4. Thiết lập KeepAlive lâu hơn để tránh bị ngắt
  client.setKeepAlive(60); 
}

void loop() {
  // --- QUẢN LÝ KẾT NỐI (KHÔNG DÙNG WHILE LOOP) ---
  if (!client.connected()) {
    long now = millis();
    // Chỉ thử kết nối lại mỗi 5 giây một lần, thời gian còn lại để xử lý việc khác
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // An toàn: Gửi lệnh Dừng khi mất mạng
      SS_Arduino.print('S'); 
      
      if (connectMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop(); // Duy trì kết nối
  }

  // --- NHẬN DỮ LIỆU TỪ ARDUINO VÀ GỬI JSON ---
  // Code này chỉ chạy khi Arduino gửi dữ liệu
  if (SS_Arduino.available()) {
    String line = SS_Arduino.readStringUntil('\n');
    line.trim(); 
    
    if (line.startsWith("D:")) {
      int dist = line.substring(2).toInt();
      
      // Chỉ gửi lên Mây khi đang có kết nối
      if (client.connected()) {
        StaticJsonDocument<200> doc;
        doc["khoang_cach"] = dist;
        doc["toc_do"] = currentSpeed;
        doc["trang_thai"] = "Online";
        
        char buffer[256];
        serializeJson(doc, buffer);
        client.publish("robot/data", buffer);
        
        // Debug nhẹ
        // Serial.println(buffer); 
      }
    }
  }
}