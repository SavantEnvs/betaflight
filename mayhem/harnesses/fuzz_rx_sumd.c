// Graupner SUMD serial receiver: frame sync, channel count, CRC16, channel decode.
// The receiver is included so its file-scope state can be reset for every input.
#include "serial_rx_fuzz.h"

#include "rx/sumd.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    sumdFrameDone = false;
    memset(sumdChannels, 0, sizeof(sumdChannels));
    crc = 0;
    memset(sumd, 0, sizeof(sumd));
    sumdChannelCount = 0;
    lastFrameTimeUs = 0;

    rxConfig_t config = fuzzRxConfig();
    rxRuntimeState_t state;
    memset(&state, 0, sizeof(state));
    fuzzRxCallback = NULL;
    if (!sumdInit(&config, &state)) {
        return 0;
    }
    feedSerialRx(data, size, &state);
    return 0;
}
