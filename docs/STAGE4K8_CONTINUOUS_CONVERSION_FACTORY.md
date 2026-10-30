# Stage 4K.8 continuous conversion factory

## Result of this factory run

The complete Arcade and Simulation source trees were inventoried read-only. The
inventory contains 10,216 files with a SHA-256 for every source file and is stored in
`artifacts/stage4k8-factory/source-inventory.json`.

The physical device remained firmware `0.4.28-stage4k7migrationrecoverfix`, migration
active, and upload idle. Two Arcade packages passed every gate and were each committed
exactly once:

| Game | ID | Package SHA-256 | Result |
|---|---|---|---|
| Phantomas Tales Marsport | `phantomas_tales` | `7d56cb4d2b50d7721462e76bc1da661406585723aa94df3220ea05edc65ca138` | visible |
| BeTiled | `betiled` | `9a1c442d9771450c67d29709dbb0f4d18c84689ae555485457e1d675ad5ba71d` | visible |

Panzadrome became the first fully validated Simulation package. It was not installed:
the current incremental CPCLIB1 overlay is explicitly routed to Arcade in
`src/launcher.cpp`. Installing it would violate category preservation. Its deterministic
package is retained at `artifacts/stage4k8-factory/packages/panzadrome.cpcgame` with
SHA-256 `c2c900da92ef249380bb15e32a10e1d83bc3f4dcc171438d06d20450d45d6a8a`.

Timeboxed failures were quarantined immediately: Elite (incompatible command set),
Star Sabre (post-launch disk dependency), Magical Drop (menu automation did not reach
gameplay), Nanako (controls not proven), Project X and Fres Fighter II (128K-only), and
Bob Winner (protected two-disk source).

## Final device state

- 15 visible games;
- 16 committed records including preserved hidden `4kroid`;
- upload idle, migration active;
- active CPCLIB1 generation 17 with 12 descriptors;
- allocation high-water `0x511000`;
- 11,399,168 bytes / 163 aligned slots remain;
- final catalogue audit SHA-256 `e006aedc68aaa1ddb2a92f0dd298c934d77af090c1d648612ed6d7ed26a3ec27`;
- no firmware, migration, removal, repair, retry, erase, or rollback operation occurred.

The resumable processing and installation state is recorded in
`artifacts/stage4k8-factory/master-manifest.json`.

## Continued host factory and physical stop gate

The next continuation validated two more deterministic Arcade packages:

| Game | Package SHA-256 | Controls | Installation |
|---|---|---|---|
| Sewer Rat | `0b2dae82d2c35ba5c10c81ae13f9cfe73600eab00d44038af8ca21f8063a7112` | joystick + fire | not attempted |
| Stranded | `5a0c0b4eafdee008cc5fe1456ff1d0f7408f266d9ffa987d6502374932a9d28d` | QAOP + Space | host-ready |

Sewer Rat reached active gameplay after disk ejection and produced distinct response
hashes for all joystick directions and fire. Its deterministic package was prepared,
but the mandatory read-only catalogue audit timed out before the uploader was invoked.
No `BEGIN`, erase, program, commit, or other write command was sent. In accordance with
the campaign stop rule, all subsequent physical writes were stopped and no automatic
retry was attempted.

Stranded was then completed host-only. The documented `HELP` password reaches its real
maze, the snapshot reloads without media, and QAOP/Space produces deterministic state
changes. Commando: Space Invasion was quarantined when bounded documented start and
joystick-selection sequences could not advance its diskless title into gameplay.

The authoritative physical state therefore remains 15 visible games, 16 committed
records including hidden `4kroid`, migration active and upload idle. Sewer Rat and
Stranded are the next ready packages after a separately controlled read-only USB
recovery establishes a clean transport session.

## Limiting factor

Flash and catalogue capacity are not yet exhausted. The immediate architecture limit
is that incremental packages have no persisted Arcade/Simulation category field and
the launcher treats every CPCLIB1 overlay record as Arcade. Further Arcade conversion
can continue with the current firmware, but correct Simulation installation requires a
separately approved format-compatible metadata/launcher change. The validated
Panzadrome payload must not be installed under the wrong category.

## Latest host-only continuation

Physical access remained stopped after the catalogue-audit timeout. No further COM
session or device write was attempted. The factory produced two additional validated
Arcade packages:

