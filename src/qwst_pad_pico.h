#pragma once

#include <cstdint>

namespace cpcexplorer::qwst::pico {

void init();
std::uint16_t poll(std::uint32_t now_ms);
bool connected();
std::uint8_t address();

}  // namespace cpcexplorer::qwst::pico
