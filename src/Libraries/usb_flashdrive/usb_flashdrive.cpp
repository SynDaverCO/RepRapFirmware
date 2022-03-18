/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../Hardware/SharedSpi/SharedSpiDevice.h"
#include "usb_flashdrive.h"

/**
 * Adjust USB_DEBUG to select debugging verbosity.
 *		0 - no debug messages
 *		1 - basic insertion/removal messages
 *		2 - show USB state transitions
 *		3 - perform block range checking
 *		4 - print each block access
 */
#define USB_DEBUG				 4
#define USB_STARTUP_DELAY 0

// uncomment to get 'printf' console debugging. NOT FOR UNO!
//#define HOST_DEBUG(...)		 {char s[255]; sprintf(s,__VA_ARGS__); debugPrintf("UHS:%s\n",s);}
//#define BS_HOST_DEBUG(...)	{char s[255]; sprintf(s,__VA_ARGS__); debugPrintf("UHS:%s\n",s);}
//#define MAX_HOST_DEBUG(...) {char s[255]; sprintf(s,__VA_ARGS__); debugPrintf("UHS:%s\n",s);}

#if SUPPORT_USB_FLASH_DRIVE

#include "lib-uhs2/usb.h"
#include "lib-uhs2/masstorage.h"

static constexpr Pin default_csPin = PortCPin(24);  // CONN_SD: SPI0_CS0
static constexpr Pin default_int1Pin = PortAPin(7); // CONN_SD: ENC_SW
static constexpr uint32_t default_clockFreq = 500000;

static USB *usb = nullptr;
static BulkOnly *bulk = nullptr;

#define UHS_START usb->start()
#define UHS_STATE(state) USB_STATE_##state

static enum {
	UNINITIALIZED,
	DO_STARTUP,
	WAIT_FOR_DEVICE,
	WAIT_FOR_LUN,
	MEDIA_READY,
	MEDIA_ERROR
} state;

#if USB_DEBUG >= 3
	uint32_t lun0_capacity;
#endif

bool usbStartup() {
	if (state <= DO_STARTUP) {
		debugPrintf("Starting USB host...\n");
		if (!UHS_START) {
			//LCD_MESSAGE(MSG_MEDIA_USB_FAILED);
			return false;
		}

		// SPI quick test - check revision register
		switch (usb->regRd(rREVISION)) {
			case 0x01: debugPrintf("rev.01 started"); break;
			case 0x12: debugPrintf("rev.02 started"); break;
			case 0x13: debugPrintf("rev.03 started"); break;
			default:	 debugPrintf("started. rev unknown."); break;
		}
		state = WAIT_FOR_DEVICE;
	}
	return true;
}

// The USB library needs to be called periodically to detect USB thumbdrive
// insertion and removals. Call this idle() function periodically to allow
// the USB library to monitor for such events. This function also takes care
// of initializing the USB library for the first time.

