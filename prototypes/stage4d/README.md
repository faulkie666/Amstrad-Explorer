# Stage 4D instruction-scheduler prototype

This is an isolated host-only feasibility experiment. It is deliberately not
part of CPC Explorer's top-level CMake build and cannot produce firmware.

The prototype restores CPC464 SNA state into both the established chips engine
and a reviewed instruction-oriented Z80 candidate. The candidate reports each
memory/I/O machine-cycle timestamp to an adapter which advances the existing
gate-array model and applies WAIT states. It compares CPU registers, RAM,
framebuffer, gate array, CRTC, PPI and PSG state at fixed instruction counts.

For Manic Miner it also applies a deterministic input schedule to both engines:
Q down/up at instructions 1000/1500, W down/up at 2000/2500, and bare Shift
down/up at 3000/3250.

## Build and run

The selected upstream core uses GNU computed-goto syntax, so this Windows host
prototype requires Clang or GCC rather than MSVC. The exact reviewed upstream
revision is pinned by `FetchContent`.

```powershell
cmake -S prototypes/stage4d -B build-host-stage4d-clang -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  '-DCMAKE_C_COMPILER=D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang.exe' `
  '-DCMAKE_CXX_COMPILER=D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang++.exe' `
  '-DCMAKE_RC_COMPILER=C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe'
cmake --build build-host-stage4d-clang --parallel
./build-host-stage4d-clang/cpc_stage4d_prototype.exe `
  roms/cpc464.rom snapshots/knight-tyme.sna `
  artifacts/stage4c1a-manic-miner/manic-miner-native.sna
```

Exit status 0 means all differential checkpoints matched; exit status 1 means
the prototype honestly found a difference. Stage 4D currently returns 1 and is
therefore a HOLD, not an integration candidate.

Stage 4D.1 adds first-instruction pin/timebase tracing and includes absolute
gate-array sequencer and WAIT/INT/SYNC state in the differential gate. It proves
the loaded-SNA origin can be aligned for BASIC and Manic Miner, but also exposes
that the candidate core's bus callbacks do not retain the pin-level placement
of all internal T-states. See `docs/STAGE4D1_EVENT_SCHEDULER.md`.
