// JR XBus Mode B / Align RJ01 serial receiver: frame start, inner/outer CRCs, channel decode.
// The first input byte picks the provider. The receiver is included so its file-scope state can be
// reset for every input.
#include "serial_rx_fuzz.h"

#include "rx/xbus.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) {
        return 0;
    }
    xBusFrameReceived = false;
    xBusDataIncoming = false;
    xBusFramePosition = 0;
    xBusFrameLength = 0;
    xBusChannelCount = 0;
    xBusProvider = 0;
    memset((void *)xBusFrame, 0, sizeof(xBusFrame));
    memset(xBusChannelData, 0, sizeof(xBusChannelData));

    rxConfig_t config = fuzzRxConfig();
    rxRuntimeState_t state;
    memset(&state, 0, sizeof(state));
    state.serialrxProvider = (data[0] & 1) ? SERIALRX_XBUS_MODE_B_RJ01 : SERIALRX_XBUS_MODE_B;
    fuzzRxCallback = NULL;
    if (!xBusInit(&config, &state)) {
        return 0;
    }
    feedSerialRx(data + 1, size - 1, &state);
    return 0;
}
