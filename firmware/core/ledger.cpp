#include "ledger.h"

#include <cstdlib>
#include <sstream>

namespace ss {
    static const char* KEY = "ledger";

    const char* outcome_name(Outcome o) {
        switch (o) {
            case Outcome::IN_PROGRESS: return "IN_PROGRESS";
            case Outcome::OK: return "OK";
            case Outcome::FAILED: return "FAILED";
        default: return "UNCERTAIN";
        }
    }

    static Outcome outcome_from(const string& s) {
        if (s == "OK") return Outcome::OK;
        if (s == "FAILED") return Outcome::FAILED;
        if (s == "UNCERTAIN") return Outcome::UNCERTAIN;
        return Outcome::IN_PROGRESS;
    }

    // Đọc lịch sử lên RAM
    void Ledger::load() {
        v_.clear();
        string blob;
        if (!hal_.storage_read(KEY, &blob)) return;
        istringstream in(blob);
        string line;
        while (getline(in, line)) {          // command|order|OUTCOME|code|delta|diag
            vector<string> f;
            string cur;
            for (char ch : line) {
                if (ch == '|') { f.push_back(cur); cur.clear(); } else cur.push_back(ch);
            }
            f.push_back(cur);
            if (f.size() != 6) continue;            // Bỏ qua nếu dữ liệu bị rách/hỏng do mất nguồn lúc đang lưu
            LedgerEntry e;
            e.command_id = f[0]; e.order_id = f[1]; e.outcome = outcome_from(f[2]); e.code = f[3];
            e.delta_mg = atoi(f[4].c_str()); e.diagnostic = f[5] == "1";
            v_.push_back(e);
        }
    }

    // Lưu toàn bộ mảng lịch sử xuống bộ nhớ cứng dạng văn bản thô
    bool Ledger::persist() {
        string blob;
        for (const auto& e : v_)
            blob += e.command_id + "|" + e.order_id + "|" + outcome_name(e.outcome) + "|" + e.code + "|" + to_string(e.delta_mg) + "|" + (e.diagnostic ? "1" : "0") + "\n";
        return hal_.storage_write(KEY, blob);
    }

    const LedgerEntry* Ledger::find_command(const string& id) const {
        for (const auto& e : v_) if (e.command_id == id) return &e;
        return nullptr;
    }

    const LedgerEntry* Ledger::find_order(const string& id) const {
        for (const auto& e : v_) if (!e.diagnostic && e.order_id == id) return &e;
        return nullptr;
    }

    bool Ledger::add(const LedgerEntry& e) {
        vector<LedgerEntry> backup = v_;
        v_.push_back(e);
        if (v_.size() > cap_) v_.erase(v_.begin()); // Cuốn chiếu vòng lặp bộ nhớ nếu đầy
        if (!persist()) { v_ = backup; return false; } // Nếu lưu thất bại thì lùi lại (rollback) và báo lỗi
        return true;
    }

    bool Ledger::update(const string& command_id, Outcome o, const string& code, int32_t delta_mg) {
        for (auto& e : v_) {
        if (e.command_id == command_id) {
            e.outcome = o; e.code = code; e.delta_mg = delta_mg;
            return persist();
        }
    }
    return false;
    }
}