// Shared scaffolding for the serial-receiver harnesses: a fake serial port that captures the
// driver's RX callback, and a fuzz-driven clock.
//
// Stream layout: 4 mask bytes (little endian), then the byte stream. Bit (i % 32) of the mask puts an
// inter-frame gap before stream byte i, so frame-timeout resynchronisation is reachable. The status
// function is polled before each gap and at the end, as the scheduler would between frames.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#include "drivers/serial.h"
#include "drivers/time.h"
#include "io/serial.h"
#include "pg/rx.h"
#include "rx/rx.h"
#include "telemetry/telemetry.h"

#define FUZZ_CLOCK_START_US 1000000u

static uint32_t fuzzClockUs = FUZZ_CLOCK_START_US;
static serialReceiveCallbackPtr fuzzRxCallback;
static void *fuzzRxCallbackData;
static serialPort_t fuzzSerialPort;

uint32_t micros(void) { return fuzzClockUs; }
uint32_t microsISR(void) { return fuzzClockUs; }

serialPort_t *openSerialPort(serialPortIdentifier_e id, serialPortFunction_e fn,
    serialReceiveCallbackPtr callback, void *callbackData, uint32_t baud, portMode_e mode, portOptions_e options)
{
    (void)id; (void)fn; (void)baud; (void)mode; (void)options;
    fuzzRxCallback = callback;
    fuzzRxCallbackData = callbackData;
    return &fuzzSerialPort;
}

bool telemetryCheckRxPortShared(serialPortIdentifier_e id, const SerialRXType type) { (void)id; (void)type; return false; }
serialPort_t *telemetrySharedPort = NULL;

#define FUZZ_BYTE_TIME_US 20u
#define FUZZ_GAP_US 20000u
// Every input starts after an idle jump longer than each receiver's frame timeout, which clears their
// function-local statics. The clock is never rewound: the signed gap checks would not fire.
#define FUZZ_IDLE_US 100000u

static void pollReceiver(rxRuntimeState_t *state)
{
    volatile uint8_t status = state->rcFrameStatusFn(state);
    (void)status;
    volatile float sink = 0;
    for (uint8_t ch = 0; ch < state->channelCount; ch++) {
        sink = state->rcReadRawFn(state, ch);
    }
    (void)sink;
}

static void feedSerialRx(const uint8_t *data, size_t size, rxRuntimeState_t *state)
{
    if (size < 4 || !fuzzRxCallback) {
        return;
    }
    const uint32_t gapMask = data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
    data += 4;
    size -= 4;

    fuzzClockUs += FUZZ_IDLE_US;
    for (size_t i = 0; i < size; i++) {
        if (i < 32 && ((gapMask >> i) & 1)) {
            pollReceiver(state);
            fuzzClockUs += FUZZ_GAP_US;
        } else {
            fuzzClockUs += FUZZ_BYTE_TIME_US;
        }
        fuzzRxCallback(data[i], fuzzRxCallbackData);
    }
    pollReceiver(state);
}

static rxConfig_t fuzzRxConfig(void)
{
    rxConfig_t config;
    memset(&config, 0, sizeof(config));
    config.rx_uart = SERIAL_PORT_USART1;
    return config;
}
