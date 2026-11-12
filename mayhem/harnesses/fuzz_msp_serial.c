// MSP serial framing: MSP v1/v2 header and checksum state machine, jumbo frames, reply
// serialisation, and the '#'/reboot-character side channel, driven through mspSerialProcess().
//
// Input: byte 0 = reboot character, byte 1 = chunk size seed, the rest is the serial byte stream.
// The command handler echoes the request payload back as the reply.
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "platform.h"

#include "common/streambuf.h"
#include "drivers/serial.h"
#include "drivers/system.h"
#include "io/serial.h"
#include "io/serial_feature_map.h"
#include "cli/cli.h"
#include "msp/msp.h"
#include "msp/msp_serial.h"
#include "pg/msp.h"

static uint32_t fakeMillis = 1000;
uint32_t millis(void) { return fakeMillis; }

#define FAKE_RX_CAP 8192
#define FAKE_PORT_ID ((serialPortIdentifier_e)0x12)
static uint8_t fakeRxBuf[FAKE_RX_CAP];
static unsigned fakeRxHead, fakeRxTail;

uint32_t serialRxBytesWaiting(const serialPort_t *p) { (void)p; return fakeRxTail - fakeRxHead; }
uint8_t serialRead(serialPort_t *p) { (void)p; return fakeRxBuf[fakeRxHead++ % FAKE_RX_CAP]; }

void systemResetToBootloader(bootloaderRequestType_e requestType) { (void)requestType; }
void cliEnter(serialPort_t *p, bool interactive) { (void)p; (void)interactive; }
bool cliProcess(void) { return false; }

static serialPort_t fakePort;
static unsigned openedCount;
serialPort_t *openSerialPort(serialPortIdentifier_e identifier, serialPortFunction_e fn,
    serialReceiveCallbackPtr cb, void *cbData, uint32_t baud, portMode_e mode, portOptions_e options)
{
    (void)fn; (void)cb; (void)cbData; (void)baud; (void)mode; (void)options;
    if (openedCount++ > 0) {
        return NULL;
    }
    fakePort.identifier = identifier;
    return &fakePort;
}
bool isSerialPortShared(serialPortIdentifier_e id, uint16_t mask, serialPortFunction_e fn) { (void)id; (void)mask; (void)fn; return false; }
serialType_e serialType(serialPortIdentifier_e id) { (void)id; return SERIALTYPE_UART; }
uint8_t serialDefaultPortBaud(serialBaudClass_e c) { (void)c; return BAUD_115200; }
unsigned serialImpliedMspPorts(serialPortIdentifier_e *ports, unsigned maxPorts) { (void)ports; (void)maxPorts; return 0; }
uint32_t serialSynthesizeFunctionMask(serialPortIdentifier_e id) { (void)id; return FUNCTION_MSP; }

uint32_t serialTxBytesFree(const serialPort_t *p) { (void)p; return FAKE_RX_CAP; }
void serialWriteBufNoFlush(serialPort_t *p, const uint8_t *data, int len)
{
    (void)p;
    volatile uint8_t sink = 0;
    for (int i = 0; i < len; i++) {
        sink ^= data[i];
    }
    (void)sink;
}
bool isSerialTransmitBufferEmpty(const serialPort_t *p) { (void)p; return true; }
void serialBeginWrite(serialPort_t *p) { (void)p; }
void serialEndWrite(serialPort_t *p) { (void)p; }
void waitForSerialPortToFinishTransmitting(serialPort_t *p) { (void)p; }
void closeSerialPort(serialPort_t *p) { (void)p; }
mspDescriptor_t mspDescriptorAlloc(void) { return 0; }
serialPortIdentifier_e displayPortMspGetSerial(void) { return (serialPortIdentifier_e)-1; }

serialConfig_t serialConfig_System;
mspConfig_t mspConfig_System;
const uint32_t baudRates[BAUD_COUNT] = { 0 };

static mspResult_e echoCmd(mspDescriptor_t desc, mspPacket_t *cmd, mspPacket_t *reply, mspPostProcessFnPtr *post)
{
    (void)desc; (void)post;
    reply->cmd = cmd->cmd;
    const int n = sbufBytesRemaining(&cmd->buf);
    sbufWriteData(&reply->buf, sbufConstPtr(&cmd->buf), n < 256 ? n : 256);
    return MSP_RESULT_ACK;
}
static void ignoreReply(mspPacket_t *p) { (void)p; }

#include "rx/rx.h"
#include "msp/msp_serial.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2) {
        return 0;
    }
    fakeRxHead = fakeRxTail = 0;
    fakeMillis = 1000;
    openedCount = 0;
    memset(&fakePort, 0, sizeof(fakePort));
    memset(&serialConfig_System, 0, sizeof(serialConfig_System));
    memset(&mspConfig_System, 0, sizeof(mspConfig_System));
    for (unsigned slot = 0; slot < MAX_MSP_PORT_COUNT; slot++) {
        mspConfig_System.msp_uart[slot] = SERIAL_PORT_NONE;
    }
    mspConfig_System.msp_uart[0] = FAKE_PORT_ID;
    serialConfig_System.reboot_character = data[0];
    mspSerialInit();

    const size_t chunk = (size_t)data[1] % 200 + 1;
    data += 2;
    size -= 2;
    while (size > 0) {
        const size_t n = size < chunk ? size : chunk;
        for (size_t i = 0; i < n; i++) {
            fakeRxBuf[fakeRxTail++ % FAKE_RX_CAP] = data[i];
        }
        data += n;
        size -= n;
        fakeMillis += 5;
        mspSerialProcess(MSP_EVALUATE_NON_MSP_DATA, echoCmd, ignoreReply);
    }
    fakeMillis += 1000;
    mspSerialProcess(MSP_EVALUATE_NON_MSP_DATA, echoCmd, ignoreReply);
    return 0;
}
