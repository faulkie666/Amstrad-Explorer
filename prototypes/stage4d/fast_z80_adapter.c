#include "fast_z80_adapter.h"

#include <stdlib.h>
#include <string.h>

#include "cpu/z80.h"

struct fast_z80 {
    z80_t* core;
    fast_z80_callbacks callbacks;
};

static uint8_t mem_read(z80_t* core, uint16_t address, int m1, void* opaque) {
    fast_z80* cpu = (fast_z80*)opaque;
    return cpu->callbacks.mem_read(cpu->callbacks.user, address, m1 != 0,
                                   core->op_tstate);
}

static void mem_write(z80_t* core, uint16_t address, uint8_t value,
                      void* opaque) {
    fast_z80* cpu = (fast_z80*)opaque;
    cpu->callbacks.mem_write(cpu->callbacks.user, address, value,
                             core->op_tstate);
}

static uint8_t io_read(z80_t* core, uint16_t port, void* opaque) {
    fast_z80* cpu = (fast_z80*)opaque;
    return cpu->callbacks.io_read(cpu->callbacks.user, port, core->op_tstate);
}

static void io_write(z80_t* core, uint16_t port, uint8_t value, void* opaque) {
    fast_z80* cpu = (fast_z80*)opaque;
    cpu->callbacks.io_write(cpu->callbacks.user, port, value,
                            core->op_tstate);
}

static uint8_t int_read(z80_t* core, void* opaque) {
    (void)core;
    (void)opaque;
    return 0xFF;
}

static void int_ack(z80_t* core, void* opaque) {
    fast_z80* cpu = (fast_z80*)opaque;
    if (cpu->callbacks.intack) {
        cpu->callbacks.intack(cpu->callbacks.user, core->op_tstate);
    }
}

fast_z80* fast_z80_create(const fast_z80_callbacks* callbacks) {
    if (!callbacks || !callbacks->mem_read || !callbacks->mem_write ||
        !callbacks->io_read || !callbacks->io_write) return NULL;
    fast_z80* cpu = (fast_z80*)calloc(1, sizeof(*cpu));
    if (!cpu) return NULL;
    cpu->callbacks = *callbacks;
    cpu->core = z80_create(mem_read, cpu, mem_write, cpu, io_read, cpu,
                           io_write, cpu, int_read, cpu);
    if (!cpu->core) {
        free(cpu);
        return NULL;
    }
    z80_set_intack(cpu->core, int_ack, cpu);
    return cpu;
}

void fast_z80_destroy(fast_z80* cpu) {
    if (!cpu) return;
    z80_destroy(cpu->core);
    free(cpu);
}

void fast_z80_load_state(fast_z80* cpu, const fast_z80_state* state) {
    z80_reset(cpu->core);
    cpu->core->af.w = state->af;
    cpu->core->bc.w = state->bc;
    cpu->core->de.w = state->de;
    cpu->core->hl.w = state->hl;
    cpu->core->af2.w = state->af2;
    cpu->core->bc2.w = state->bc2;
    cpu->core->de2.w = state->de2;
    cpu->core->hl2.w = state->hl2;
    cpu->core->ix.w = state->ix;
    cpu->core->iy.w = state->iy;
    cpu->core->wz.w = state->wz;
    cpu->core->sp = state->sp;
    cpu->core->pc = state->pc;
    cpu->core->i = state->i;
    cpu->core->r = state->r;
    cpu->core->im = state->im;
    cpu->core->iff1 = state->iff1;
    cpu->core->iff2 = state->iff2;
    cpu->core->halted = state->halted;
    cpu->core->cycles = 0;
    cpu->core->total_cycles = state->total_cycles;
}

void fast_z80_get_state(const fast_z80* cpu, fast_z80_state* state) {
    memset(state, 0, sizeof(*state));
    state->af = cpu->core->af.w;
    state->bc = cpu->core->bc.w;
    state->de = cpu->core->de.w;
    state->hl = cpu->core->hl.w;
    state->af2 = cpu->core->af2.w;
    state->bc2 = cpu->core->bc2.w;
    state->de2 = cpu->core->de2.w;
    state->hl2 = cpu->core->hl2.w;
    state->ix = cpu->core->ix.w;
    state->iy = cpu->core->iy.w;
    state->wz = cpu->core->wz.w;
    state->sp = cpu->core->sp;
    state->pc = cpu->core->pc;
    state->i = cpu->core->i;
    state->r = cpu->core->r;
    state->im = cpu->core->im;
    state->iff1 = cpu->core->iff1;
    state->iff2 = cpu->core->iff2;
    state->halted = cpu->core->halted;
    state->total_cycles = cpu->core->total_cycles;
}

int fast_z80_step(fast_z80* cpu) { return z80_step(cpu->core); }
void fast_z80_raise_int(fast_z80* cpu) { z80_int(cpu->core); }
void fast_z80_add_wait(fast_z80* cpu, int cycles) {
    if (cycles > 0) z80_add_wait_states(cpu->core, cycles);
}

