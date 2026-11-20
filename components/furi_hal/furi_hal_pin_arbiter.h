#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IO47/IO48 are shared between the ST25R3916 (NFC) and the RDM6300 (RFID).
 * Whoever actually uses its API claims them; the other peripheral stays idle. */
typedef enum {
    FuriHalSharedPinNone = 0,
    FuriHalSharedPinNfc,
    FuriHalSharedPinRfid,
} FuriHalSharedPinOwner;

bool furi_hal_shared_pins_claim(FuriHalSharedPinOwner owner);
void furi_hal_shared_pins_release(FuriHalSharedPinOwner owner);
FuriHalSharedPinOwner furi_hal_shared_pins_owner(void);

#ifdef __cplusplus
}
#endif
