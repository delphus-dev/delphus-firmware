#include "furi_hal_pin_arbiter.h"

#include <furi.h>

static FuriMutex* arbiter_mutex = NULL;
static volatile FuriHalSharedPinOwner pin_owner = FuriHalSharedPinNone;

static void ensure_mutex(void) {
    if(!arbiter_mutex) arbiter_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
}

bool furi_hal_shared_pins_claim(FuriHalSharedPinOwner owner) {
    ensure_mutex();
    bool granted = false;
    if(furi_mutex_acquire(arbiter_mutex, FuriWaitForever) == FuriStatusOk) {
        if(pin_owner == FuriHalSharedPinNone || pin_owner == owner) {
            pin_owner = owner;
            granted = true;
        }
        furi_mutex_release(arbiter_mutex);
    }
    return granted;
}

void furi_hal_shared_pins_release(FuriHalSharedPinOwner owner) {
    ensure_mutex();
    if(furi_mutex_acquire(arbiter_mutex, FuriWaitForever) == FuriStatusOk) {
        if(pin_owner == owner) pin_owner = FuriHalSharedPinNone;
        furi_mutex_release(arbiter_mutex);
    }
}

FuriHalSharedPinOwner furi_hal_shared_pins_owner(void) {
    return pin_owner;
}
