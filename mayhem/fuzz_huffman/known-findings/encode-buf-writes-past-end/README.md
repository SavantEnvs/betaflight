# huffman: `huffmanEncodeBuf` writes one byte past an exactly-full output buffer

- **Reproduce:** `/mayhem/fuzz_huffman-standalone input` (ASan heap-buffer-overflow, `common/huffman.c:51`). The input asks for a 40-byte output buffer and compresses 54 bytes.
- **Cause:** after finishing a byte the encoder does `++outByte; *outByte = 0;` before checking `ret >= outBufLen`, so when the output fills the buffer exactly the zeroing write lands one byte beyond it. The overflow check also never fires on the last bit of the last symbol.
- **Impact:** one-byte heap/stack write past the caller's buffer, contents always 0.
- **Fix:** test `ret >= outBufLen` before advancing `outByte` (or require callers to allocate `outBufLen + 1`, and document it).
