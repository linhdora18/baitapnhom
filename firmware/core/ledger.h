#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "hal.h"
using namespace std;

namespace ss{
    // Các kết quả của một lệnh
    enum class Outcome{IN_PROGRESS, OK, FAILED, UNCERTAIN};
    const char* outcome_name(Outcome o);

    // Một bản ghi giao dịch đơn lẻ
    struct LedgerEntry{
        string command_id, order_id, code;
        Outcome outcome = Outcome::IN_PROGRESS;
        int32_t delta_mg = 0;
        bool diagnostic = false; // Thuộc chế độ test/bảo trì
    };

    class Ledger{
        public:
            // Sức chứa của sổ lưu trữ (Cần lớn hơn chu kỳ quay vòng bộ nhớ)
            explicit Ledger(Hal& hal, size_t capacity = 128) : hal_(hal), cap_(capacity) {}
            void load();                                     // Đọc toàn bộ lịch sử từ Flash khi máy mới bật
            const LedgerEntry* find_command(const string& id) const; // Tìm xem có trùng lặp Command ID không
            const LedgerEntry* find_order(const string& id) const;   // Tìm xem đơn hàng Order ID đã thực thi chưa
            bool add(const LedgerEntry& e);                  // Ghi lệnh xuống Flash TRƯỚC khi mở van
            bool update(const string& command_id, Outcome o, const string& code, int32_t delta_mg);
            const vector<LedgerEntry>& entries() const { return v_; }

        private:
            bool persist(); // Thao tác ghi chuỗi mã hóa xuống phần cứng qua lớp Hal
            Hal& hal_;
            size_t cap_;
            vector<LedgerEntry> v_;
    };
}