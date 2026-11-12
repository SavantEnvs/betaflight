# typeconversion: `i2a(INT_MIN)` negates the most negative `int`

- **Reproduce:** `/mayhem/fuzz_typeconversion-standalone input` (UBSan: `common/typeconversion.c:85: negation of -2147483648 cannot be represented in type 'int'`). The input is a zero base byte followed by the little-endian value `0x80000000`.
- **Cause:** `i2a` handles a negative number by doing `num = -num` on a signed `int`, which is undefined for `INT_MIN`. This is a different root cause from the `a2i` overflow in `a2i-signed-overflow/`.
- **Impact:** undefined behavior formatting `INT_MIN` (CLI/OSD/MSP number output); in practice the result stays `INT_MIN` and the digits come out wrong.
- **Fix:** negate in unsigned arithmetic (`unsigned u = 0u - (unsigned)num;`) and format `u`.