| Game | Package SHA-256 | Evidence | Installation |
|---|---|---|---|
| Exterminator | `848b548cb351c145b2963e7d85cb8b6c1aa55b3acf26fc7788b494571b9748e6` | diskless reload reached live car/house gameplay; joystick-left and documented O/P/Q/A/Space state were exercised | host-ready |
| Red And Blue | `4695c76c5c479cd49844742f377d83b3c2c62f10bd7d24bdc4b998249f4ffd2b` | diskless reload reached the live puzzle board; all joystick directions and fire produced distinct framebuffer and RAM hashes | host-ready |

Buffalo Bill's Rodeo Games was quarantined after its authentic 64K loader advanced a
long opening sequence but bounded fire/Space probes did not demonstrate controllable
gameplay. The Running Man was quarantined because side A reaches an explicit request
for side B and therefore has an unresolved media dependency. Simulation received a
fresh deterministic loose-file test with 3D Sub: it reaches a diskless live cockpit
and C/A/M/T/Q input changes state, but its digit-leading title is unsupported by the
current A-Z launcher and its required C/M/T actions are absent from the deployed
controller profile. It was therefore quarantined without renaming or packaging.
Double Dragon was also timeboxed: its authentic loader remained on a visibly
corrupted crackers/title sequence and never supplied gameplay evidence.

The next Simulation probe, ACE, reached its authentic menu from a deterministic
loose-file AMSDOS build. A longer deterministic host key hold selected Start Game and
Multirole before capture, producing a diskless CPC464 snapshot that responds to every
joystick direction and fire. Its package is retained at
`artifacts/stage4k8-factory/packages/ace.cpcgame` with SHA-256
`ff87960932768a69ac4f30d6720922df43225afe624dfce16abe0112f050d3d2`.
Like Panzadrome, it is held because the deployed incremental overlay cannot preserve
the Simulation category. Wings Of Fury also accepts the longer numeric selection, but
then remains black while loading even with the source DSK mounted; it stays quarantined
for an unresolved post-menu media/loading dependency.

The reusable snapshot probe now accepts an optional bounded keyboard hold and optional
mounted DSK (`--key-hold-vbl` and `--disk-media`). Defaults preserve the prior probing
behaviour. This prevents valid titles with slowly sampled numeric menus from being
rejected merely because a five-VBL tap was too short, while still exposing post-menu
media dependencies such as Wings Of Fury.

Two further Simulation titles now pass the full host gate:

| Game | Package SHA-256 | Gameplay evidence | Installation |
|---|---|---|---|
| Fighter Pilot | `d3b0925ea4e7f4a68a67c1c4c4b655fd33a5908c97f415282a41262af05fa7ed` | deterministic Space/Enter take-off reaches live cockpit; all joystick inputs change framebuffer/RAM | held: category |
| ACE 2 | `54217495d78fbf333cff03526704be29cbe429377e0ec24e30a58b9bf704e8a4` | mounted-media menu transition reaches combat; captured snapshot then reloads diskless and all joystick inputs change state | held: category |

Both are valid CPC464/64 KiB packages. Neither is physically installed because the
deployed incremental format still routes CPCLIB1 overlay records to Arcade.

ACE of Aces required a fresh-boot sequence because snapshot restoration cannot
recreate a later physical-style disk change. The capture harness now accepts optional
`--capture-keys`, applying bounded keyboard/joystick inputs while the original DSK is
still mounted. A fresh `JOYF,JOYD,JOYF` Practice/Train sequence reached its live
cockpit; the subsequent snapshot continued after disk ejection and all joystick inputs
changed framebuffer/RAM state. The validated package is
`artifacts/stage4k8-factory/packages/ace_of_aces.cpcgame`, SHA-256
`729ef3ce0e53ae721c52b5532c2c4a2535867d09384636d67ba13b61b5adc8d4`.
It is held with the other Simulation packages pending category-safe installation.

Top Gun also passes after a deterministic fresh-boot sequence: answer `N` to infinite
lives, select one joystick, then start with Space. The captured split-screen flight
state reloads without media and all joystick inputs produce distinct RAM state. Its
package is `artifacts/stage4k8-factory/packages/top_gun.cpcgame`, SHA-256
`b132f2d76bef29cc0e1df65d49371eceab87ae78d2e81a4e3bc14a73740cdb9a`.
It remains held solely for Simulation category preservation.

