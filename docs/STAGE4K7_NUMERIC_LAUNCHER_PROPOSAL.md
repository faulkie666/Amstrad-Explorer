# Stage 4K.7 numeric launcher proposal

## Objective

Expose the already committed `4kroid` record without rewriting its payload, descriptor,
ID, title, or allocation.

## Minimal future firmware change

Add a `0-9` launcher category beside the existing A-Z categories. During cached merged
catalogue construction, route titles whose first ASCII character is `0` through `9` to
that bucket. Keep punctuation-leading titles unsupported unless a separate deliberate
policy is approved.

The change must reuse the current cached menu model. It must not rescan flash during
navigation, alter CPCLIB1 or CPXPCK2 formats, rewrite catalogues, or move payloads.
After reboot, the existing valid `4kroid` descriptor will naturally enumerate under
`0-9`.

## Required tests

- Existing A-Z ordering and navigation remain unchanged.
- `4kroid` appears once under `0-9` and launches from its current descriptor.
- Multiple digit-leading titles sort deterministically.
- Alphabetic duplicates remain deduplicated by game ID.
- Punctuation-leading titles remain rejected by the host uploader.
- Hidden CPXPCK2 records remain hidden.
- Menu-cache performance and the mandatory SRAM gate remain satisfied.

This is a host-only proposal. No firmware was built or deployed in Stage 4K.7 recovery.
