#include <Arduino.h> // Nạp thư viện cơ bản của Arduino để điều khiển các chân phần cứng (GPIO)
#include "HX711.h"   // Nạp thư viện HX711 để giao tiếp với cảm biến khối lượng (Load Cell)

namespace Pinout { // Khai báo tập hợp các chân cắm phần cứng trên vi điều khiển
    constexpr uint8_t HX711_DOUT    = 16; // Chân nhận dữ liệu Data từ cảm biến cân HX711
    constexpr uint8_t HX711_SCK     = 4;  // Chân xung Clock phát tín hiệu đồng bộ cho HX711
    constexpr uint8_t SOLENOID_VALVE = 23; // Chân phát tín hiệu kích mở van phun nước hoa
    constexpr uint8_t LEAK_SENSOR   = 36; // Chân đọc dữ liệu Analog từ cảm biến phát hiện rò rỉ
    constexpr uint8_t DOOR_SENSOR   = 18; // Chân đọc trạng thái cảm biến đóng/mở cửa máy
}

namespace Config { // Khai báo các tham số cấu hình cài đặt mặc định cho hệ thống
    constexpr float   TARGET_WEIGHT_G   = 0.080f; // Khối lượng tiêu chuẩn mục tiêu cho 1 lần xịt (0.080g = 80mg)
    constexpr float   CALIB_FACTOR      = 420.5f; // Hệ số hiệu chuẩn mặc định của cân Load Cell
    constexpr int     LEAK_THRESHOLD    = 1500;   // Ngưỡng tín hiệu ADC để xác định có dung dịch chảy rò rỉ
    constexpr uint32_t MAX_PULSE_MS     = 1000;   // Thời gian mở van tối đa an toàn (1000ms = 1 giây) để tránh cháy van
}

enum class SystemState { NORMAL, MAINTENANCE, ERROR_LEAK, ERROR_DOOR }; // Các trạng thái hoạt động của hệ thống

class SensorManager { // Lớp đối tượng chịu trách nhiệm quản lý toàn bộ cảm biến
private:
    HX711 scale; // Khai báo đối tượng điều khiển cân HX711

public:
    void begin() { // Hàm khởi tạo các chân cắm và thiết lập ban đầu cho cảm biến
        pinMode(Pinout::LEAK_SENSOR, INPUT);        // Cấu hình chân đọc rò rỉ làm đầu vào
        pinMode(Pinout::DOOR_SENSOR, INPUT_PULLUP); // Cấu hình chân cảm biến cửa làm đầu vào có điện trở kéo lên
        
        scale.begin(Pinout::HX711_DOUT, Pinout::HX711_SCK); // Kết nối module HX711 với các chân tương ứng
        scale.set_scale(Config::CALIB_FACTOR);              // Cài đặt hệ số hiệu chuẩn cho cân
        scale.tare();                                       // Đặt điểm 0 cho cân (trừ tạ ban đầu)
    }

    float getWeight() { // Hàm đọc khối lượng thực tế từ cảm biến cân (đơn vị: gram)
        return scale.is_ready() ? scale.get_units(5) : -1.0f; // Nếu cảm biến sẵn sàng thì đọc lấy trung bình 5 lần, ngược lại trả về -1
    }

    SystemState checkSafety(bool isMaintenance) { // Hàm kiểm tra tình trạng an toàn của các cảm biến
        if (analogRead(Pinout::LEAK_SENSOR) > Config::LEAK_THRESHOLD) { // Nếu giá trị cảm biến rò rỉ vượt ngưỡng cho phép
            return SystemState::ERROR_LEAK; // Trả về trạng thái lỗi rò rỉ dung dịch
        }
        if (digitalRead(Pinout::DOOR_SENSOR) == HIGH && !isMaintenance) { // Nếu phát hiện cửa mở khi đang không ở chế độ bảo trì
            return SystemState::ERROR_DOOR; // Trả về trạng thái lỗi mở cửa trái phép
        }
        return SystemState::NORMAL; // Hệ thống an toàn bình thường
    }
};

