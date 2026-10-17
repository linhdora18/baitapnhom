#include "protocol.h"
#include "sha256.h"

namespace ss {
    const char* action_name(Action a) {
        switch (a) {
            case Action::SPRAY: return "SPRAY";
            case Action::DIAG_SPRAY: return "DIAG_SPRAY";
            default: return "UNKNOWN";
        }
    }

    Action action_from_string(const string& s) {
        if (s == "SPRAY") return Action::SPRAY;
        if (s == "DIAG_SPRAY") return Action::DIAG_SPRAY;
        return Action::UNKNOWN;
    }

    // Xác thực an toàn chuỗi đầu vào (Chỉ cho phép các ký tự thông thường)
    bool valid_id(const string& s) {
        if (s.empty() || s.size() > 64) return false;
        for (char ch : s) {
            bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == ':' || ch == '-';
            if (!ok) return false;
        }
        return true;
    }

    // Chuỗi mẫu Canonical để server và firmware cùng sử dụng chung 1 công thức ký
    string canonical(const Command& c) {
        return "v1|" + c.command_id + "|" + c.device_id + "|" + to_string(c.slot) + "|" + action_name(c.action) + "|" +
            to_string(c.dose_ms) + "|" + to_string(c.issued_at) + "|" + to_string(c.expires_at) + "|" + c.order_id;
    }

    // Tạo chữ ký dựa trên Master Key
    string sign(const string& key, const Command& c) {
        string msg = canonical(c);
        uint8_t out[32];
        hmac_SHA256(reinterpret_cast<const uint8_t*>(key.data()), key.size(), reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), out);
        return to_hex(out, 32);
    }

    // Giải mã chuỗi HEX về Byte array
    string hex_decode(const string& hex, bool* ok) {
        string out;
        bool good = hex.size() % 2 == 0;
        auto nib = [&](char ch) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            good = false;
            return 0;
        };
        for (size_t i = 0; good && i + 1 < hex.size(); i += 2) out.push_back(static_cast<char>(nib(hex[i]) << 4 | nib(hex[i + 1])));
        if (ok) *ok = good;
        return good ? out : string();
    }

    // So khớp chữ ký đính kèm trong tin nhắn với chữ ký máy tự tính toán
    bool verify_signature(const string& key, const Command& c) {
        bool ok = false;
        string got = hex_decode(c.signature, &ok);
        if (!ok || got.size() != 32) return false;
        string want = hex_decode(sign(key, c));
        // Sử dụng hàm ct_equal (thời gian cố định) để ngăn chặn kẻ tấn công lợi dụng độ trễ vi xử lý
        return ct_equal(reinterpret_cast<const uint8_t*>(got.data()), reinterpret_cast<const uint8_t*>(want.data()), 32);   
    }
}