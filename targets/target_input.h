/**
 * @file target_input.h
 * Abstract input driver interface, implemented by targets/delphus.
 * The board uses six GPIO buttons (OK/Back/Left/Up/Down/Right).
 */

#pragma once

#include <furi.h>
#include <input.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize target-specific input hardware.
 * Called once at input service startup.
 */
void target_input_init(void);

/**
 * Poll target-specific input hardware and publish events.
 * Called every INPUT_POLL_MS from the input service main loop.
 *
 * @param pubsub           Input event pubsub channel
 * @param sequence_counter Monotonic sequence counter (caller owns, callee increments)
 */
void target_input_poll(FuriPubSub* pubsub, uint32_t* sequence_counter);

#ifdef __cplusplus
}
#endif