void usbIdle() {
	static uint32_t next_state_ms = millis();

	#define PENDING(NOW,SOON) ((int32_t)(NOW-(SOON))<0)
	#define ELAPSED(NOW,SOON) (!PENDING(NOW,SOON))
	#define GOTO_STATE_AFTER_DELAY(STATE, DELAY) do{ state = STATE; next_state_ms = millis() + DELAY; }while(0)

	if(!usb || !bulk) {
		if(millis() - next_state_ms < 10000)
			return;

		debugPrintf("Allocating USB object...\n");
		usb = new USB(SharedSpiDevice::GetMainSharedSpiDevice(), default_clockFreq, default_csPin, default_int1Pin);
		if(!usb) {
			debugPrintf("Failed\n");
			return;
		}
		debugPrintf("Allocating Bulk object...\n");
		bulk = new BulkOnly(usb);
		if(!bulk) {
			debugPrintf("Failed\n");
			return;
		}
	}

	usb->Task();

	const uint8_t task_state = usb->getUsbTaskState();

	#if USB_DEBUG >= 2
		if (state > DO_STARTUP) {
			static uint8_t laststate = 232;
			if (task_state != laststate) {
				laststate = task_state;
				#define UHS_USB_DEBUG(x) case x: debugPrintf(#x "\n"); break
				switch (task_state) {
					UHS_USB_DEBUG(USB_STATE_DETACHED);
					UHS_USB_DEBUG(USB_DETACHED_SUBSTATE_INITIALIZE);
					UHS_USB_DEBUG(USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE);
					UHS_USB_DEBUG(USB_DETACHED_SUBSTATE_ILLEGAL);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_SETTLE);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_RESET_DEVICE);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_WAIT_SOF);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_WAIT_RESET);
					UHS_USB_DEBUG(USB_ATTACHED_SUBSTATE_GET_DEVICE_DESCRIPTOR_SIZE);
					UHS_USB_DEBUG(USB_STATE_ADDRESSING);
					UHS_USB_DEBUG(USB_STATE_CONFIGURING);
					UHS_USB_DEBUG(USB_STATE_RUNNING);
					UHS_USB_DEBUG(USB_STATE_ERROR);
					default:
						debugPrintf("UHS_USB_HOST_STATE: %d\n", task_state);
						break;
				}
			}
		}
	#endif

	 if (ELAPSED(millis(), next_state_ms)) {
		GOTO_STATE_AFTER_DELAY(state, 250); // Default delay

		switch (state) {

			case UNINITIALIZED:
				#ifndef MANUAL_USB_STARTUP
					GOTO_STATE_AFTER_DELAY( DO_STARTUP, USB_STARTUP_DELAY );
				#endif
				break;

			case DO_STARTUP: usbStartup(); break;

			case WAIT_FOR_DEVICE:
				if (task_state == UHS_STATE(RUNNING)) {
					#if USB_DEBUG >= 1
						debugPrintf("USB device inserted\n");
					#endif
					GOTO_STATE_AFTER_DELAY( WAIT_FOR_LUN, 250 );
				}
				break;

			case WAIT_FOR_LUN:
				/* USB device is inserted, but if it is an SD card,
				 * adapter it may not have an SD card in it yet. */
				if (bulk->LUNIsGood(0)) {
					#if USB_DEBUG >= 1
						debugPrintf("LUN is good\n");
					#endif
					GOTO_STATE_AFTER_DELAY( MEDIA_READY, 100 );
				}
				else {
					#ifdef USB_HOST_MANUAL_POLL
						// Make sure we catch disconnect events
						usb->busprobe();
						usb->VBUS_changed();
					#endif
					#if USB_DEBUG >= 1
						debugPrintf("Waiting for media\n");
					#endif
					//LCD_MESSAGE(MSG_MEDIA_WAITING);
					GOTO_STATE_AFTER_DELAY(state, 2000);
				}
				break;

			case MEDIA_READY:
				#if USB_DEBUG >= 1
					//debugPrintf("USB device ready\n");
				#endif
				break;
			case MEDIA_ERROR: break;
		}

		if (state > WAIT_FOR_DEVICE && task_state != UHS_STATE(RUNNING)) {
			// Handle device removal events
			#if USB_DEBUG >= 1
				debugPrintf("USB device removed\n");
			#endif
			if (state != MEDIA_READY)
				//LCD_MESSAGE(MSG_MEDIA_USB_REMOVED);
			GOTO_STATE_AFTER_DELAY(WAIT_FOR_DEVICE, 0);
		}

		else if (state > WAIT_FOR_LUN && !bulk->LUNIsGood(0)) {
			// Handle media removal events
			#if USB_DEBUG >= 1
				debugPrintf("Media removed\n");
			#endif
			//LCD_MESSAGE(MSG_MEDIA_REMOVED);
			GOTO_STATE_AFTER_DELAY(WAIT_FOR_DEVICE, 0);
		}

		else if (task_state == UHS_STATE(ERROR)) {
			//LCD_MESSAGE(MSG_MEDIA_READ_ERROR);
			GOTO_STATE_AFTER_DELAY(MEDIA_ERROR, 0);
		}
	}
}

// Marlin calls this function to check whether an USB drive is inserted.
// This is equivalent to polling the SD_DETECT when using SD cards.
bool usbIsInserted() {
	return state == MEDIA_READY;
}

