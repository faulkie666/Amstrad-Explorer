# Stage 4B.1 safe game removal

Stage 4B.1 adds explicit removal of an installed CPCLIB1 game by ID. It does
not erase snapshot payloads or reclaim their space. BASIC and Knight Tyme are
built-in entries and cannot be removed.

## Command

Removal requires both the host confirmation option and an on-wire confirmation
token:

```powershell
py tools/cpc_game.py remove GAME_ID --port COM11 --confirm REMOVE
```

The device accepts `REMOVE` only from the launcher command loop, after a recent
`HELLO`, while no upload transaction is active. The payload contains only the
validated game ID; the host cannot supply a flash offset. `ABORT` remains
reserved for unfinished uploads.

## Durable removal transaction

A one-slot catalog update would allow the older catalog to resurrect a removed
game if it later became the fallback. Stage 4B.1 therefore uses two durable
catalog publications:

1. Validate the active catalog and locate the exact game ID.
2. Publish the filtered catalog to the inactive slot with generation + 1 and a
   persistent `removal pending` marker.
3. Verify that committed catalog and confirm the ID is absent.
4. Rewrite the former active slot with the same filtered descriptors,
   generation + 2, and the pending marker cleared.
5. Verify both catalog slots independently and acknowledge success only after
   both exclude the removed ID.

If power fails before step 2 commits, the original catalog remains active and
the removal has not occurred. If power fails after step 2 commits, the pending
catalog is authoritative and boot-time recovery mirrors it to the other slot
before uploads are enabled. The pending catalog itself already excludes the
ID, so the recovery fallback cannot resurrect it.

The reserved CPCLIB1 header bytes hold a backward-compatible transaction flag,
pending ID, and 4 KiB-aligned allocation high-water mark. Header CRC covers all
three. Existing CPCLIB1 readers ignore these reserved fields.

## Space treatment

Removal changes catalogs only. The payload is deliberately left programmed.
The persistent high-water mark and a scan of every valid catalog prevent later
installs from reusing either removed storage or data referenced by a fallback
catalog. Removed payload capacity is **not reclaimed or reported as free**.
Compaction or garbage collection requires a separate future design.

## Limitations

- There is no space reclamation.
- Removal is by immutable game ID, not title or menu index.
- A Stage 4B.1 physical deployment and disposable upload/remove/reboot test
  still require separate explicit approval.
- The candidate does not modify flash merely by being built or inspected.
