/* Auto-generated stubs for RfalRfClass virtuals. Needed only so the
 * base class vtable is emitted; RfalRfST25R3916Class overrides all of them. */
#include "rfal_rf.h"

ReturnCode RfalRfClass::rfalInitialize() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalCalibrate() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalAdjustRegulators(uint16_t * a0) {
    (void)a0;
    return ST_ERR_NOT_IMPLEMENTED;
}

void RfalRfClass::rfalSetUpperLayerCallback(rfalUpperLayerCallback a0) {
    (void)a0;
}

void RfalRfClass::rfalSetPreTxRxCallback(rfalPreTxRxCallback a0) {
    (void)a0;
}

void RfalRfClass::rfalSetPostTxRxCallback(rfalPostTxRxCallback a0) {
    (void)a0;
}

ReturnCode RfalRfClass::rfalDeinitialize() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalSetMode(rfalMode a0, rfalBitRate a1, rfalBitRate a2) {
    (void)a0;
    (void)a1;
    (void)a2;
    return ST_ERR_NOT_IMPLEMENTED;
}

rfalMode RfalRfClass::rfalGetMode() {
    return RFAL_MODE_NONE;
}

ReturnCode RfalRfClass::rfalSetBitRate(rfalBitRate a0, rfalBitRate a1) {
    (void)a0;
    (void)a1;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalGetBitRate(rfalBitRate * a0, rfalBitRate * a1) {
    (void)a0;
    (void)a1;
    return ST_ERR_NOT_IMPLEMENTED;
}

void RfalRfClass::rfalSetErrorHandling(rfalEHandling a0) {
    (void)a0;
}

rfalEHandling RfalRfClass::rfalGetErrorHandling() {
    return RFAL_ERRORHANDLING_NONE;
}

void RfalRfClass::rfalSetObsvMode(uint8_t a0, uint8_t a1) {
    (void)a0;
    (void)a1;
}

void RfalRfClass::rfalGetObsvMode(uint8_t * a0, uint8_t * a1) {
    (void)a0;
    (void)a1;
}

void RfalRfClass::rfalDisableObsvMode() {
}

void RfalRfClass::rfalSetFDTPoll(uint32_t a0) {
    (void)a0;
}

uint32_t RfalRfClass::rfalGetFDTPoll() {
    return 0;
}

void RfalRfClass::rfalSetFDTListen(uint32_t a0) {
    (void)a0;
}

uint32_t RfalRfClass::rfalGetFDTListen() {
    return 0;
}

uint32_t RfalRfClass::rfalGetGT() {
    return 0;
}

void RfalRfClass::rfalSetGT(uint32_t a0) {
    (void)a0;
}

bool RfalRfClass::rfalIsGTExpired() {
    return false;
}

ReturnCode RfalRfClass::rfalFieldOnAndStartGT() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalFieldOff() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalStartTransceive(const rfalTransceiveContext * a0) {
    (void)a0;
    return ST_ERR_NOT_IMPLEMENTED;
}

rfalTransceiveState RfalRfClass::rfalGetTransceiveState() {
    return RFAL_TXRX_STATE_IDLE;
}

ReturnCode RfalRfClass::rfalGetTransceiveStatus() {
    return ST_ERR_NOT_IMPLEMENTED;
}

bool RfalRfClass::rfalIsTransceiveInTx() {
    return false;
}

bool RfalRfClass::rfalIsTransceiveInRx() {
    return false;
}

ReturnCode RfalRfClass::rfalGetTransceiveRSSI(uint16_t * a0) {
    (void)a0;
    return ST_ERR_NOT_IMPLEMENTED;
}

void RfalRfClass::rfalWorker() {
}

ReturnCode RfalRfClass::rfalISO14443ATransceiveShortFrame(rfal14443AShortFrameCmd a0, uint8_t * a1, uint8_t a2, uint16_t * a3, uint32_t a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalISO14443ATransceiveAnticollisionFrame(uint8_t * a0, uint8_t * a1, uint8_t * a2, uint16_t * a3, uint32_t a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalFeliCaPoll(rfalFeliCaPollSlots a0, uint16_t a1, uint8_t a2, rfalFeliCaPollRes * a3, uint8_t a4, uint8_t * a5, uint8_t * a6) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalISO15693TransceiveAnticollisionFrame(uint8_t * a0, uint8_t a1, uint8_t * a2, uint8_t a3, uint16_t * a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalISO15693TransceiveEOFAnticollision(uint8_t * a0, uint8_t a1, uint16_t * a2) {
    (void)a0;
    (void)a1;
    (void)a2;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalISO15693TransceiveEOF(uint8_t * a0, uint8_t a1, uint16_t * a2) {
    (void)a0;
    (void)a1;
    (void)a2;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalTransceiveBlockingTx(uint8_t * a0, uint16_t a1, uint8_t * a2, uint16_t a3, uint16_t * a4, uint32_t a5, uint32_t a6) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalTransceiveBlockingRx() {
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalTransceiveBlockingTxRx(uint8_t * a0, uint16_t a1, uint8_t * a2, uint16_t a3, uint16_t * a4, uint32_t a5, uint32_t a6) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    return ST_ERR_NOT_IMPLEMENTED;
}

bool RfalRfClass::rfalIsExtFieldOn() {
    return false;
}

ReturnCode RfalRfClass::rfalListenStart(uint32_t a0, const rfalLmConfPA * a1, const rfalLmConfPB * a2, const rfalLmConfPF * a3, uint8_t * a4, uint16_t a5, uint16_t * a6) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalListenSleepStart(rfalLmState a0, uint8_t * a1, uint16_t a2, uint16_t * a3) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalListenStop() {
    return ST_ERR_NOT_IMPLEMENTED;
}

rfalLmState RfalRfClass::rfalListenGetState(bool * a0, rfalBitRate * a1) {
    (void)a0;
    (void)a1;
    return RFAL_LM_STATE_NOT_INIT;
}

ReturnCode RfalRfClass::rfalListenSetState(rfalLmState a0) {
    (void)a0;
    return ST_ERR_NOT_IMPLEMENTED;
}

ReturnCode RfalRfClass::rfalWakeUpModeStart(const rfalWakeUpConfig * a0) {
    (void)a0;
    return ST_ERR_NOT_IMPLEMENTED;
}

bool RfalRfClass::rfalWakeUpModeHasWoke() {
    return false;
}

ReturnCode RfalRfClass::rfalWakeUpModeStop() {
    return ST_ERR_NOT_IMPLEMENTED;
}
