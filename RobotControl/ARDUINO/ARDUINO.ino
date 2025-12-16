#include <Servo.h>
#include <SoftwareSerial.h>

// ===================== GIAO TIẾP =====================
#define RX_PIN 13   // Arduino RX (Nối với ESP TX)
#define TX_PIN 12   // Arduino TX (Nối với ESP RX)
SoftwareSerial ESP_Serial(RX_PIN, TX_PIN);

// ===================== THIẾT BỊ =====================
#define SERVO_PIN 2
#define TRIG_PIN  A0
#define ECHO_PIN  A1
Servo headServo;

// ===================== BIẾN TOÀN CỤC =====================
long thoigian;
int khoangcach = 0;
unsigned long lastSensorTime = 0;
#define STOP_DIST 20 

// ===================== MOTOR PIN (ĐÃ SỬA LỖI CÚ PHÁP) =====================
// Lưu ý: Đã xóa dấu chấm phẩy thừa ở cuối các dòng #define
#define ENA1 11 // Trái Trước PWM
#define ENB1 6  // Trái Sau PWM (Theo code cũ của bạn map ENB1 là pin 6)
// Tuy nhiên logic thường là: Driver 1 (Trái Trước + Phải Trước), Driver 2 (Trái Sau + Phải Sau)
// Nhưng tôi sẽ giữ nguyên chân pin theo code của bạn cung cấp.

// BÁNH TRÁI TRƯỚC (FL)
#define TT_T 10 
#define TT_L 9

// BÁNH PHẢI TRƯỚC (FR)
#define PT_T 8
#define PT_L 7

// BÁNH TRÁI SAU (BL)
#define TS_T A2
#define TS_L A3

// BÁNH PHẢI SAU (BR)
#define PS_T A4
#define PS_L A5

// Pin PWM cho Driver 2
#define ENA2 5 
#define ENB2 3 

int motorSpeed = 110; 
int currentMode = 1; // 1=Manual, 2=Auto

// ===================== HÀM ĐO KHOẢNG CÁCH =====================
int doKhoangCach() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  thoigian = pulseIn(ECHO_PIN, HIGH, 25000); 
  if (thoigian == 0) return 300; 
  return thoigian / 58; 
}

void setMotorSpeed(int speed) {
  motorSpeed = constrain(speed, 60, 255);
  // Bật tất cả các kênh Enable
  analogWrite(ENA1, motorSpeed); analogWrite(ENB1, motorSpeed);
  analogWrite(ENA2, motorSpeed); analogWrite(ENB2, motorSpeed);
}

// ===================== HÀM ĐIỀU KHIỂN MOTOR (MA TRẬN) =====================
// Thứ tự tham số: 
// Trái Trước(F,B) - Phải Trước(F,B) - Trái Sau(F,B) - Phải Sau(F,B)
void go(int t1,int t2, int p1,int p2, int s1,int s2, int ps1,int ps2) {
  digitalWrite(TT_T, t1); digitalWrite(TT_L, t2); // Trái Trước
  digitalWrite(PT_T, p1); digitalWrite(PT_L, p2); // Phải Trước
  digitalWrite(TS_T, s1); digitalWrite(TS_L, s2); // Trái Sau
  digitalWrite(PS_T, ps1); digitalWrite(PS_L, ps2); // Phải Sau
}

void stopRobot() { go(0,0, 0,0, 0,0, 0,0); }

