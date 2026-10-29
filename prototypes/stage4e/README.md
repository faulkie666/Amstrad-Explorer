# Stage 4E fast playable CPC464 prototype

This directory is a standalone, host-only feasibility prototype. It is not
included by the production `CMakeLists.txt`, does not build an RP2350 image and
does not change the deployed emulator.

## Architecture

- MIT-licensed instruction-oriented Z80 core, pinned by commit in the local
  `CMakeLists.txt` and adapted through the existing Stage 4D C wrapper.
- Existing CPC464 ROM/RAM mapping and existing CPC PPI, AY, gate-array and CRTC
  register I/O paths.
- Instruction-level cycle accounting with fixed CPC gate-array interrupt
  scheduling.
- Event/span CRTC scheduling: ordinary character clocks are consumed as a
  span; only scanline and VSYNC transitions are dispatched.
- Presentation-time reconstruction of the CPC raster directly from CRTC start
  address, RAM, gate-array mode and ink registers.
- CPU/game logic continues at the full emulated 4 MHz budget while the host
  display may be presented every fourth 50 Hz CPC frame (12.5 Hz).

The timing model is intentionally practical, not bit exact. It does not model
WAIT/contention, mid-scanline CRTC tricks, raster palette changes or exact AY
waveform timing. Those limitations make this suitable for a curated CPC464
snapshot library, not a general-purpose compatibility claim.

## Build and run

The dependency uses GNU computed-goto extensions, so use Clang or GCC rather
than MSVC. The tested Windows configuration was:

```powershell
cmake -S prototypes/stage4e -B build-host-stage4e-clang -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  '-DCMAKE_C_COMPILER=D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang.exe' `
  '-DCMAKE_CXX_COMPILER=D:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang++.exe' `
  '-DCMAKE_RC_COMPILER=C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe'
cmake --build build-host-stage4e-clang --parallel
build-host-stage4e-clang/cpc_stage4e_fast.exe `
  roms/cpc464.rom snapshots/knight-tyme.sna `
  artifacts/stage4c1a-manic-miner/manic-miner-native.sna `
  artifacts/stage4e-fast-evidence-final2
```

The executable returns success only if Manic Miner movement/jump, Knight Tyme
space-key response and BASIC keyboard/display checks pass. It prints both the
new fast benchmark and slower character-clock controls.

## Latest measured host result

On the development PC, one emulated second of the Manic Miner state measured:

| Path | Host time | Relative to established engine |
|---|---:|---:|
| Established per-T-state engine | 31.198 ms | 1.00x |
| Alternative Z80 only | 4.813 ms | 6.48x |
| Stage 4E span scheduler + 12.5 Hz presentation | 5.846 ms | 5.337x |
| Character-clock-compatible control | 13.694 ms | 2.278x |

The span scheduler dispatched 15,975 scanline/interrupt/frame events instead
of 1,000,001 character events per emulated second, a 98.4% reduction. These are
desktop measurements and do not predict RP2350 frame rate.

## Provenance and licensing

- `michalhucik/z80-mz800`, commit
  `687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`, MIT: selected Z80 dependency.
- `rpsubc8/ESP32TinyCPC`, commit
  `041dff33fc2a42bfd8598645f078d33ae4f35bf8`, WTFPL: architectural reference
  for scanline budgeting only; no code copied.
- `ColinPitrat/caprice32`, commit
  `a0d78f0f7c6d19a5d3205d1b231bd4da100ec6f3`, GPL-2.0: architectural
  reference for post-instruction peripheral cycle accounting only; no code
  copied.
- `EremusOne/CPCESP_alpha`, commit
  `f130f94bf60699ab466ef06c90b91a722b65298a`, GPLv3: reviewed but not reused.

Only the MIT core is compiled into the host prototype. No GPL/WTFPL source was
copied into CPC Explorer.
