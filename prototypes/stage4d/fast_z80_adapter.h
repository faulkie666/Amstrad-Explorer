#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fast_z80 fast_z80;

typedef uint8_t (*fast_z80_mem_read_fn)(void*, uint16_t, bool, int);
typedef void (*fast_z80_mem_write_fn)(void*, uint16_t, uint8_t, int);
typedef uint8_t (*fast_z80_io_read_fn)(void*, uint16_t, int);
typedef void (*fast_z80_io_write_fn)(void*, uint16_t, uint8_t, int);
typedef void (*fast_z80_intack_fn)(void*, int);

typedef struct fast_z80_callbacks {
    fast_z80_mem_read_fn mem_read;
    fast_z80_mem_write_fn mem_write;
    fast_z80_io_read_fn io_read;
    fast_z80_io_write_fn io_write;
    fast_z80_intack_fn intack;
    void* user;
} fast_z80_callbacks;

typedef struct fast_z80_state {
    uint16_t af, bc, de, hl;
    uint16_t af2, bc2, de2, hl2;
    uint16_t ix, iy, wz, sp, pc;
    uint8_t i, r, im, iff1, iff2;
    bool halted;
    uint32_t total_cycles;
} fast_z80_state;

fast_z80* fast_z80_create(const fast_z80_callbacks* callbacks);
void fast_z80_destroy(fast_z80* cpu);
void fast_z80_load_state(fast_z80* cpu, const fast_z80_state* state);
void fast_z80_get_state(const fast_z80* cpu, fast_z80_state* state);
int fast_z80_step(fast_z80* cpu);
void fast_z80_raise_int(fast_z80* cpu);
void fast_z80_add_wait(fast_z80* cpu, int cycles);

#ifdef __cplusplus
}
#endif

