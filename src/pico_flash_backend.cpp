#include "pico_flash_backend.h"

#include "game_library.h"

#include <cstdio>
#include <cstring>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include "pico/platform.h"

namespace cpcexplorer::storage {
namespace {
struct JedecOp { std::uint8_t tx[4]; std::uint8_t rx[4]; };
struct EraseOp { std::uint32_t offset; std::size_t size; };
struct ProgramOp { std::uint32_t offset; const std::uint8_t* data; std::size_t size; };
void __not_in_flash_func(read_jedec)(void* value) {
    auto* op = static_cast<JedecOp*>(value);
    flash_do_cmd(op->tx, op->rx, sizeof(op->tx));
}
void __not_in_flash_func(do_erase)(void* value) {
    auto* op = static_cast<EraseOp*>(value);
    flash_range_erase(op->offset, op->size);
}
void __not_in_flash_func(do_program)(void* value) {
    auto* op = static_cast<ProgramOp*>(value);
    flash_range_program(op->offset, op->data, op->size);
}
}  // namespace

bool PicoFlash::probe() {
    JedecOp op{{0x9f, 0, 0, 0}, {0, 0, 0, 0}};
    last_sdk_error_ = flash_safe_execute(read_jedec, &op, 1000);
    if (last_sdk_error_ != PICO_OK) {
        std::snprintf(identity_, sizeof(identity_), "probe-error:%d", last_sdk_error_);
        return false;
    }
    std::snprintf(identity_, sizeof(identity_), "%02X-%02X-%02X", op.rx[1], op.rx[2], op.rx[3]);
    // Winbond W25Q128JV: manufacturer EF, memory type 40, 128-Mbit capacity 18.
    ready_ = op.rx[1] == 0xef && op.rx[2] == 0x40 && op.rx[3] == 0x18;
    capacity_ = ready_ ? 16U * 1024U * 1024U : 0;
    return ready_;
}

const std::uint8_t* PicoFlash::mapped_library() const {
    return reinterpret_cast<const std::uint8_t*>(XIP_BASE + library::kLibraryFlashOffset);
}

bool PicoFlash::erase(std::uint32_t offset, std::size_t size) {
    if (!ready_ || (offset & (FLASH_SECTOR_SIZE - 1)) != 0 ||
        (size & (FLASH_SECTOR_SIZE - 1)) != 0 ||
        !library::is_library_flash_range(offset, static_cast<std::uint32_t>(size))) return false;
    EraseOp op{offset, size};
    last_sdk_error_ = flash_safe_execute(do_erase, &op, 5000);
    return last_sdk_error_ == PICO_OK;
}

bool PicoFlash::program(std::uint32_t offset, const std::uint8_t* data, std::size_t size) {
    if (!ready_ || data == nullptr || (offset & (FLASH_PAGE_SIZE - 1)) != 0 ||
        (size & (FLASH_PAGE_SIZE - 1)) != 0 ||
        !library::is_library_flash_range(offset, static_cast<std::uint32_t>(size))) return false;
    ProgramOp op{offset, data, size};
    last_sdk_error_ = flash_safe_execute(do_program, &op, 1000);
    return last_sdk_error_ == PICO_OK;
}

}  // namespace cpcexplorer::storage
