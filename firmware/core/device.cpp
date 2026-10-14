#include "device.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ss {
    const char* mode_name(Mode m) {
        switch (m) {
            case Mode::IDLE: return "IDLE";
            case Mode::DISPENSING: return "DISPENSING";
            case Mode::MAINTENANCE: return "MAINTENANCE";
            default: return "FAULT";
        }
    }

    Device::Device(Hal& hal, const string& key, const string& device_id, const Config& cfg)
        : hal_(hal), key_(key), id_(device_id), cfg_(cfg), ledger_(hal) {}

    // Xác định trạng thái máy hiện tại dựa trên các cờ nội bộ
    Mode Device::mode() const {
        if (active_) return Mode::DISPENSING;
        if (maint_) return Mode::MAINTENANCE;
        if (!fault_.empty()) return Mode::FAULT;
        return Mode::IDLE;
    }

    void Device::event(const char* type, const string& detail) {
        if (events_.size() >= 100) events_.erase(events_.begin());       // Giữ buffer 100 sự kiện, xóa cái cũ nhất
        ++seq_;
        events_.push_back({seq_, id_ + "-" + to_string(boot_count_) + "-" + to_string(seq_), type, detail});
    }

    vector<Event> Device::drain_events() {vector<Event> e; e.swap(events_); return e; }

    // Khóa lỗi thiết bị cứng, chuyển sang chế độ FAULT
    void Device::latch_fault(const string& code) {
        if (fault_.empty()) event("FAULT", code);
        fault_ = code;
    }

    // Hàm khởi động: Luôn ưu tiên tắt toàn bộ van để đảm bảo an toàn
    void Device::begin() {
        hal_.all_actuators_off();                       // Không bao giờ khởi động máy với một van đang mở
        string bc;
        boot_count_ = hal_.storage_read("bootcount", &bc) ? static_cast<uint32_t>(strtoul(bc.c_str(), nullptr, 10)) + 1 : 1;
        hal_.storage_write("bootcount", to_string(boot_count_));
        ledger_.load();
        // Xử lý sự cố mất điện giữa chừng: nếu có lệnh đang dở dang thì đánh dấu là KHÔNG CHẮC CHẮN
        for (const auto& e : ledger_.entries()) {
            if (e.outcome != Outcome::IN_PROGRESS) continue;
            ledger_.update(e.command_id, Outcome::UNCERTAIN, "POWER_LOSS", 0);
            Result r; r.command_id = e.command_id; r.order_id = e.order_id; r.outcome = Outcome::UNCERTAIN; r.code = "POWER_LOSS"; r.diagnostic = e.diagnostic;
            results_.push_back(r);
        }
        event("STARTUP", "boot " + to_string(boot_count_));
        last_sample_ms_ = hal_.millis();
        sample_inventory();
    }   

    Ack Device::reject(const Command& c, const char* code) {
        event("COMMAND_REJECTED", c.command_id + ":" + code);
        return {c.command_id, false, code};
    }

    // Nơi tiếp nhận và KIỂM TRA AN TOÀN trước khi thực thi 1 lệnh phun
    Ack Device::on_command(const Command& c) {
        // 0. Cú pháp
        if (!valid_id(c.command_id) || !valid_id(c.device_id) || !valid_id(c.order_id) || c.action == Action::UNKNOWN) return reject(c, "BAD_FORMAT");
        
        // 1. Kiểm tra ID thiết bị và xác thực chữ ký (chống hacker)
        if (c.device_id != id_) return reject(c, "WRONG_DEVICE");
        if (!verify_signature(key_, c)) return reject(c, "BAD_SIGNATURE");
        
        // 2. Tính mới của lệnh (chống Replay Attack bằng thời gian)
        int64_t now = hal_.unix_time();
        if (now <= 0) return reject(c, "TIME_NOT_SYNCED");
        if (c.expires_at <= c.issued_at || c.expires_at - c.issued_at > cfg_.max_window_s) return reject(c, "BAD_WINDOW");
        if (now > c.expires_at) return reject(c, "EXPIRED");
        if (c.issued_at > now + cfg_.skew_s) return reject(c, "FROM_FUTURE");
        
        // 3. Tính độc nhất (Idempotency): Một đơn hàng không bao giờ được phun 2 lần
        if (ledger_.find_command(c.command_id)) return reject(c, "DUPLICATE_COMMAND");
        if (c.action == Action::SPRAY && ledger_.find_order(c.order_id)) return reject(c, "DUPLICATE_ORDER");
        
        // 4. KIỂM TRA ĐIỀU KIỆN PHẦN CỨNG (Safety Interlocks)
        if (active_) return reject(c, "BUSY");
        if (hal_.estop_pressed()) return reject(c, "ESTOP");
        if (hal_.leak_detected()) return reject(c, "LEAK_DETECTED");
        if (hal_.door_open()) return reject(c, "DOOR_OPEN");
        if (c.action == Action::SPRAY) {
            if (maint_) return reject(c, "MAINTENANCE_MODE");
            if (!fault_.empty()) return reject(c, "CRITICAL_FAULT");
            } else if (!maint_) {
            return reject(c, "REQUIRES_MAINTENANCE_MODE");
            }
            
        // 5. Kiểm tra vị trí chai nước hoa và dung lượng
        if (c.slot < 0 || c.slot >= cfg_.slot_count) return reject(c, "BAD_SLOT");
        const SlotConfig& sc = cfg_.slots[c.slot];
        if (!sc.enabled) return reject(c, "SLOT_DISABLED");
        if (c.dose_ms == 0 || c.dose_ms > sc.max_dose_ms) return reject(c, "DOSE_OUT_OF_RANGE");
        if (had_spray_ && hal_.millis() - last_end_ms_ < cfg_.cooldown_ms) return reject(c, "COOLDOWN");
        int32_t w = 0;
        if (!hal_.read_weight_mg(c.slot, &w)) { slots_[c.slot].flags |= FLAG_SENSOR; return reject(c, "SENSOR_FAILURE"); }
        if (liquid_mg(c.slot, w) < max(sc.min_safe_liquid_mg, sc.mg_per_spray)) return reject(c, "SLOT_EMPTY");

        // 6. Lưu vào bộ nhớ flash TRƯỚC KHI mở van (để đảm bảo không bị phun lặp nếu mất điện)
        LedgerEntry e; e.command_id = c.command_id; e.order_id = c.order_id; e.outcome = Outcome::IN_PROGRESS;
        e.diagnostic = c.action == Action::DIAG_SPRAY;
        if (!ledger_.add(e)) return reject(c, "STORAGE_FAILURE");
        
        // --- CHÍNH THỨC MỞ VAN PHUN ---
        cur_ = Active(); cur_.cmd = c; cur_.baseline_mg = w; cur_.start_ms = hal_.millis();
        active_ = true;
        actuations_[c.slot]++;
        hal_.actuator(c.slot, true);
        return {c.command_id, true, "ACCEPTED"};
    }

    // Xử lý sau khi kết thúc phun (đóng van)
    void Device::finish(Outcome o, const string& code, int32_t delta_mg) {
        const Command& c = cur_.cmd;
        uint32_t executed = static_cast<uint32_t>((cur_.stop_ms ? cur_.stop_ms : hal_.millis()) - cur_.start_ms);
        ledger_.update(c.command_id, o, code, delta_mg);
        Result r; r.command_id = c.command_id; r.order_id = c.order_id; r.outcome = o; r.code = code; r.delta_mg = delta_mg;
        r.executed_ms = executed; r.diagnostic = c.action == Action::DIAG_SPRAY;
        results_.push_back(r);
        if (o == Outcome::OK) {
            if (r.diagnostic) { slots_[c.slot].diag_sprays++; diag_ok_ = true; } else slots_[c.slot].sale_sprays++;
            sprays_since_install_[c.slot]++;
        }
        if (o != Outcome::OK) event("DISPENSE_PROBLEM", c.command_id + ":" + outcome_name(o) + ":" + code);
        active_ = false;
        had_spray_ = true;
        last_end_ms_ = hal_.millis();
    }

    // Hàm vòng lặp chính (Tick) liên tục kiểm tra quá trình hoạt động
    void Device::tick() {
        uint64_t now = hal_.millis();
        if (active_) {
            if (!cur_.off) {
                uint64_t elapsed = now - cur_.start_ms;
                const SlotConfig& sc = cfg_.slots[cur_.cmd.slot];
                
                // KIỂM TRA INTERLOCK LIÊN TỤC TRONG KHI PHUN
                const char* abort_code = hal_.estop_pressed() ? "ABORTED_ESTOP" : hal_.leak_detected() ? "ABORTED_LEAK" : hal_.door_open() ? "ABORTED_DOOR" : nullptr;
                if (abort_code) {                                     // Nếu có sự cố => Lập tức ngắt điện toàn bộ van
                    hal_.all_actuators_off(); cur_.off = true; cur_.stop_ms = now;
                    if (hal_.leak_detected()) latch_fault("LEAK");
                    finish(Outcome::UNCERTAIN, abort_code, 0);          
                    return;
                }
                
                // WATCHDOG AN TOÀN: Đảm bảo van không mở quá thời gian cho phép
                if (elapsed > static_cast<uint64_t>(sc.max_dose_ms) + cfg_.overrun_margin_ms) {   
                    hal_.all_actuators_off(); cur_.off = true; cur_.stop_ms = now;
                    latch_fault("ACTUATOR_OVERRUN");
                    finish(Outcome::UNCERTAIN, "ACTUATOR_OVERRUN", 0);
                    return;
                }
                
                // Hết thời gian liều lượng thì tắt van bình thường
                if (elapsed >= cur_.cmd.dose_ms) { hal_.actuator(cur_.cmd.slot, false); cur_.off = true; cur_.stop_ms = now; }
                return;
            }
            if (now - cur_.stop_ms < cfg_.settle_ms) return;
            
            // Đọc cân điện tử để kiểm tra xem dung dịch đã thực sự được xịt ra ngoài chưa
            const SlotConfig& sc = cfg_.slots[cur_.cmd.slot];
            int32_t after = 0;
            if (!hal_.read_weight_mg(cur_.cmd.slot, &after)) { slots_[cur_.cmd.slot].flags |= FLAG_SENSOR; finish(Outcome::UNCERTAIN, "SENSOR_FAILURE", 0); return; }
            int32_t delta = cur_.baseline_mg - after;
            double expected = static_cast<double>(sc.mg_per_spray) * cur_.cmd.dose_ms / sc.dose_ms;
            if (delta >= 0.5 * expected) finish(Outcome::OK, "OK", delta);
            else if (delta <= 0.1 * expected) finish(Outcome::FAILED, "NO_FLOW", delta);          // Xịt không ra gì
            else finish(Outcome::UNCERTAIN, "PARTIAL_FLOW", delta);                               
            return;
        }
        
        // Kiểm tra an toàn khi rảnh rỗi (rò rỉ, cân nặng)
        if (hal_.leak_detected()) latch_fault("LEAK");
        if (now - last_sample_ms_ >= cfg_.sample_interval_ms) { last_sample_ms_ = now; sample_inventory(); }
    }

    // Hàm lấy mẫu kiểm kê lượng tồn kho
    void Device::sample_inventory() {
        for (int i = 0; i < cfg_.slot_count; i++) {
            const SlotConfig& sc = cfg_.slots[i];
            SlotStatus& s = slots_[i];
            int32_t w = 0;
            if (!hal_.read_weight_mg(i, &w)) {
                if (!(s.flags & FLAG_SENSOR)) event("SENSOR_ERROR", "slot " + to_string(i));
                s.flags |= FLAG_SENSOR;
                continue;
            }
            s.flags &= ~FLAG_SENSOR;
            uint8_t before = s.flags;
            int32_t liquid = max<int32_t>(0, liquid_mg(i, w));
            s.remaining_sprays = sc.mg_per_spray > 0 ? liquid / sc.mg_per_spray : 0;
            
            // Cảnh báo hết/thấp dung dịch
            if (liquid < max(sc.min_safe_liquid_mg, sc.mg_per_spray)) s.flags |= FLAG_EMPTY; else s.flags &= ~FLAG_EMPTY;
            if (s.remaining_sprays <= sc.low_sprays) s.flags |= FLAG_LOW; else s.flags &= ~FLAG_LOW;
            
            // Phân tích rò rỉ: Trọng lượng giảm nhiều hơn số lần phun thực tế
            if (s.weight_mg >= 0) {
                double lost = s.weight_mg - w;
                double explained = (actuations_[i] - actuations_at_sample_[i]) * static_cast<double>(sc.mg_per_spray) * 1.5;   // +50 % sai số
                if (lost - explained > cfg_.leak_tolerance_mg) s.flags |= FLAG_LEAK;
            }
            
            // Ước tính phần mềm lệch với phần cứng
            if (install_mg_[i] > 0) {
                double est = install_mg_[i] - static_cast<double>(sprays_since_install_[i]) * sc.mg_per_spray;
                double span = max(1, install_mg_[i] - sc.empty_mg);
                if (fabs(est - w) / span * 100.0 > cfg_.divergence_pct) s.flags |= FLAG_DIVERGENT; else s.flags &= ~FLAG_DIVERGENT;
            }
            s.weight_mg = w;
            actuations_at_sample_[i] = actuations_[i];
            for (uint8_t f : {FLAG_LOW, FLAG_EMPTY, FLAG_LEAK, FLAG_DIVERGENT})
                if ((s.flags & f) && !(before & f)) event("SLOT_ALERT", "slot " + to_string(i) + " flag " + to_string(f));
        }
    }

    bool Device::enter_maintenance() {
        if (active_) return false;
        maint_ = true; diag_ok_ = false;
        event("MAINTENANCE_ENTER", "");
        return true;
    }

    bool Device::exit_maintenance() {
        if (!maint_ || !diag_ok_ || !fault_.empty()) return false;       // Bắt buộc phải phun thử không lỗi mới cho ra
        maint_ = false;
        event("MAINTENANCE_EXIT", "");
        return true;
    }

    bool Device::clear_fault() {
        if (!maint_ || hal_.leak_detected()) return false;
        fault_.clear();
        for (int i = 0; i < cfg_.slot_count; i++) slots_[i].flags &= ~FLAG_LEAK;
        event("FAULT_CLEARED", "");
        return true;
    }

    // Hàm gọi khi kỹ thuật thay chai nước hoa mới
    void Device::install_bottle(int slot, int32_t measured_mg) {
        if (slot < 0 || slot >= cfg_.slot_count) return;
        install_mg_[slot] = measured_mg;
        sprays_since_install_[slot] = 0;
        slots_[slot].sale_sprays = 0; slots_[slot].diag_sprays = 0;
        slots_[slot].weight_mg = measured_mg;
        slots_[slot].flags = 0;
        actuations_at_sample_[slot] = actuations_[slot];
        event("BOTTLE_INSTALLED", "slot " + to_string(slot) + " " + to_string(measured_mg) + "mg");
        sample_inventory();
    }

    void Device::confirm_result(const string& command_id) {
        results_.erase(remove_if(results_.begin(), results_.end(), [&](const Result& r) { return r.command_id == command_id; }), results_.end());
    }
}