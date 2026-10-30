# Stage 4K.2 — Rapid game library expansion

Date: 2026-10-30  
Mode: host-only conversion, gameplay evidence, packaging, and installation-path assessment

## Result

**PASS (host preparation), HOLD (physical installation).** Three additional games now
meet the CPC464/64 KiB, diskless reload, gameplay, graphics, control, package-integrity,
and source-provenance gates:

1. Block Invasion
2. Kiloroid
3. Ktris

No COM port, USB device, BOOTSEL volume, catalogue, or physical flash was accessed.
The active Stage 4I.7 pack, the Stage 4J.3 firmware artifact, rollback data, source
collection, and current six-game physical library were not changed.

## Active-library and installation findings

The active firmware mounts the Stage 4I.7 `CPXPCK2` image as the authoritative game
view. `launcher::entry_count()`, category/letter enumeration, and launch selection use
the packed view whenever it is mounted. The legacy CPCLIB1 catalogue then remains valid
but is not enumerated by the launcher.

The ordinary Stage 4B upload path is not disabled in migration state `active`, and the
transactional allocator has correctly reserved the packed area through `0x466FFF`.
It can therefore allocate a new CPCLIB1 payload at or above `0x467000`. However, the
current launcher would not show that newly uploaded entry. Uploading now would consume
flash without producing a usable menu entry and is not approved.

The migration transport is also not a generic pack updater. It accepts only the exact
Stage 4I.7 manifest: offset `0x43B000`, 179,360 bytes, CRC32 `ec7fd1e9`, and the fixed
SHA-256. The Stage 4K.1 proof pack and the Stage 4K.2 proof packs are consequently not
physical-installation artifacts.

### Fastest safe incremental method

The smallest compatible firmware change is a packed-plus-legacy launcher overlay:

- keep the active CPXPCK2 pack authoritative and byte-identical;
- enumerate additional valid CPCLIB1 entries after packed entries;
- filter the existing legacy Manic Miner and Dizzy descriptors because those IDs are
  already represented by CPXPCK2;
- reject an upload whose ID collides with any packed or legacy ID;
- retain the existing transactional CPCLIB1 upload/removal and allocator;
- allocate new payloads only beyond the reserved packed high-water mark;
- merge the category/letter view without loading the whole catalogue into SRAM.

This needs a firmware update, but it does not need a new flash format, a CPXPCK2 rewrite,
or another migration of the current six entries. After that firmware is independently
verified and physically accepted, the existing `.cpcgame` uploader can install additions
one at a time with its current commit/readback guarantees.

A whole-pack replacement is possible but is not the fastest or lowest-risk option. The
prospective nine-record image is 197,552 bytes and ends at `0x46B3AF`, beyond the current
reserved end `0x466FFF`; it would require a new reservation, manifest, recovery proof,
and whole-pack transaction.

## Priority-candidate results

| Game | Source and identity | Host result | Controls | AY result | Decision |
|---|---|---|---|---|---|
| Chuckie Egg | Existing native SNA, SHA-256 `2ce4e8f2eb14eada2e02bfc708a54872e291bdb47d85dd952a49099b3fdae08e` | Existing proof reaches level-one gameplay and reloads disklessly | Up/down/left/right/jump; title also needs S and player count | Isolated effect, not music | **HOLD for install:** gameplay is proven, but the current package/profile cannot perform the complete title/start/redefinition sequence on the physical controls |
| Frogger | `frogger.dsk.gz`, 18,899 bytes, SHA-256 `4007a7b355bb7e32880c35583b9a09a26cc8219bb3bf0a3d0dba07901c2e1072` | Supplied metadata explicitly states “Only works on Plus machines”; CPC464 run returns to BASIC | Not applicable | Not measured | **UNSUPPORTED** on the CPC464 target |
| Ktris | `ktris.dsk.gz`, 1,433 bytes, SHA-256 `02789d6a388ac52a46cecac14456a710c329f02fe3a86f3a467b929ab09fc051` | Actual falling-block gameplay; standalone SNA reload without disk; deterministic input deltas | Cursor left/right/down and Space | 0 volume-active frames in 1,000-frame trace | **READY after launcher-overlay firmware** |
| Block Invasion | `blockinv.dsk.gz`, 1,411 bytes, SHA-256 `5e0084f725a81567981758e6848f2863a1ab4facbba7ed30d2848e6dfa632546` | Actual arcade playfield; standalone SNA reload without disk; Z/X move the player/paddle state | Z, X; Space retained in the generic profile | 0 volume-active frames in 1,033-frame trace | **READY after launcher-overlay firmware** |
| Kiloroid | `kiloroid.dsk.gz`, 1,466 bytes, SHA-256 `a0033d5ad79ca0d55cfb714d46389e17c8836a2512dc6cdb4723f6edfab7e0d0` | Actual asteroid playfield; standalone SNA reload without disk; cursor-state responses observed | Cursor directions; Space available | 0 volume-active frames in 1,000-frame trace | **READY after launcher-overlay firmware** |
| Harrier Attack | `HARRIER` SHA-256 `3ffeeb3cc8d203bd67d1113823ddee75fc36fbbf2f6f6edeb181e640ac89b5e8`; `HARRIERC.BIN` SHA-256 `0feaf6c9638e94991a96d1dd78ff6f2308d53b90f670e3f37db8ae976e1d2b6e` | Authentic loader reaches the skill/control screen; skill 1 reaches the carrier/game scene and reloads disklessly | Complete take-off and flight control response not yet proven | No active AY in captured state | **HOLD:** gameplay/control acceptance is incomplete |

