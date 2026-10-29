# CPC 464 ROM provenance

The local validation input is the unmodified English CPC 464 32 KiB image,
split by the build into the lower OS ROM and upper BASIC ROM without changing a
byte.

| Field | Value |
| --- | --- |
| Local filename | `roms/cpc464.rom` (Git-ignored) |
| Size | 32,768 bytes |
| SHA-256 | `00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e` |
| SHA-1 | `56d39c463da60968d93e58b4ba0e675829412a20` |
| MD5 | `a993f85b88ac4350cf4d41554e87fe4f` |
| Retrieval source | `https://github.com/Abdess/retrobios/blob/main/bios/Amstrad/CPC/cpc464.rom` |
| Retrieved | 2026-10-29 |

The SHA-1 and MD5 match established CPC BIOS catalogues. The image visibly
retains its original Amstrad and Locomotive Software copyright messages.

Amstrad's published emulator policy permits redistribution of its copyrighted
CPC ROM material when copyright messages remain unchanged and Amstrad's
retained copyright is acknowledged. The historical statement also notes that
some CPC code is owned by Locomotive Software and that its permission must be
checked separately. This repository therefore does **not** commit the ROM or
claim that a ROM-containing UF2 may be redistributed. The generated UF2 is a
local test candidate only until the remaining redistribution position is
confirmed.

Acknowledgement requested by the Amstrad statement:

> Amstrad has kindly given permission for its copyrighted material to be
> redistributed, but Amstrad retains its copyright.

References:

- Amstrad CPC emulator/ROM policy archive:
  `https://www.freetimeweb.nl/home/computer/alt/lawsons-amstrad-computer_site/web.ukonline.co.uk/cliff.lawson/cpchomec.htm`
- Preserved 1999 Amstrad ROM permission message:
  `https://github.com/maziac/DeZog/blob/main/documentation/amstrad-rom-permissions.txt`