Acrojet passes after deterministic registration, event selection and entry of the
lock code displayed by the game itself (`BURGINI`). The resulting cockpit state
reloads without media and all joystick directions/fire change framebuffer and RAM.
Package: `artifacts/stage4k8-factory/packages/acrojet.cpcgame`; SHA-256
`7e5c597181ea3679019dd25adbfa0bf7655529946802cf46cfa183546b3076ae`.
It is also held solely for Simulation category preservation.

F15 Strike Eagle now passes the same gate. A deterministic DSK assembled from the
original `F15.BAS`, three binary segments and `F15TEXT` reaches the mission selector.
The supplied text identifies authentication challenge 10 as `I`; entering `I` and
Enter reaches live cockpit flight without modifying the game. The captured snapshot
reloads without media, Space changes the view/state, and every joystick direction
and fire produces distinct framebuffer and RAM hashes. Package:
`artifacts/stage4k8-factory/packages/f15_strike.cpcgame`; SHA-256
`ba096f18fffcc454e73c2ce120f6c82e41ad6a1a7ae4e7f48b3ad57f85e08dba`.
It remains host-only because the deployed overlay cannot preserve Simulation.

F16 Combat Pilot also passes after a source-media-backed menu transition. Keyboard
Up selects Quick Start on the 3D mission cube and Space loads the cockpit. The saved
CPC464 state remains live after the DSK is ejected; sustained joystick probes alter
RAM for every direction/fire, with left/right also producing distinct framebuffer
states. Package: `artifacts/stage4k8-factory/packages/f16_combat.cpcgame`; SHA-256
`721a0230c57273f8f61df98ed2516cc48235dcfa5189dac57c2d6ddc5ed27e49`.
It is held for the same category-preservation reason.

Flight Path 737 passes from a deterministic two-file DSK. The bounded sequence selects
English, First Solo and Space-to-fly, producing a live runway/cockpit snapshot that
continues without its disk. Sustained joystick direction/fire probes produce distinct
RAM state and directional framebuffer changes. Package:
`artifacts/stage4k8-factory/packages/flight_path_737.cpcgame`; SHA-256
`beab137ff5290da7092b181efbd730239fd76b717e3fc93e7975550b6846d028`.
It remains held until Simulation category metadata can be preserved physically.

Jump Jet passes from its original BASIC/BIN set. Selecting practice level 1 enters a
live moving runway/cockpit state; the captured CPC464 snapshot continues without the
DSK and sustained joystick direction/fire probes all produce distinct framebuffer and
RAM states. Package: `artifacts/stage4k8-factory/packages/jump_jet.cpcgame`; SHA-256
`2c3a19cec564287966e301f7be70fd33e74d53db0bfa694911491c2e45d00aa0`.
It is held for correct Simulation categorisation.

Red Arrows passes without relying on its autonomous demo. The bounded sequence enters
Training flight and Solo flight practice, where the live cockpit snapshot continues
without media and sustained joystick direction/fire probes all change framebuffer and
RAM state. Package: `artifacts/stage4k8-factory/packages/red_arrows.cpcgame`; SHA-256
`10e5cf2351f9f10b1748423b2f6bfa412b647bd43455871b163e1c76ed6cdcd1`.
It remains held for correct Simulation categorisation.

Night Gunner was timeboxed and quarantined. Its authentic loader reaches the E/D/F
language prompt, but bounded English, Space, fire and Enter injection does not advance
it to gameplay. No package was produced from the prompt-only state.

Combat Lynx passes after selecting its built-in joystick option and skill 1. The live
helicopter battlefield snapshot reloads without media, and every sustained joystick
direction/fire probe produces distinct framebuffer and RAM state. Package:
`artifacts/stage4k8-factory/packages/combat_lynx.cpcgame`; SHA-256
`118240390abc152d28fca424d8b319d232d426970c4b22bab012398dd974265f`.
It is held for correct Simulation categorisation.

ATF - Advanced Tactical Fighter was timeboxed at its interactive armament/loadout
screen. The authentic loader and war-report path work, but bounded current-controller
fire, Space, Enter and directional sequences do not complete the loadout into flight.
The non-gameplay state was quarantined and not packaged.

