# uvarint: shift exponent 35 on a 6-byte varint

- **Reproduce:** `/mayhem/fuzz_uvarint-standalone input` (UBSan: `common/uvarint.c:60: shift exponent 35 is too large for 32-bit type`).
- **Cause:** `uvarintRead` shifts each 7-bit group by `7 * n` into a `uint32_t` without capping the number of continuation bytes, so a sixth byte shifts by 35.
- **Impact:** undefined behavior on attacker-controlled blackbox/MSP data; the decoded value is also wrong.
- **Fix:** stop (or fail) after 5 bytes, i.e. `if (shift >= 32) return error`.
