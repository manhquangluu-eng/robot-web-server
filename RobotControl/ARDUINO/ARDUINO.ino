#include <Servo.h>
#include <SoftwareSerial.h>

// ===================== GIAO TIẾP =====================
#define RX_PIN 13   // Arduino RX (Nối với ESP TX - GPIO 4)
#define TX_PIN 12   // Arduino TX (Nối với ESP RX - GPIO 5)
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

// ===================== MOTOR PIN (Cấu hình của bạn) =====================
#define ENA1 11; #define ENB1 6;  #define TT_T 10; #define TT_L 9;  #define PT_T 8;  #define PT_L 7;
#define ENA2 5;  #define ENB2 3;  #define TS_T A2; #define TS_L A3; #define PS_T A4; #define PS_L A5;

int motorSpeed = 110; 
int currentMode = 1; // 1=Manual, 2=Auto

// ===================== HÀM ĐO KHOẢNG CÁCH (TỐI ƯU) =====================
int doKhoangCach() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  thoigian = pulseIn(ECHO_PIN, HIGH, 25000); // Giảm timeout xuống 25ms để đỡ lag
  if (thoigian == 0) return 300; // Quá xa
  return thoigian / 58; // Công thức chuẩn cm
}

void setMotorSpeed(int speed) {
  motorSpeed = constrain(speed, 60, 255); // Giới hạn dưới để motor không gầm
  analogWrite(11, motorSpeed); analogWrite(6, motorSpeed);
  analogWrite(5, motorSpeed);  analogWrite(3, motorSpeed);
}

// Macro điều khiển (Rút gọn cho dễ nhìn)
void go(int t1,int t2, int p1,int p2, int s1,int s2, int ps1,int ps2) {
  digitalWrite(10, t1); digitalWrite(9, t2); digitalWrite(8, p1); digitalWrite(7, p2);
  digitalWrite(A2, s1); digitalWrite(A3, s2); digitalWrite(A4, ps1); digitalWrite(A5, ps2);
}
void stopRobot() { go(0,0, 0,0, 0,0, 0,0); }

void execute(char cmd) {
  switch(cmd) {
    case 'F': go(1,0, 1,0, 1,0, 1,0); break;
    case 'B': go(0,1, 0,1, 0,1, 0,1); break;
    case 'L': go(0,1, 1,0, 0,1, 1,0); break;
    case 'R': go(1,0, 0,1, 1,0, 0,1); break;
    case 'G': go(0,1, 1,0, 1,0, 0,1); break;
    case 'I': go(1,0, 0,1, 0,1, 1,0); break;
    case 'K': go(0,0, 1,0, 0,0, 1,0); break;
    case 'Q': go(1,0, 0,0, 1,0, 0,0); break; // App gửi Q
    case 'M': go(1,0, 0,0, 1,0, 0,0); break; // Web gửi M
    case 'H': go(0,0, 0,1, 0,0, 0,1); break;
    case 'J': go(0,1, 0,0, 0,1, 0,0); break;
    case 'S': stopRobot(); break;
  }
}

void runAutoMode() {
  int kc = doKhoangCach();
  if (kc < STOP_DIST) {
    stopRobot(); delay(300);
    go(0,1, 0,1, 0,1, 0,1); delay(400); stopRobot(); // Lùi
    
    headServo.write(170); delay(400); int left = doKhoangCach();
    headServo.write(10);  delay(400); int right = doKhoangCach();
    headServo.write(90);  delay(300);

    if(left > right) { execute('L'); delay(400); } 
    else { execute('R'); delay(400); }
    stopRobot();
  } else {
    execute('F');
  }
}

void setup() {
  Serial.begin(9600);
  ESP_Serial.begin(9600);
  headServo.attach(SERVO_PIN); headServo.write(90);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  
  pinMode(11, OUTPUT); pinMode(6, OUTPUT); pinMode(5, OUTPUT); pinMode(3, OUTPUT);
  pinMode(10, OUTPUT); pinMode(9, OUTPUT); pinMode(8, OUTPUT); pinMode(7, OUTPUT);
  pinMode(A2, OUTPUT); pinMode(A3, OUTPUT); pinMode(A4, OUTPUT); pinMode(A5, OUTPUT);

  setMotorSpeed(motorSpeed);
  stopRobot();
}

void loop() {
  // --- 1. NHẬN LỆNH ---
  if (ESP_Serial.available()) {
    char cmd = ESP_Serial.read();
    
    if (cmd == 'V') { int s = ESP_Serial.parseInt(); setMotorSpeed(s); }
    else if (cmd == 'A') { currentMode = 2; stopRobot(); headServo.write(90); }
    else if (cmd == 'M') { currentMode = 1; stopRobot(); headServo.write(90); }
    else if (currentMode == 1 && cmd != '\n' && cmd != '\r') {
       if ((cmd=='F' || cmd=='K' || cmd=='Q') && doKhoangCach() < STOP_DIST) stopRobot();
       else execute(cmd);
    }
  }

  // --- 2. GỬI DỮ LIỆU (Chỉ gửi khi không quá bận) ---
  if (millis() - lastSensorTime > 2000) { // 2 giây gửi 1 lần để đỡ lag
    int val = doKhoangCach();
    ESP_Serial.print("D:"); ESP_Serial.println(val);
    lastSensorTime = millis();
  }

  // --- 3. AUTO ---
  if (currentMode == 2) runAutoMode();
}