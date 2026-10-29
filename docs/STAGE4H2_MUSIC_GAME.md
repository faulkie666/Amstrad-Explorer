# Stage 4H.2 — verified music-producing CPC game

## Result

**PASS (host only).** Knight Tyme produces sustained, structured three-channel
AY music in the existing fast CPC464 emulator. A clean 20-second interval was
captured after the deterministic menu sequence finished and after all host
input stopped. Manic Miner independently produces sustained one-channel music.
Chuckie Egg and the existing Dizzy state do not provide a suitable continuous
music test in the exercised states.

No COM port, BOOTSEL volume, USB game storage, catalog, device flash, or
installed game was accessed. No firmware was built or deployed, and neither AY
synthesis nor production firmware code was changed.

## Method

`cpc_game_audio_probe` is a host-only executable linked to the same CPC core as
the existing regressions. It loads an existing 65,792-byte SNA, records PCM
through the normal AY callback, and records PPI/AY bus operations through the
host-only Stage 4H.1A trace. It uses documented deterministic inputs and writes
mono PCM16 WAV plus CSV evidence. It is not included in RP2350 firmware.

Duplicate PPI strobes are collapsed before analysis. Register 14 keyboard
scans are excluded from sound activity. Tone frequencies use the CPC AY clock:
`1,000,000 / (16 * period)`. Audio activity is measured in 100 ms PCM windows;
the conservative active threshold is RMS 100 on the signed-16-bit scale.

## Exact snapshots

| Game | Snapshot | SNA identity | SHA-256 | Provenance |
|---|---|---|---|---|
| Knight Tyme | `snapshots/knight-tyme.sna` | v1, 64 KiB; no v1 machine byte | `ca5fd1c8fe5b72694038fa6263349958d0537fe04c5605e49e489cba13f65156` | deterministic CPC464 BASIC/BIN loader pipeline |
| Manic Miner | `artifacts/stage4c1a-manic-miner/manic-miner-native.sna` | v1, 64 KiB; no v1 machine byte | `6661c74f919e228b2d21e1c0fceba7369c80281c1e27c53d03d624b1ff6d743d` | deterministic CPC464 MANIC/MANIC.BIN pipeline |
| Chuckie Egg | `artifacts/stage4g2-chuckie/chuckie-egg.sna` | v1, 64 KiB; no v1 machine byte | `2ce4e8f2eb14eada2e02bfc708a54872e291bdb47d85dd952a49099b3fdae08e` | deterministic CPC464 CHUCKIE/CHUCK.BIN pipeline |
| Dizzy | `artifacts/stage4g2-controls/dizzy/dizzy.sna` | v3, 64 KiB, explicit machine 0 (CPC464) | `2086041545be0bbdaa4e87a60c86d0e69e7d41c27242621d78416ceed34da943` | lossless decompression of local `dizzy.sna.gz` |

For the v1 files, the CPC464 claim comes from the proven native construction
pipeline, not a nonexistent v1 machine field.

## Sixty-second activity results

| Game and controlled state | Changed sound registers | Tone changes A/B/C | Distinct periods A/B/C | Volume changes A/B/C | Envelope-register changes | PCM result | Classification |
|---|---:|---:|---:|---:|---:|---|---|
| Knight Tyme; deterministic menu sequence, then no input after 21.1 s | 1,170 | 494 / 547 / 117 | 24 / 34 / 17 | 1 / 1 / 1 | 4 | RMS 3,370.70; peak 13,723; active 21.5..60.0 s | sustained three-channel music |
| Manic Miner; no input | 3,222 | 329 / 0 / 0 | 9 / 0 / 0 | 2,343 / 0 / 0 | 0 | RMS 2,116.76; peak 5,269; active all 60 s | sustained one-channel in-game music |
| Chuckie Egg; S then 1, no gameplay input | 20 | 9 / 7 / 0 | 8 / 6 / 0 | 1 / 1 / 0 | 0 | RMS 491.71; 98.33% exact silence; one 1.0 s burst | isolated effect, not sustained music |
| Dizzy; title, gameplay, pause/resume, title return and restart | 14 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 11 | 0 | RMS 25.67; 99.78% exact silence; one 0.1 s burst | initial fade only, not music |

