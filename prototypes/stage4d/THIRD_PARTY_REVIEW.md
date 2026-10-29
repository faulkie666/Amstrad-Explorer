# Stage 4D third-party review

This directory is an isolated host-only feasibility prototype. It is not
linked into CPC Explorer firmware.

## Selected CPU engine

- Project: `cpu-z80` from `michalhucik/z80-mz800`
- Upstream: <https://github.com/michalhucik/z80-mz800>
- Reviewed revision: `687840df5b449e3b8ee31bf4cbc20f4ec6cf6d59`
- Declared version at that revision: 0.3.5
- Licence: MIT, copyright (c) 2026 Michal Hucik
- Source used by the prototype: `cpu-z80/cpu/z80.c`, `z80_execute.inc`,
  `z80.h`, and `utils/types.h`

The MIT licence permits use, modification and redistribution provided its
copyright and permission notice are retained. The prototype downloads the
reviewed revision without copying it into the production source tree. Any
future firmware integration must retain the upstream MIT notice in source
and binary distribution materials and must undergo a fresh revision review.

## Technical fit

The selected core provides instruction-level stepping, exact instruction
T-state totals, per-M-cycle `op_tstate`, memory and I/O callbacks, injected
wait states, interrupt acknowledge callbacks, documented and undocumented
instructions, F3/F5 flags, MEMPTR/WZ, EI delay and HALT/interrupt behaviour.
The upstream repository reports ZEXALL passing through both execution modes.

This makes it suitable for investigating an instruction scheduler. It does
not prove CPC compatibility: CPC gate-array contention, interrupt phase and
peripheral ordering remain responsibilities of this prototype.

## Rejected candidates

- `redcode/Z80`: technically mature, but LGPL-3.0 static embedded use would
  require a distribution and relinking strategy that has not been approved.
- `z80ex`: GPL-2.0; unsuitable for an isolated permissive prototype intended
  as a possible firmware component without a project-wide licence decision.
- `superzazu/z80`: MIT and ZEXALL tested, but exposes only instruction-level
  cycle totals and lacks the per-M-cycle timing needed to model CPC WAIT
  alignment and I/O ordering.

No third-party source has been added to a production target.

