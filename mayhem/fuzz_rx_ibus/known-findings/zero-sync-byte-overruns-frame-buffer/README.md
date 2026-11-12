# ibus: a leading 0x00 byte overruns `ibus[32]`

- **Reproduce:** `/mayhem/fuzz_rx_ibus-standalone input` (UBSan: `rx/ibus.c:123: index 32 out of bounds for type 'uint8_t[32]'`).
- **Cause:** after `ibusInit`, `ibusSyncByte` and `ibusFrameSize` are 0. A first received byte of 0x00 satisfies the "frame start" test (`ibusFramePosition == 0`, byte == `ibusSyncByte`), and because `ibusFrameSize` is 0 the "frame complete" test (`ibusFramePosition == ibusFrameSize`) is never true when the position is already past it, so `ibusFramePosition` keeps growing and writes past the 32-byte `ibus[]`.
- **Impact:** out-of-bounds write from the serial RX line (a wire-level attacker or line noise) on the flight controller.
- **Fix:** reject frame start while `ibusFrameSize == 0`, and bound `ibusFramePosition` by `sizeof(ibus)` before storing.