class DispenserController { // Lớp đối tượng chịu trách nhiệm điều khiển van phun và quy trình hiệu chuẩn
private:
    SensorManager& sensors;          // Tham chiếu tới đối tượng quản lý cảm biến
    uint32_t pulseDurationMs = 120;  // Thời gian kích mở van mặc định ban đầu (120ms)

    bool fireValve(uint32_t durationMs) { // Hàm nội bộ thực hiện kích mở van solenoid
        if (durationMs > Config::MAX_PULSE_MS) return false; // Nếu thời gian yêu cầu vượt quá giới hạn an toàn thì chặn lại

        digitalWrite(Pinout::SOLENOID_VALVE, HIGH); // Bật điện kích mở van phun
        delay(durationMs);                          // Duy trì mở van trong khoảng thời gian durationMs
        digitalWrite(Pinout::SOLENOID_VALVE, LOW);  // Tắt điện đóng van lại
        return true;                                // Phun thành công
    }

public:
    DispenserController(SensorManager& sens) : sensors(sens) {} // Hàm khởi tạo nhận tham chiếu tới bộ quản lý cảm biến

    void begin() { // Hàm thiết lập chân điều khiển van
        pinMode(Pinout::SOLENOID_VALVE, OUTPUT);   // Cấu hình chân van solenoid làm đầu ra
        digitalWrite(Pinout::SOLENOID_VALVE, LOW); // Mặc định luôn ngắt điện đóng van khi mới khởi động
    }

    float dispense(bool isTest = false) { // Hàm thực hiện 1 chu kỳ phun nước hoa và đo khối lượng tiêu hao
        float weightBefore = sensors.getWeight(); // Bước 1: Đo khối lượng ban đầu trước khi xịt
        
        if (!fireValve(pulseDurationMs)) return -1.0f; // Bước 2: Kích mở van phun, nếu lỗi thì dừng và trả về -1
        
        delay(300); // Bước 3: Chờ 300ms cho giọt nước hoa rơi hẳn và cân ổn định
        float weightAfter = sensors.getWeight(); // Bước 4: Đo lại khối lượng sau khi xịt
        
        float delta = weightBefore - weightAfter; // Bước 5: Tính dung lượng tiêu hao (Lượng ban đầu - Lượng còn lại)

        if (!isTest && delta < 0.01f) { // Nếu xịt thật mà lượng tiêu hao nhỏ hơn 0.01g
            Serial.println("[Cảnh báo] Lượng phun quá ít hoặc hết nước hoa!"); // In ra thông báo cảnh báo
        }
        return delta; // Trả về khối lượng đã xịt ra (gram)
    }

