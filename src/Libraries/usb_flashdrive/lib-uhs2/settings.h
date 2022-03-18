/**
 * Copyright (C) 2011 Circuits At Home, LTD. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact information
 * -------------------
 *
 * Circuits At Home, LTD
 * Web      :  https://www.circuitsathome.com
 * e-mail   :  support@circuitsathome.com
 */

#pragma once

#include "../usb_flashdrive.h"

#include "macros.h"

#if SUPPORT_USB_FLASH_DRIVE
  ////////////////////////////////////////////////////////////////////////////////
  /* Added by Bill Greiman to speed up mass storage initialization with USB
   * flash drives and simple USB hard drives.
   * Disable this by defining DELAY(x) to be delay(x).
   */
  //#define delay(x)  if ((x) < 200) safe_delay(x)
  /* Almost all USB flash drives and simple USB hard drives fail the write
   * protect test and add 20 - 30 seconds to USB init.  Set SKIP_WRITE_PROTECT
   * to nonzero to skip the test and assume the drive is writable.
   */
  #define SKIP_WRITE_PROTECT 1
  /* Since Marlin only cares about USB flash drives, we only need one LUN. */
  #define MASS_MAX_SUPPORTED_LUN 1
#endif

////////////////////////////////////////////////////////////////////////////////
// SPI Configuration
////////////////////////////////////////////////////////////////////////////////

#ifndef USB_SPI
  #define USB_SPI SPI
  //#define USB_SPI SPI1
#endif

////////////////////////////////////////////////////////////////////////////////
// DEBUGGING
////////////////////////////////////////////////////////////////////////////////

/* Set this to 1 to activate serial debugging */
#define ENABLE_UHS_DEBUGGING 0

////////////////////////////////////////////////////////////////////////////////
// MASS STORAGE
////////////////////////////////////////////////////////////////////////////////
// ******* IMPORTANT *******
// Set this to 1 to support single LUN devices, and save RAM. -- I.E. thumb drives.
// Each LUN needs ~13 bytes to be able to track the state of each unit.
#ifndef MASS_MAX_SUPPORTED_LUN
  #define MASS_MAX_SUPPORTED_LUN 8
#endif

#if !defined(DEBUG_USB_HOST) && ENABLE_UHS_DEBUGGING
  #define DEBUG_USB_HOST
#endif

// Fix defines on Arduino Due
#ifdef ARDUINO_SAM_DUE
  #ifdef tokSETUP
    #undef tokSETUP
  #endif
  #ifdef tokIN
    #undef tokIN
  #endif
  #ifdef tokOUT
    #undef tokOUT
  #endif
  #ifdef tokINHS
    #undef tokINHS
  #endif
  #ifdef tokOUTHS
    #undef tokOUTHS
  #endif
#endif
