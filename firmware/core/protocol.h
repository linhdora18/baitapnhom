#pragma once
#include <cstdint>
#include <string>
using namespace std;

namespace ss{
    // Các lệnh hành động
    enum class Action{SPRAY, DIAG_SPRAY, UNKNOWN};

    // Khối thông tin điều khiển (Payload)
    struct Command{
        string command_id, device_id, order_id, signature;      // Chữ ký bảo mật dài 64 ký tự dạng HEX
        int slot = -1;
        Action action = Action::UNKNOWN;
        uint32_t dose_ms = 0;
        int64_t issued_at = 0, expires_at = 0;                 // Thời gian phát hành và hết hạn
    };

    const char* action_name(Action a);
    Action action_from_string(const string& s);
    bool valid_id(const string& s); // Bộ lọc tránh chuỗi mã độc (injection)

    string canonical(const Command& c); // Hàm nối chuỗi tiêu chuẩn hóa trước khi băm
    string sign(const string& key, const Command& c);         // Hàm tạo chữ ký số HMAC 
    bool verify_signature(const string& key, const Command& c);    // Hàm kiểm tra chữ ký (constant-time, chống side-channel attack)
    string hex_decode(const string& hex, bool* ok = nullptr);
}