## Ready packages

| Game | SNA SHA-256 | Package SHA-256 | Package CRC32 | Control profile |
|---|---|---|---|---|
| Block Invasion | `4a3a08472f70c8fe95af12f03578acde32254de95c3c335e6c8efe62ab9ef86e` | `4b35e235235edf835c2b99d8188027e9b69d0e7cb8e23f1b84d6f6890270b137` | `b35e7cfc` | `zx-space` |
| Kiloroid | `75bebbbd8ed9ea80a85bf00e013164913ec077ff0d2613bc01f1e427d05754b4` | `7c0e813f8c61414a3546f1421b1ec60a805362f4ac36a204959d4cb5f6ceec8d` | `4a1969be` | `cursor-qaop` |
| Ktris | `ef23111f3974e65354674d10eb8af0e11e79f8b946f88c64deb25a0f58514612` | `81cc556496609189af0a2ee9058a903cf5dfee8f91c7a73ea61ceeff6b1377db` | `7b3a00a5` | `cursor-qaop` |

All packages are 65,888 bytes, report CPC464 / 64 KiB, use SNA v3 with machine
identity 0, and pass `tools/cpc_game.py inspect`.

Paths:

- `artifacts/stage4k2-rapid-library/packages/block_invasion.cpcgame`
- `artifacts/stage4k2-rapid-library/packages/kiloroid.cpcgame`
- `artifacts/stage4k2-rapid-library/packages/ktris.cpcgame`

The three-record CPXPCK2 host proof is 34,576 bytes, has two valid headers, valid record
and payload CRCs, decompresses all three 65,792-byte snapshots, and has SHA-256
`e2d9794dc3860b812252037a9249b5dbd8f491698ed1e92450b04f330936b3cf`.
Its compressed payloads are 4,795, 8,695, and 4,687 bytes (18,177 bytes total).

The prospective current-plus-three proof contains nine records (eight launchable plus
hidden Fruity Frank), is 197,552 bytes, and has SHA-256
`b209b96f0163bd8ac3a90acce1e8e7460ae150a6bea2e9e3de4a21e2181d7ea4`.
It is capacity evidence only and must not be sent to the current migration command.

## Evidence and focused verification

- konCePCja v6.4.2 ran as CPC model 0 with 64 KiB and AMSDOS.
- Each accepted state produced game framebuffer evidence rather than a BASIC prompt.
- Each accepted SNA was loaded into a new emulator instance with no disk attached.
- Deterministic held-key/joystick trials recorded framebuffer and RAM hashes.
- AY traces were captured for 1,000 or more frames and independently inspected.
- `.cpcgame` import and inspect passed for all three additions.
- Both CPXPCK2 proof images passed header, record CRC, payload CRC, LZ4 decompression,
  and per-record raw-size validation.
- Independent repeat builds of all three `.cpcgame` files and the first-additions
  CPXPCK2 image were byte-identical (four of four deterministic comparisons passed).
- Preserved Stage 4I.7 pack SHA-256 remains
  `27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe`.
- Preserved Stage 4J.3 UF2 SHA-256 remains
  `fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53`.

## Exact next physical acceptance

1. Implement and host-test only the packed-plus-legacy launcher overlay and device-side
   duplicate-ID guard; do not change Z80, audio, display, flash layout, or CPXPCK2.
2. Build a separate firmware candidate and prove that the six current visible entries
   remain unchanged while synthetic CPCLIB1 additions appear only after commit.
3. Under separate approval, flash that firmware and confirm migration remains active,
   the existing six entries launch, and the packed region is byte-identical.
4. Upload **Ktris only** with the existing transactional uploader. Verify status/list,
   reboot persistence, D-pad left/right/down, and Space rotation on the Qw/ST Pad.
5. After Ktris physical PASS, upload Block Invasion and Kiloroid one at a time, verifying
   catalogue integrity and the previous entries after each commit.

No physical operation is authorized by this report.
