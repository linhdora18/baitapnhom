#pragma once
#include <cstdint>
#include <string>
using namespace std;

namespace ss {
    constexpr int MAX_SLOTS = 8; // Số khe tối đa trên board mạch

    class Hal {
    public:
        virtual ~Hal() = default;
        
        virtual uint64_t millis() = 0;                 // Lấy mili-giây kể từ lúc khởi động máy
        virtual int64_t unix_time() = 0;               // Thời gian thực (UTC seconds); bằng 0 nếu chưa đồng bộ mạng
        
        // --- Điều khiển phần cứng ---
        virtual void actuator(int slot, bool on) = 0;  // Điều khiển đóng/mở van phun của slot (kích MOSFET)
        virtual void all_actuators_off() = 0;          // Cắt điện TOÀN BỘ van (Dùng khi Emergency Stop)
        
        // --- Các cảm biến an toàn ---
        virtual bool door_open() = 0;                  // Trạng thái công tắc cửa tủ thiết bị
        virtual bool leak_detected() = 0;              // Cảm biến rò rỉ dung dịch dưới khay hứng
        virtual bool estop_pressed() = 0;              // Trạng thái nút bấm dừng khẩn cấp (E-Stop)
        virtual bool read_weight_mg(int slot, int32_t* mg) = 0;  // Đọc dữ liệu từ load cell (cảm biến cân)
        
        // --- Bộ nhớ lưu trữ không bay hơi (NVS) ---
        virtual bool storage_read(const char* key, string* value) = 0;   // Đọc bộ nhớ Flash
        virtual bool storage_write(const char* key, const string& value) = 0; // Ghi vào bộ nhớ Flash
    };
}