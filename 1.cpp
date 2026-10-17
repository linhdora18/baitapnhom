#include "HX711.h"

// Định nghĩa Pin kết nối
#define HX711_DOUT_PIN  16
#define HX711_SCK_PIN   4
#define LEAK_SENSOR_PIN 36
#define DOOR_SENSOR_PIN 18

// Ngưỡng phát hiện rò rỉ (0 - 4095)
#define LEAK_THRESHOLD  1500 

HX711 scale;

// Hệ số hiệu chuẩn Load Cell (được xác định sau khi hiệu chuẩn tạ chuẩn)
float calibration_factor = 420.5; 

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo Load Cell
  scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); // Cân bằng về 0 (Zero out)

  // Khởi tạo Pin cảm biến
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
}

// 1. Hàm đọc khối lượng nước hoa (Đơn vị: Grams)
float getPerfumeWeight() {
  if (scale.is_ready()) {
    // Đọc trung bình 5 lần để giảm nhiễu (noise filtering)
    return scale.get_units(5); 
  } else {
    Serial.println("[ERROR] HX711 not detected!");
    return -1.0;
  }
}

// 2. Hàm kiểm tra an toàn và rò rỉ
void checkSensorsHealth() {
  // Đọc cảm biến rò rỉ
  int leakValue = analogRead(LEAK_SENSOR_PIN);
  if (leakValue > LEAK_THRESHOLD) {
    Serial.println("[ALERT] LEAKAGE_DETECTED! Shutting down valves...");
    // Thực hiện ngắt van khẩn cấp tại đây
  }

  // Đọc cảm biến cửa
  bool isDoorOpen = digitalRead(DOOR_SENSOR_PIN);
  if (isDoorOpen == HIGH) {
    Serial.println("[WARNING] DOOR_OPEN!");
  }
}

void loop() {
  float currentWeight = getPerfumeWeight();
  Serial.printf("Current Perfume Weight: %.2f g\n", currentWeight);
  
  checkSensorsHealth();
  
  delay(1000); // Chu kỳ đọc 1 giây
}