// ===================== LOGIC DI CHUYỂN MECANUM CHUẨN =====================
// Dựa trên biểu đồ vector lực bạn cung cấp
void execute(char cmd) {
  switch(cmd) {
    // 1. CƠ BẢN
    case 'F': go(1,0, 1,0, 1,0, 1,0); break; // Tiến: Tất cả tiến
    case 'B': go(0,1, 0,1, 0,1, 0,1); break; // Lùi: Tất cả lùi
    
    // 2. XOAY TẠI CHỖ (Rotate)
    case 'L': go(0,1, 1,0, 0,1, 1,0); break; // Xoay Trái: Trái lùi, Phải tiến
    case 'R': go(1,0, 0,1, 1,0, 0,1); break; // Xoay Phải: Trái tiến, Phải lùi

    // 3. ĐI NGANG (Slide / Strafe)
    // Sang Trái (Left): Trái Trước lùi, Phải Trước tiến, Trái Sau tiến, Phải Sau lùi
    case 'G': go(0,1, 1,0, 1,0, 0,1); break; 
    
    // Sang Phải (Right): Trái Trước tiến, Phải Trước lùi, Trái Sau lùi, Phải Sau tiến
    case 'I': go(1,0, 0,1, 0,1, 1,0); break; 

    // 4. ĐI CHÉO (Diagonal) - Nguyên tắc: Chỉ 2 bánh chéo nhau chạy
    // Chéo Tiến Trái (Forward-Left): Phải Trước + Trái Sau chạy tiến
    case 'K': go(0,0, 1,0, 1,0, 0,0); break; 

    // Chéo Tiến Phải (Forward-Right): Trái Trước + Phải Sau chạy tiến
    case 'Q': go(1,0, 0,0, 0,0, 1,0); break; 

    // Chéo Lùi Trái (Backward-Left): Trái Trước + Phải Sau chạy lùi
    case 'H': go(0,1, 0,0, 0,0, 0,1); break; 

    // Chéo Lùi Phải (Backward-Right): Phải Trước + Trái Sau chạy lùi
    case 'J': go(0,0, 0,1, 0,1, 0,0); break; 

    // 5. KHÁC
    case 'S': stopRobot(); break;
    // Các lệnh M, A giữ nguyên logic chuyển chế độ
    case 'M': currentMode = 1; stopRobot(); headServo.write(90); break;
    // case 'A' xử lý bên dưới
  }
}

void runAutoMode() {
  int kc = doKhoangCach();
  // Gửi trạng thái về ESP (Để hiển thị lên Web)
  
  if (kc < STOP_DIST) {
    stopRobot(); delay(300);
    go(0,1, 0,1, 0,1, 0,1); delay(400); stopRobot(); // Lùi lại
    
    headServo.write(170); delay(400); int left = doKhoangCach();
    headServo.write(10);  delay(400); int right = doKhoangCach();
    headServo.write(90);  delay(300);

    if(left > right) { 
      // Rẽ trái (Xoay tại chỗ)
      execute('L'); delay(400); 
    } else { 
      // Rẽ phải
      execute('R'); delay(400); 
    }
    stopRobot();
  } else {
    execute('F'); // Đi thẳng
  }
}

void setup() {
  Serial.begin(9600);
  ESP_Serial.begin(9600);
  headServo.attach(SERVO_PIN); headServo.write(90);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  
  // Khai báo Output cho Motor
  pinMode(ENA1, OUTPUT); pinMode(ENB1, OUTPUT); 
  pinMode(ENA2, OUTPUT); pinMode(ENB2, OUTPUT);
  
  pinMode(TT_T, OUTPUT); pinMode(TT_L, OUTPUT); 
  pinMode(PT_T, OUTPUT); pinMode(PT_L, OUTPUT);
  pinMode(TS_T, OUTPUT); pinMode(TS_L, OUTPUT); 
  pinMode(PS_T, OUTPUT); pinMode(PS_L, OUTPUT);

  setMotorSpeed(motorSpeed);
  stopRobot();
}

void loop() {
  // --- 1. NHẬN LỆNH ---
  if (ESP_Serial.available()) {
    char cmd = ESP_Serial.read();
    
    // Debug: In ra Serial máy tính để kiểm tra nhận được lệnh gì
    // Serial.println(cmd); 

    if (cmd == 'V') { int s = ESP_Serial.parseInt(); setMotorSpeed(s); }
    else if (cmd == 'A') { currentMode = 2; stopRobot(); headServo.write(90); }
    else if (cmd == 'M') { currentMode = 1; stopRobot(); headServo.write(90); }
    else if (currentMode == 1 && cmd != '\n' && cmd != '\r') {
       // Chỉ chặn Tiến/Chéo tiến khi gặp vật cản, còn Lùi/Xoay thì vẫn cho phép
       if ((cmd=='F' || cmd=='K' || cmd=='Q') && doKhoangCach() < STOP_DIST) {
          stopRobot();
       } else {
          execute(cmd);
       }
    }
  }

  // --- 2. GỬI DỮ LIỆU ---
  if (millis() - lastSensorTime > 2000) { 
    int val = doKhoangCach();
    ESP_Serial.print("D:"); ESP_Serial.println(val);
    lastSensorTime = millis();
  }

  // --- 3. AUTO ---
  if (currentMode == 2) runAutoMode();
}