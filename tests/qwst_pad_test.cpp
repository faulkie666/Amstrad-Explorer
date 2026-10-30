#include "qwst_pad.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct FakeBus {
    std::uint8_t address = 0x23;
    std::uint16_t inputs = 0;
    bool available = true;
    std::vector<std::vector<std::uint8_t>> writes;
};

bool write(void* context, std::uint8_t address, const std::uint8_t* data, std::size_t size) {
    auto& bus = *static_cast<FakeBus*>(context);
    if (!bus.available || address != bus.address) return false;
    bus.writes.emplace_back(data, data + size);
    return true;
}

bool write_read(void* context, std::uint8_t address, const std::uint8_t* request,
                std::size_t request_size, std::uint8_t* result, std::size_t result_size) {
    auto& bus = *static_cast<FakeBus*>(context);
    if (!bus.available || address != bus.address || request_size != 1 ||
        request[0] != 0 || result_size != 2) return false;
    result[0] = static_cast<std::uint8_t>(bus.inputs);
    result[1] = static_cast<std::uint8_t>(bus.inputs >> 8);
    return true;
}

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main() {
    using namespace cpcexplorer::qwst;
    FakeBus bus;
    Driver pad({&bus, write, write_read});
    auto sample = pad.poll(0);
    if (!expect(sample.connected && sample.address == 0x23,
                "detects supported alternate address") ||
        !expect(bus.writes.size() == 3, "writes exactly three configuration registers") ||
        !expect(bus.writes[0] == std::vector<std::uint8_t>({0x06, 0x3f, 0xf9}) &&
                bus.writes[1] == std::vector<std::uint8_t>({0x04, 0x3f, 0xf8}) &&
                bus.writes[2] == std::vector<std::uint8_t>({0x02, 0xc0, 0x06}),
                "configuration matches official TCA9555 reference")) return 1;

    bus.inputs = static_cast<std::uint16_t>((1U << 1) | (1U << 2) | (1U << 15) |
                                             (1U << 11));
    if (!expect(pad.poll(20).buttons == 0, "first changed sample is debounced") ||
        !expect(pad.poll(40).buttons == (Up | Left | X | Plus),
                "simultaneous buttons survive debounce") ||
        !expect(pad.poll(60).buttons == (Up | Left | X | Plus), "held buttons remain held"))
        return 2;

    bus.inputs = 0;
    if (!expect(pad.poll(80).buttons != 0, "release is debounced") ||
        !expect(pad.poll(100).buttons == 0, "debounced release clears all buttons")) return 3;

    bus.inputs = static_cast<std::uint16_t>((1U << 14) | (1U << 12) | (1U << 13) |
                                             (1U << 4) | (1U << 3) | (1U << 5));
    pad.poll(120);
    if (!expect(pad.poll(140).buttons == (A | B | Y | Down | Right | Minus),
                "remaining physical button bits decode correctly")) return 4;

    bus.available = false;
    if (!expect(!pad.poll(160).connected && pad.sample().buttons == 0,
                "disconnect immediately releases held buttons")) return 5;
    const auto writes_after_disconnect = bus.writes.size();
    if (!expect(!pad.poll(1'000).connected && bus.writes.size() == writes_after_disconnect,
                "reconnect probing is rate limited")) return 6;
    bus.available = true;
    if (!expect(pad.poll(1'160).connected && pad.sample().address == 0x23,
                "controller reconnects without restart")) return 7;

    std::puts("PASS: bounded-address detection, official mapping, debounce, holds, chords and reconnect");
    return 0;
}