The Knight Tyme run deliberately stops input after the final menu action. AY
music continues without further keys for the remaining 38.5 seconds. At the
start of the evidence interval, mixer `0xb8` enables the tone generator on all
three channels and disables noise; fixed volumes are A=12, B=13 and C=15.
The 20-second interval contains 250, 265 and 42 stable tone updates after
near-simultaneous register-byte writes are grouped. It has 10, 30 and 8 stable
distinct tone frequencies across A/B/C. The changing harmonic note sequences,
three simultaneously enabled tone channels, and uninterrupted PCM distinguish
it from a repeated isolated effect.

Manic Miner is a second positive result: its no-input state remains active for
all 60 seconds, with a repeating nine-period channel-A melody and volume
sequencing. Knight Tyme is preferred for physical acceptance because its three
channels and wider note set exercise more of the AY path.

## Dizzy identity and state investigation

The existing asset is **Dizzy: The Ultimate Cartoon Adventure (Dizzy 1)**, not
Treasure Island Dizzy. Local RAM contains the ending text naming Katmandu and
the evil wizard Zaks, plus `DIZZYFL2` loader markers. Those story identifiers
match the original game's description in the
[Codemasters Archive](https://thecodemastersarchive.co.uk/games/dizzy-tuca/);
Treasure Island is a separate sequel.

The snapshot's title bitmap carries unusual `DIZZY LIVES HOLIDAY` branding, so
the exact crack/compilation dump variant cannot be named more narrowly from the
available metadata. Its definitive local identity is the SNA hash above and
the source GZip hash
`0e655d1bd1ba743356e8512cbf244a75feadc2697483494babf83fdc54618a8f`.
This correction does not alter the approved package.

The expanded 60-second state trial spends ten seconds on the title, enters
gameplay, exercises movement/jump, pauses and resumes, returns to the title,
and restarts. It produces exactly the same 14 changing sound-register writes:
the initial channel-C 880 Hz fade, then silence. No alternate tested state
executes sustained music. This is a result for this exact captured state, not a
claim about every release of Dizzy.

## Audio evidence

Preferred acceptance WAV:

- `artifacts/stage4h2-music-probes/knight-tyme-music-20s.wav`
- SHA-256: `cc8c04864da6f63135663869e55cb6cee51d39565272227dd9d1410f6fa9ce8d`
- RIFF/WAVE, little-endian PCM signed 16-bit, mono, 22,050 Hz;
- exactly 441,000 samples / 20.000 seconds;
- minimum -7,736, maximum 7,631;
- RMS 4,223.39 (12.89% full scale);
- peak 7,736 (23.61% full scale);
- 0.0066% exact-zero samples;
- all 200 100-ms windows active;
- zero clipped samples.

The file is a lossless extraction of seconds 22..42 from the deterministic
60-second emulator capture. It contains only genuine AY PCM generated from the
Knight Tyme game state—no synthetic or prerecorded content.

Full controlled evidence is retained in
`artifacts/stage4h2-music-probes/knight-once-60s.wav` (SHA-256
`241dc67ca03932278ebc707c6a60829f4b662263e25179d30715cb708a0d2c3f`)
and `knight-once-60s.csv` (SHA-256
`a67db09cf2f81f9fe2cfb9e021521eb06fd539e9b19e7dc655a9afe0bc6dfabc`).

## Regression and preservation

The complete host suite passes 16/16, including CPC464 BASIC, Knight Tyme,
Manic Miner, controls, game library, upload/removal recovery, UF2/E10 policy,
AY synthesis, WAV serialization, audio buffering, Dizzy tracing and fast-engine
integration.

Preservation hashes remain valid:

- physical Stage 4G.2.1: `37edb2731a5580d8d76366bdb7fb2ba1dd43aadf7659e292b3ab48b0c478cd9e`;
- Stage 4H.1: `38b93a3e96c840f819684748c9496c5da0e14f23b4eb3a063acd974de71447ae`;
- Stage 4H.1A: `f04d62d6790f93f2b9ef470e1b7dc50ac64745f5700a761e13088a05cfbe0485`;
- Manic Miner, Chuckie Egg and Dizzy packages all retain their recorded hashes.

## Recommendation

Use Knight Tyme for the first separately approved physical AY test. The test
should deploy a subsequently approved audio firmware once, preserve the
catalog, launch the existing built-in Knight Tyme entry, reproduce the same
menu sequence, and listen for the sustained three-channel passage. Capture
CPCPERF at the same time because physical audio cost is still unmeasured.

No firmware deployment or game upload is authorized by this report.
