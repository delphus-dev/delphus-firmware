/**
 * @file board.h
 * Board selection header — includes the active board definition.
 *
 * The only supported board is Delphus; the build system defines
 * BOARD_INCLUDE accordingly.
 */

#pragma once

#if defined(BOARD_INCLUDE)
#include BOARD_INCLUDE
#else
#include "board_delphus.h"
#endif

/* Sanity check: every board must define these */
#ifndef BOARD_NAME
#error "Board header must define BOARD_NAME"
#endif
#ifndef BOARD_PIN_LCD_MOSI
#error "Board header must define LCD pins"
#endif