bool usbIsReady() {
	return state > DO_STARTUP && usb->getUsbTaskState() == UHS_STATE(RUNNING);
}

// Marlin calls this to initialize an SD card once it is inserted.
bool usbMountCard() {
	if (!usbIsInserted()) return false;

	#if USB_DEBUG >= 1
	const uint32_t sectorSize = bulk->GetSectorSize(0);
	if (sectorSize != 512) {
		debugPrintf("Expecting sector size of 512. Got: %ld\n", sectorSize);
		return false;
	}
	#endif

	#if USB_DEBUG >= 3
		lun0_capacity = bulk->GetCapacity(0);
		debugPrintf("LUN Capacity (in blocks): %ld\n", lun0_capacity);
	#endif
	return true;
}

bool USB_REDIRECT_mem_wr_protect(uint8_t lun) {
	#if USB_DEBUG >= 4
		debugPrintf("USB_REDIRECT_mem_wr_protect: lun %d\n", lun);
	#endif
	return false;
}

Ctrl_status USB_REDIRECT_mem_test_unit_ready(uint8_t lun) {
	#if USB_DEBUG >= 4
		debugPrintf("USB_REDIRECT_mem_test_unit_ready: lun %d\n", lun);
	#endif
	return usbIsInserted() ? CTRL_GOOD : CTRL_NO_PRESENT;
}

// Returns the capacity of the card in blocks.

Ctrl_status USB_REDIRECT_mem_read_capacity(uint8_t lun, uint32_t *u32_nb_sector) {
	if(!usbIsInserted()) {
		return CTRL_NO_PRESENT;
	}
	*u32_nb_sector = bulk->GetCapacity(0);
	#if USB_DEBUG >= 4
		debugPrintf("USB_REDIRECT_mem_read_capacity: Read capacity %ld\n", *u32_nb_sector);
	#endif
	return CTRL_GOOD;
}

uint32_t USB_REDIRECT_sd_mmc_get_interface_speed(uint8_t slot) {
	return usbIsInserted() ? 100 : 0;
}

card_type_t USB_REDIRECT_sd_mmc_get_type(uint8_t slot) {
	return CARD_TYPE_SD;
}

Ctrl_status USB_REDIRECT_memory_2_ram(uint8_t lun, uint32_t addr, void *ram, uint32_t numBlocks) {
	if(!usbIsInserted()) {
		return CTRL_NO_PRESENT;
	}
	#if USB_DEBUG >= 3
		if (addr >= lun0_capacity) {
			debugPrintf("Attempt to read past end of LUN: %ld\n", addr);
			return CTRL_FAIL;
		}
	#endif
	#if USB_DEBUG >= 4
		debugPrintf("USB_REDIRECT_memory_2_ram: Read addr %ld\n", addr);
	#endif
	if(bulk->Read(0, addr, 512, numBlocks, (uint8_t *)ram) == 0) {
		return CTRL_GOOD;
	}
	return CTRL_FAIL;
}

Ctrl_status USB_REDIRECT_ram_2_memory(uint8_t lun, uint32_t addr, const void *ram, uint32_t numBlocks) {
	if(!usbIsInserted()) {
		return CTRL_NO_PRESENT;
	}
	#if USB_DEBUG >= 3
		if (addr >= lun0_capacity) {
			debugPrintf("Attempt to write past end of LUN:	%ld\n", addr);
			return CTRL_FAIL;
		}
	#endif
	#if USB_DEBUG >= 4
		debugPrintf("USB_REDIRECT_ram_2_memory: Write block %ld\n", addr);
	#endif
	if(bulk->Write(0, addr, 512, numBlocks, (const uint8_t*)ram) == 0) {
		return CTRL_GOOD;
	}
	return CTRL_FAIL;
}

sd_mmc_err_t USB_REDIRECT_sd_mmc_check(uint8_t slot) {
	 return usbMountCard() ? SD_MMC_OK : SD_MMC_ERR_NO_CARD;
}

uint32_t USB_REDIRECT_sd_mmc_get_capacity(uint8_t slot) {
	return usbIsInserted() ? bulk->GetCapacity(0) : 0;
}

#endif // SUPPORT_USB_FLASH_DRIVE
