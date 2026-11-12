// Firmware's own number <-> string conversions (CLI, OSD, MSP): decimal/float parsing of a
// NUL-terminated copy of the input and integer formatting into fixed buffers.
//
// Input: 1 byte base selector, 8 bytes of integer value, then text for the parsers.
// The long conversions take 32-bit values: the target's long is 32-bit, and uli2a's divisor is a 32-bit int.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "common/typeconversion.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 9) {
        return 0;
    }
    const int base = 2 + data[0] % 15;
    uint64_t raw = 0;
    memcpy(&raw, data + 1, 8);
    data += 9;
    size -= 9;

    char buf[72];
    ui2a((unsigned int)raw, (unsigned int)base, raw & 1, buf);
    uli2a((unsigned long)(uint32_t)raw, (unsigned int)base, raw & 2, buf);
    i2a((int)raw, buf);
    li2a((long)(int32_t)raw, buf);
    itoa((int)raw, buf, base);

    char *text = malloc(size + 1);
    if (!text) {
        return 0;
    }
    memcpy(text, data, size);
    text[size] = 0;

    volatile float f = fastA2F(text);
    (void)f;

    const char *src = text + (size ? 1 : 0);
    int num = 0;
    volatile char next = a2i(text[0], &src, base, &num);
    (void)next;

    free(text);
    return 0;
}
