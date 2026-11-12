# typeconversion: `a2i` signed integer overflow

- **Reproduce:** `/mayhem/fuzz_typeconversion-standalone input` (UBSan: `common/typeconversion.c:111: signed integer overflow: 351803205 * 12`).
- **Cause:** `num = num * base + digit` is accumulated in an `int` with no overflow check, so a long digit string overflows.
- **Impact:** undefined behavior on user-supplied numeric strings (CLI/OSD text); the result wraps silently.
- **Fix:** accumulate in `unsigned`/`int64_t` and saturate, or stop at the digit limit.
