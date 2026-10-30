#include "qwst_pad_pico.h"

#include "qwst_pad.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"

namespace cpcexplorer::qwst::pico {
namespace {

constexpr unsigned kSdaPin = 20;
constexpr unsigned kSclPin = 21;
constexpr unsigned kBusHz = 400'000;
constexpr unsigned kOperationTimeoutUs = 2'000;

bool write(void*, std::uint8_t address, const std::uint8_t* data, std::size_t size) {
    return i2c_write_blocking_until(i2c0, address, data, size, false,
                                    make_timeout_time_us(kOperationTimeoutUs)) ==
           static_cast<int>(size);
}

bool write_read(void*, std::uint8_t address, const std::uint8_t* request,
                std::size_t request_size, std::uint8_t* result, std::size_t result_size) {
    if (i2c_write_blocking_until(i2c0, address, request, request_size, true,
                                 make_timeout_time_us(kOperationTimeoutUs)) !=
        static_cast<int>(request_size)) return false;
    return i2c_read_blocking_until(i2c0, address, result, result_size, false,
                                   make_timeout_time_us(kOperationTimeoutUs)) ==
           static_cast<int>(result_size);
}

Driver driver({nullptr, write, write_read});
Sample last{};

}  // namespace

void init() {
    i2c_init(i2c0, kBusHz);
    gpio_set_function(kSdaPin, GPIO_FUNC_I2C);
    gpio_set_function(kSclPin, GPIO_FUNC_I2C);
    gpio_pull_up(kSdaPin);
    gpio_pull_up(kSclPin);
    last = driver.poll(to_ms_since_boot(get_absolute_time()));
}

std::uint16_t poll(std::uint32_t now_ms) {
    last = driver.poll(now_ms);
    return last.buttons;
}

bool connected() { return last.connected; }
std::uint8_t address() { return last.address; }

}  // namespace cpcexplorer::qwst::pico
