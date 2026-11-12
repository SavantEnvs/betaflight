// Betaflight Huffman encoder (blackbox/OSD text compression): one-shot and streaming encoders into
// an exactly-sized heap output buffer, with the output length chosen by the input.
//
// Input: 2 bytes output buffer length (1..), then the bytes to compress.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "common/huffman.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) {
        return 0;
    }
    const int outLen = 1 + ((data[0] | data[1] << 8) % 512);
    data += 2;
    size -= 2;

    uint8_t *in = malloc(size ? size : 1);
    uint8_t *out = malloc((size_t)outLen);
    if (!in || !out) {
        free(in);
        free(out);
        return 0;
    }
    memcpy(in, data, size);

    huffmanEncodeBuf(out, outLen, in, (int)size, huffmanTable);

    memset(out, 0, (size_t)outLen);
    huffmanState_t state = { .outByte = out, .bytesWritten = 0, .outBufLen = (uint16_t)outLen, .outBit = 0x80 };
    *state.outByte = 0;
    huffmanEncodeBufStreaming(&state, in, (int)size, huffmanTable);

    free(in);
    free(out);
    return 0;
}
