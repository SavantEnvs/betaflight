// LEB128-style unsigned varint decoder and encoder: decode arbitrary bytes, and round-trip the
// decoded value through the encoder into an exactly-sized and a one-byte-short heap buffer.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "common/uvarint.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t *in = malloc(size ? size : 1);
    if (!in) {
        return 0;
    }
    memcpy(in, data, size);

    uint32_t val = 0;
    const int used = uvarintDecode(&val, in, size);
    if (used > 0) {
        uint8_t out[8];
        const int enc = uvarintEncode(val, out, sizeof(out));
        uint32_t back = 0;
        if (enc < 1 || uvarintDecode(&back, out, (size_t)enc) != enc || back != val) {
            abort();
        }
        // Encode into a heap buffer sized exactly to the encoded length.
        uint8_t *exact = malloc((size_t)enc);
        if (exact) {
            if (uvarintEncode(val, exact, (size_t)enc) != enc) {
                abort();
            }
            free(exact);
        }
        // One byte too small: the encoder must refuse (-1) without writing past the buffer.
        uint8_t *shortBuf = malloc(enc > 1 ? (size_t)enc - 1 : 1);
        if (shortBuf) {
            if (uvarintEncode(val, shortBuf, (size_t)enc - 1) != -1) {
                abort();
            }
            free(shortBuf);
        }
    }
    free(in);
    return 0;
}
