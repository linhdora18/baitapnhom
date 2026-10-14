// Kiosk controller safety core: xác thực lệnh, chạy đúng 1 liều lượng phun đã được chuẩn hóa,
// kiểm tra cửa / kho / thời gian chờ / rò rỉ, và ước tính lượng dung dịch tồn kho.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "hal.h"
#include "ledger.h"
#include "protocol.h"
using namespace std;

namespace ss {
    // Các trạng thái hoạt động chính của máy (State Machine)
    enum class Mode { IDLE, DISPENSING, MAINTENANCE, FAULT };
    const char* mode_name(Mode m);

    // Cấu hình cho từng vị trí cắm chai nước hoa (Slot)
    struct SlotConfig {
        bool enabled = true;           // Cho phép hoạt động hay không
        uint32_t dose_ms = 120;        // Thời gian phun tiêu chuẩn đã được hiệu chuẩn (ms)
        uint32_t max_dose_ms = 250;    // Giới hạn thời gian phun TỐI ĐA (bảo vệ phần cứng)
        int32_t mg_per_spray = 90;     // Khối lượng tính bằng mg cho mỗi lần phun
        int32_t empty_mg = 0;          // Trọng lượng khi chai rỗng
        int32_t min_safe_liquid_mg = 300;   // Mức dung dịch tối thiểu để máy không báo cạn
        int32_t low_sprays = 20;       // Cảnh báo LOW (thấp) khi số lần phun còn lại ít hơn mức này
    };

    // Cấu hình chung của toàn bộ thiết bị
    struct Config {
        int slot_count = 5;                   // Số lượng slot (5 slot cho 5 brand)
        uint32_t cooldown_ms = 1500;          // Thời gian làm mát bắt buộc giữa 2 lần phun
        uint32_t settle_ms = 600;             // Thời gian chờ cảm biến cân ổn định
        uint32_t overrun_margin_ms = 50;      // Biên độ thời gian lố an toàn cho van (watchdog)
        uint32_t sample_interval_ms = 10000;  // Chu kỳ lấy mẫu kiểm tra rò rỉ/tồn kho
        int64_t max_window_s = 120;           // Thời gian sống tối đa của một lệnh điều khiển
        int64_t skew_s = 5;                   // Độ lệch thời gian đồng hồ cho phép
        double leak_tolerance_mg = 400;       // Ngưỡng thất thoát bất thường (cảnh báo Rò rỉ)
        double divergence_pct = 15;           // Mức chênh lệch % giữa ước tính và thực tế
        SlotConfig slots[MAX_SLOTS];
    };

    struct Ack {string command_id; bool accepted; string code; };

    // Kết quả sau khi thực hiện phun
    struct Result {
        string command_id, order_id, code;
        Outcome outcome = Outcome::UNCERTAIN;
        int32_t delta_mg = 0;
        uint32_t executed_ms = 0;
        bool diagnostic = false;
    };

    // Cờ trạng thái lỗi/cảnh báo cho từng slot
    enum SlotFlag : uint8_t { FLAG_LOW = 1, FLAG_EMPTY = 2, FLAG_LEAK = 4, FLAG_DIVERGENT = 8, FLAG_SENSOR = 16 };

    // Trạng thái hiện tại của một slot
    struct SlotStatus {
        int32_t weight_mg = -1;            // Cân nặng hiện tại
        int32_t remaining_sprays = -1;     // Số lần phun còn lại
        uint32_t sale_sprays = 0, diag_sprays = 0;   // Đếm số lần phun khách mua / test lỗi
        uint8_t flags = 0;
    };

    struct Event { uint32_t seq; string id, type, detail; };

    class Device {
        public:
            Device(Hal& hal, const string& key, const string& device_id, const Config& cfg);

            void begin();                                // Khởi tạo ban đầu khi bật máy
            Ack on_command(const Command& c);            // Xử lý và xác thực lệnh phun từ server
            void tick();                                 // Hàm chạy lặp liên tục (chu kỳ <= 10ms)

            // ---- Các thao tác của kỹ thuật viên / kiểm kho ----
            bool enter_maintenance();                    // Vào chế độ bảo trì
            bool exit_maintenance();                     // Thoát bảo trì
            bool clear_fault();                          // Xóa lỗi (nếu cảm biến rò rỉ đã khô)
            void install_bottle(int slot, int32_t measured_mg);   // Cài đặt chai mới, reset bộ đếm

            Mode mode() const;
            const SlotStatus& slot(int i) const { return slots_[i]; }
            const string& fault() const { return fault_; }
            bool results_pending() const { return !results_.empty(); }
            vector<Result> pending_results() const { return results_; }
            void confirm_result(const string& command_id);
            vector<Event> drain_events();

    private:
        // Trạng thái đang phun hiện tại
        struct Active { Command cmd; uint64_t start_ms = 0, stop_ms = 0; int32_t baseline_mg = 0; bool off = false; };
        Ack reject(const Command& c, const char* code);
        void finish(Outcome o, const string& code, int32_t delta_mg);
        void latch_fault(const string& code);
        void sample_inventory();
        void event(const char* type, const string& detail);
        int32_t liquid_mg(int slot, int32_t weight) const { return weight - cfg_.slots[slot].empty_mg; }

        Hal& hal_;
        string key_, id_;
        Config cfg_;
        Ledger ledger_;
        SlotStatus slots_[MAX_SLOTS];
        uint32_t actuations_[MAX_SLOTS] = {0}, actuations_at_sample_[MAX_SLOTS] = {0};
        int32_t install_mg_[MAX_SLOTS] = {0};
        uint32_t sprays_since_install_[MAX_SLOTS] = {0};
        bool active_ = false, maint_ = false, diag_ok_ = false;
        Active cur_;
        uint64_t last_end_ms_ = 0, last_sample_ms_ = 0;
        bool had_spray_ = false;
        string fault_;
        vector<Result> results_;
        vector<Event> events_;
        uint32_t seq_ = 0, boot_count_ = 0;
    };
}