    bool autoCalibrate() { // Quy trình TỰ ĐỘNG HIỆU CHUẨN thời gian mở van dựa trên kết quả đo
        Serial.println("\n>>> Bắt đầu hiệu chuẩn..."); // In thông báo bắt đầu
        
        fireValve(pulseDurationMs); delay(200); // Phun mồi lần 1 để xả bọt khí đọng trong đường ống
        fireValve(pulseDurationMs); delay(500); // Phun mồi lần 2 để đường ống đi vào trạng thái ổn định

        float totalDelta = 0; // Biến tích lũy tổng khối lượng phun thử
        int validCount = 0;   // Biến đếm số lần đo thành công

        for (int i = 0; i < 3; i++) { // Lặp 3 lần phun thử nghiệm để lấy số liệu trung bình
            float d = dispense(true); // Phun ở chế độ kiểm tra (isTest = true)
            if (d > 0) {              // Nếu dữ liệu đo được hợp lệ (> 0g)
                totalDelta += d;      // Cộng dồn khối lượng
                validCount++;         // Tăng số lần đo thành công lên 1
            }
            delay(400); // Nghỉ 400ms giữa các lần phun thử
        }

        if (validCount == 0) { // Nếu sau 3 lần phun mà không có dữ liệu đo hợp lệ
            Serial.println("[Lỗi] Hiệu chuẩn thất bại: Không đo được dữ liệu."); // Báo lỗi thất bại
            return false;
        }

        float avgWeight = totalDelta / validCount; // Tính khối lượng xịt trung bình thực tế của 1 lần phun
        uint32_t newPulse = (uint32_t)(pulseDurationMs * (Config::TARGET_WEIGHT_G / avgWeight)); // Tính thời gian mở van mới theo tỷ lệ
        
        pulseDurationMs = constrain(newPulse, 40, 400); // Ép thời gian mở van mới nằm trong khoảng an toàn (40ms đến 400ms)

        Serial.printf("=> Hoàn tất! Lượng trung bình: %.3fg | Pulse mới: %d ms\n\n", avgWeight, pulseDurationMs); // In kết quả hiệu chuẩn
        return true; // Hiệu chuẩn thành công
    }

    uint32_t getPulseMs() const { return pulseDurationMs; } // Hàm lấy ra thời gian kích van hiện tại
};

SensorManager sensors;                  // Khai báo đối tượng cảm biến toàn cục
DispenserController dispenser(sensors); // Khai báo đối tượng điều khiển van toàn cục

uint32_t lastCheckTime = 0; // Biến lưu mốc thời gian phục vụ kiểm tra an toàn không gây nghẽn (Non-blocking)

void setup() { // Hàm cấu hình chạy 1 lần duy nhất khi vi điều khiển cấp điện
    Serial.begin(115200); // Khởi tạo cổng giao tiếp nối tiếp Serial ở tốc độ 115200 baud
    sensors.begin();      // Khởi tạo các cảm biến
    dispenser.begin();    // Khởi tạo đầu phun
    
    Serial.println("ScentStation Ready. Bấm 's' để Phun, 'c' để Hiệu chuẩn."); // In dòng hướng dẫn sử dụng
}

void loop() { // Vòng lặp chính chạy liên tục trong suốt quá trình thiết bị hoạt động
    if (millis() - lastCheckTime > 500) { // Kiểm tra an toàn định kỳ sau mỗi 500ms
        lastCheckTime = millis();         // Cập nhật lại mốc thời gian kiểm tra mới nhất
        
        SystemState state = sensors.checkSafety(false); // Gọi hàm kiểm tra an toàn cảm biến
        if (state == SystemState::ERROR_LEAK) {         // Nếu phát hiện sự cố rò rỉ chất lỏng
            digitalWrite(Pinout::SOLENOID_VALVE, LOW);  // Lập tức ngắt điện đóng van khẩn cấp
            Serial.println("[CẢNH BÁO NGUY HẠI] Phát hiện rò rỉ dung dịch!"); // In cảnh báo nguy hại
            return; // Thoát vòng lặp, ngưng nhận các lệnh tiếp theo
        }
    }

    if (Serial.available() > 0) { // Nếu có dữ liệu gửi tới từ máy tính qua cổng Serial
        char cmd = Serial.read(); // Đọc ký tự lệnh

        if (cmd == 's' || cmd == 'S') { // Nếu người dùng gõ chữ 's' hoặc 'S'
            Serial.print("Đang xịt nước hoa... "); // In thông báo
            float used = dispenser.dispense();     // Thực hiện 1 lần xịt nước hoa cho khách
            Serial.printf("Tiêu hao: %.3f grams\n", used); // In khối lượng dung dịch đã xịt ra
        } 
        else if (cmd == 'c' || cmd == 'C') { // Nếu người dùng gõ chữ 'c' hoặc 'C'
            dispenser.autoCalibrate();       // Chạy quy trình tự động hiệu chuẩn
        }
    }
}