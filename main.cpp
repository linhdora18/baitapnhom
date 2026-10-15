
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>

#include "../core/device.h"
#include "../core/protocol.h"
#include "../tests/mock_hal.h"

using namespace ss;

// Chỉ dùng cho mô phỏng cục bộ
static const std::string KEY = "0123456789abcdef0123456789abcdef";
static const std::string DEVICE_ID = "SS-0001";

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: bridge.exe <slot 1-5>\n";
        return 1;
    }

    int slot = 0;
    try {
        std::string input = argv[1];
        size_t pos = 0;
        slot = std::stoi(input, &pos);

        if (pos != input.size() || slot < 1 || slot > 5) {
            throw std::invalid_argument("Invalid slot");
        }
    } catch (...) {
        std::cerr << "BAD_SLOT\n";
        return 1;
    }

    MockHal hal;
    Config cfg;
    cfg.slot_count = 5;

    Device device(hal, KEY, DEVICE_ID, cfg);
    device.begin();

    Command cmd;
    cmd.command_id = "cmd-1";
    cmd.device_id = DEVICE_ID;
    cmd.order_id = "order-1";
    cmd.slot = slot;
    cmd.action = Action::SPRAY;
    cmd.dose_ms = 120;
    cmd.issued_at = hal.unix_time();
    cmd.expires_at = cmd.issued_at + 30;
    cmd.signature = sign(KEY, cmd);

    Ack ack = device.on_command(cmd);

    if (!ack.accepted) {
        std::cout << "REJECTED: " << ack.code << "\n";
        return 1;
    }

    std::cout << "ACCEPTED Slot " << slot << "\n";

    hal.run(1000, [&]() {
        device.tick();
    });

    auto results = device.pending_results();

    if (results.empty()) {
        std::cout << "NO_RESULT\n";
        return 1;
    }

    const Result& result = results.front();

    std::cout << "Result: " << result.code << "\n";
    std::cout << "Executed: " << result.executed_ms << " ms\n";
    std::cout << "Measured: " << result.delta_mg << " mg\n";
    std::cout << "Slot: " << slot << "\n";

    return result.outcome == Outcome::OK ? 0 : 1;
}