## Deployment request and fail-closed preflight

After the user requested deployment, the factory stopped selecting new candidates and
started a fresh read-only device preflight. `cpc_game.py detect` returned `COM11`, but
the immediately following `status --port COM11` failed to open the port with Windows
`FileNotFoundError(2)`. No `BEGIN`, erase, program, commit, migration, activation or
other write command was issued. The physical write count remains zero and deployment
is HOLD until the Explorer is reconnected in normal application mode and a fresh,
stable status/list/migration/catalogue audit passes.

The currently deployable host queue contains Sewer Rat, Stranded, Exterminator and
Red And Blue. The validated Simulation packages remain intentionally held because the
installed incremental overlay would misclassify them as Arcade.

### Resumed deployment preflight

After the Explorer was reconnected, a new read-only session successfully confirmed
COM11, firmware `0.4.28-stage4k7migrationrecoverfix`, Winbond `EF-40-18` capacity
16,777,216 bytes, migration `active`, upload `idle`, and all 15 expected visible menu
entries. The mandatory `audit-catalogues` command then timed out before returning any
catalogue evidence; no output file was created. The uploader was not invoked and the
physical write count remains zero. The four approved Arcade package hashes were
rechecked unchanged. Deployment remains HOLD with no automatic retry.

The host-only recovery path is now `tools/stage4k8_catalogue_audit.py`. It divides the
same strict 32 KiB audit into eight exact 4 KiB windows, opens a bounded read-only
session for each, saves every completed window immediately, and stops at the first
failed window without retry. `--resume` accepts only manifest-hashed prior evidence;
after all eight windows it still requires the established aggregate SHA-256
`e006aedc68aaa1ddb2a92f0dd298c934d77af090c1d648612ed6d7ed26a3ec27`
and the existing CPCLIB1 slot parser. Focused filesystem, round-trip, corrupt-manifest
and short-window tests pass. This tool has not yet been run against the device.

The next safe physical action is still a separately controlled read-only transport
recovery. Until that passes, the ready queue is Sewer Rat, Stranded, Exterminator and
Red And Blue; upload attempts for all four remain zero.

## Physical audit and validated-queue deployment

The separately authorised eight-window audit completed successfully on COM11. All
eight 4 KiB windows were saved and independently verified; the reconstructed 32 KiB
catalogue SHA-256 was
`e006aedc68aaa1ddb2a92f0dd298c934d77af090c1d648612ed6d7ed26a3ec27`.
The active pre-deployment slot was generation 17 with 12 records and high-water
`0x111000`. Firmware, flash identity, migration state and upload state matched the
approved baseline.

The complete waiting queue was then installed with the existing transactional
uploader. Seventeen attempts were made and all seventeen committed successfully; no
attempt was retried. After every commit the device returned to `upload=idle`, the new
ID appeared exactly once, and all prior visible IDs remained present. The deployment
added four Arcade-source packages and thirteen Simulation-source packages. The
current incremental overlay presents both groups through its existing A-Z launcher;
it does not preserve a separate Simulation branch.

The final read-only state is firmware `0.4.28-stage4k7migrationrecoverfix`, migration
`active`, upload `idle`, 32 visible entries and 29 committed CPCLIB1 records. The sole
known hidden record remains `4kroid`. Both catalogue slots parse and validate: the
authoritative slot is generation 34, count 29, high-water `0x232000`; the fallback is
generation 33, count 28, high-water `0x221000`. The post-install 32 KiB catalogue
SHA-256 is
`8c8f81118ce128abf512eb10525814fd88d6724a393c10250339b2ca43a9b7b5`.
The next physical allocation is `0x632000`, leaving 10,215,424 bytes (146 aligned
64 KiB snapshot slots) before the permanent E10 guard.

The per-commit record is
`artifacts/stage4k8-factory/install-progress-20261030.json`; the parsed final catalogue
audit is
`artifacts/stage4k8-factory/post-install-catalogue-audit-20261030.json`. The factory
manifest now marks the entire validated queue committed. Fifteen candidates remain
quarantined because gameplay, controls, memory requirements or post-load media
independence did not pass the established gate.
