# PWEDIT1 — selective Pokéwalker edit format, version 1

All header and record integers are **big endian**. Payload bytes retain their
EEPROM byte order. CRC32 is IEEE / reflected polynomial `0xEDB88320`, initial
`0xFFFFFFFF`, final XOR `0xFFFFFFFF`. This is an integrity check, not authentication.

| Header offset | Length | Meaning |
| --- | --- | --- |
| 0 | 8 | ASCII `PWEDIT1` followed by NUL |
| 8 | 2 | Version, exactly 1 |
| 10 | 2 | Number of records, 1–19 |
| 12 | 4 | Total file length, at most 1024 |
| 16 | 4 | CRC32 of the entire file, treating these four bytes as zero |
| 20 | 40 | Original UniqueIdentityData, EEPROM `0x0083` |
| 60 | 4 | Trainer TID/SID bytes, EEPROM `0x00F9` |
| 64 | 4 | Walk's lastSyncTime, EEPROM `0x014D` |
| 68 | 16 | Walking Pokémon summary, EEPROM `0x8F00` |
| 84 | 4 | Reserved, must be zero |

Each record is `address:u16, length:u16, expectedOldBytes[length], newBytes[length]`.
Records must be sorted, non-overlapping and actually change their field. Only
these **exact** address/length pairs are accepted:

- `0x0156 / 4`: lifetime steps, BE, 0–9 999 999.
- `0x0162 / 2`: cumulative days, BE, 0–65 535.
- `0x0164 / 2`: watts, BE, 0–9 999.
- `0xCE8C, 0xCE9C, 0xCEAC / 16`: captured Pokémon summaries.
- `0xCEBC, 0xCEC0, 0xCEC4 / 2`: dowsed item IDs, LE.
- `0xCEC8 + 4*i / 2`, `i=0..9`: peer gift item IDs, LE.

A Pokémon summary contains:
`species:u16, heldItem:u16, moves[4]:u16, level:u8, formAndGender:u8,
flags:u8, preservedPadding:u8` (16 bytes). Species 1–493, held item 0–536 except
113–134, moves 0–467, level 1–100. Form uses bits 0–4, female bit 5, other bits
zero; flags use bit 0 (form) and bit 1 (shiny), other bits zero. An empty summary
is sixteen zero bytes. These are structural constraints, not Pokémon legality rules.

The importer validates the original EEPROM size/signature and both copies of
UniqueIdentityData (40 bytes), IdentityData (104) and HealthData (24). Each reliable
checksum is **1 + sum(data), modulo 256**. Both copies must be valid and equal.
This seed was checked against the firmware; a plain zero-seeded byte sum is wrong.

Before writing, header identity/walk data and all expected field bytes must match
the connected device. Every other byte is outside the patch's authority. The
editor's raw EEPROM export recalculates HealthData checksum `0x016E` and mirrors
the complete 25 bytes to `0x0256`. The physical importer updates selected RAM cache
fields and asks the firmware to persist HealthData instead, preserving live cache
values that are not edited. Captures and item IDs are written/read back directly.

The file cannot request arbitrary EEPROM/RAM addresses, machine code execution,
identity reassignment or calibration changes. Its allowed activity operations use
the fixed upstream firmware routine documented in the import guide.
