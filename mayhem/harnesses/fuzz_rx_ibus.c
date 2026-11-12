// FlySky iBUS serial receiver on a port shared with iBUS telemetry: length-byte framing, IA6/IA6B
// checksums, channel decode, and the sensor discover/type/measurement responder.
// The receiver is included so its file-scope state can be reset for every input.
//
// Input: IBUS_SENSOR_COUNT sensor-type bytes, 8 bytes of flight state, then the serial_rx_fuzz.h stream.
#include "serial_rx_fuzz.h"

#include "common/utils.h"
#include "pg/pg.h"
#include "pg/pg_ids.h"
#include "io/gps.h"
#include "flight/imu.h"
#include "config/config.h"
#include "fc/rc_controls.h"
#include "fc/runtime_config.h"
#include "sensors/gyro.h"
#include "sensors/battery.h"
#include "sensors/barometer.h"
#include "sensors/acceleration.h"
#include "sensors/sensors.h"

uint8_t armingFlags = 0;
uint8_t stateFlags = 0;
uint16_t flightModeFlags = 0;
float rcCommand[4] = {0, 0, 0, 0};
telemetryConfig_t telemetryConfig_System;
static batteryProfile_t batteryProfile_System;
const batteryProfile_t *currentBatteryProfile = &batteryProfile_System;
attitudeEulerAngles_t attitude = EULER_INITIALIZE;
acc_t acc;
baro_t baro;
gpsSolutionData_t gpsSol;
uint16_t GPS_distanceToHome;

static uint32_t fuzzSensors;
static uint32_t fuzzVal;
static bool fuzzPortShared;

int16_t gyroGetTemperature(void) { return (int16_t)fuzzVal; }
int32_t getAmperage(void) { return (int32_t)(fuzzVal * 3); }
int16_t getEstimatedVario(void) { return (int16_t)(fuzzVal >> 3); }
uint8_t calculateBatteryPercentageRemaining(void) { return fuzzVal >> 5; }
uint16_t getBatteryAverageCellVoltage(void) { return fuzzVal >> 7; }
uint16_t getBatteryVoltage(void) { return fuzzVal >> 9; }
uint8_t getBatteryCellCount(void) { return 1 + (fuzzVal & 7); }
int32_t getMAhDrawn(void) { return (int32_t)(fuzzVal * 11); }
throttleStatus_e calculateThrottleStatus(void) { return (throttleStatus_e)(fuzzVal & 1); }
bool featureIsEnabled(uint32_t mask) { return (fuzzVal & mask) != 0; }
bool sensors(uint32_t mask) { return (fuzzSensors & mask) != 0; }
void serialWrite(serialPort_t *instance, uint8_t ch) { (void)instance; (void)ch; }
bool isSerialPortShared(serialPortIdentifier_e id, uint16_t mask, serialPortFunction_e fn)
{
    (void)id; (void)mask; (void)fn;
    return fuzzPortShared;
}

#include "rx/ibus.c"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const size_t sensorBytes = sizeof(telemetryConfig_System.flysky_sensors);
    const size_t prefix = sensorBytes + 8;
    if (size < prefix) {
        return 0;
    }

    ibusModel = 0;
    ibusSyncByte = 0;
    ibusFrameSize = 0;
    ibusChannelOffset = 0;
    rxBytesToIgnore = 0;
    ibusChecksum = 0;
    ibusFrameDone = false;
    memset(ibusChannelData, 0, sizeof(ibusChannelData));
    memset(ibus, 0, sizeof(ibus));
    lastFrameTimeUs = 0;
    initSharedIbusTelemetry(NULL);

    memcpy(telemetryConfig_System.flysky_sensors, data, sensorBytes);
    data += sensorBytes;
    fuzzPortShared = data[0] & 1;
    armingFlags = data[1];
    stateFlags = data[2];
    flightModeFlags = data[2] | data[3] << 8;
    fuzzSensors = data[3] * 0x01010101u;
    fuzzVal = data[4] | data[5] << 8 | data[6] << 16 | (uint32_t)data[7] << 24;
    gpsSol.llh.lat = (int32_t)fuzzVal;
    gpsSol.llh.lon = (int32_t)(0u - fuzzVal);
    gpsSol.numSat = data[4];
    attitude.values.yaw = (int16_t)fuzzVal;
    acc.accADC.x = (float)(int16_t)fuzzVal;
    data += 8;
    size -= prefix;

    rxConfig_t config = fuzzRxConfig();
    rxRuntimeState_t state;
    memset(&state, 0, sizeof(state));
    fuzzRxCallback = NULL;
    if (!ibusInit(&config, &state)) {
        return 0;
    }
    feedSerialRx(data, size, &state);
    return 0;
}
