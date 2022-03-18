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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "../sd_mmc/ctrl_access.h"
#include "../sd_mmc/sd_mmc_mem.h"
#include "../sd_mmc/sd_mmc.h"

#include <Core.h>
#undef ACC // Avoid name conflict
#define PSTR(a) a

#define SUPPORT_USB_FLASH_DRIVE 1

#ifdef __cplusplus
extern "C" {
#endif

	void usbStateDebug();
	bool usbStartup();
	bool usbIsInserted();
	bool usbMountCard();

	bool usbIsReady();
	void usbIdle();

	sd_mmc_err_t USB_REDIRECT_sd_mmc_check(uint8_t slot);
	uint32_t USB_REDIRECT_sd_mmc_get_capacity(uint8_t slot);
	uint32_t USB_REDIRECT_sd_mmc_get_interface_speed(uint8_t slot);
	card_type_t USB_REDIRECT_sd_mmc_get_type(uint8_t slot);

	bool        USB_REDIRECT_mem_wr_protect(uint8_t lun);
	Ctrl_status USB_REDIRECT_mem_test_unit_ready(uint8_t lun);
	Ctrl_status USB_REDIRECT_mem_read_capacity(uint8_t lun, uint32_t *u32_nb_sector);
	Ctrl_status USB_REDIRECT_memory_2_ram(uint8_t lun, uint32_t addr, void *ram, uint32_t numBlocks);
	Ctrl_status USB_REDIRECT_ram_2_memory(uint8_t lun, uint32_t addr, const void *ram, uint32_t numBlocks);
#ifdef __cplusplus
}
#endif
