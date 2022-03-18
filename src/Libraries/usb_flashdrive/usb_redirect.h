#pragma once

#include "../sd_mmc/ctrl_access.h"
#include "usb_flashdrive.h"

#define USB_LUN (MAX_LUN - 1)
#define USB_LUN_REDIRECT(LUN, FUNC, ...) if (LUN == USB_LUN) return USB_REDIRECT_ ## FUNC(LUN, ##__VA_ARGS__ )

#define EXIT_IF_USB_SLOT(lun) if (lun == USB_LUN) {return usbMountCard() ? SD_MMC_OK : SD_MMC_ERR_NO_CARD;}

#ifdef __cplusplus
extern "C" {
#endif
	void debugPrintf(const char* fmt, ...) noexcept __attribute__ ((format (printf, 1, 2)));
#ifdef __cplusplus
}
#endif
