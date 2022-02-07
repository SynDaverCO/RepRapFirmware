/*
 * Devices.cpp
 *
 *  Created on: 9 Jul 2020
 *      Author: David
 *  License: GNU GPL v3
 */

#include "Devices.h"
#include <RepRapFirmware.h>
#include <AnalogIn.h>
#include <AnalogOut.h>
#include <Platform/TaskPriorities.h>

#include <hal_gpio.h>
#include <hal_usb_device.h>
#include <peripheral_clk_config.h>

// Analog input support
constexpr size_t AnalogInTaskStackWords = 300;
static Task<AnalogInTaskStackWords> analogInTask;

// Serial device support
void Serial0PortInit(AsyncSerial *) noexcept
{
	SetPinFunction(Serial0TxPin, Serial0PinFunction);
	SetPinFunction(Serial0RxPin, Serial0PinFunction);
}

void Serial0PortDeinit(AsyncSerial *) noexcept
{
	pinMode(Serial0TxPin, INPUT_PULLUP);
	pinMode(Serial0RxPin, INPUT_PULLUP);
}

void Serial1PortInit(AsyncSerial *) noexcept
{
	SetPinFunction(Serial1TxPin, Serial1PinFunction);
	SetPinFunction(Serial1RxPin, Serial1PinFunction);
}

void Serial1PortDeinit(AsyncSerial *) noexcept
{
	pinMode(Serial1TxPin, INPUT_PULLUP);
	pinMode(Serial1RxPin, INPUT_PULLUP);
}

AsyncSerial serialUart0(Serial0SercomNumber, Sercom0RxPad, 512, 512, Serial0PortInit, Serial0PortDeinit);
AsyncSerial serialUart1(Serial1SercomNumber, Sercom1RxPad, 512, 512, Serial1PortInit, Serial1PortDeinit);

# if !defined(SERIAL0_ISR0) || !defined(SERIAL0_ISR2) || !defined(SERIAL0_ISR3)
#  error SERIAL0_ISRn not defined
# endif

void SERIAL0_ISR0() noexcept
{
	serialUart0.Interrupt0();
}

void SERIAL0_ISR2() noexcept
{
	serialUart0.Interrupt2();
}

void SERIAL0_ISR3() noexcept
{
	serialUart0.Interrupt3();
}

# if !defined(SERIAL1_ISR0) || !defined(SERIAL1_ISR2) || !defined(SERIAL1_ISR3)
#  error SERIAL1_ISRn not defined
# endif

void SERIAL1_ISR0() noexcept
{
	serialUart1.Interrupt0();
}

void SERIAL1_ISR2() noexcept
{
	serialUart1.Interrupt2();
}

void SERIAL1_ISR3() noexcept
{
	serialUart1.Interrupt3();
}

SerialCDC serialUSB(UsbVBusPin, 512, 512);

static void UsbInit() noexcept
{
	// Set up USB clock
	hri_gclk_write_PCHCTRL_reg(GCLK, USB_GCLK_ID, GCLK_PCHCTRL_GEN(GclkNum48MHz) | GCLK_PCHCTRL_CHEN);
	hri_mclk_set_AHBMASK_USB_bit(MCLK);
	hri_mclk_set_APBBMASK_USB_bit(MCLK);

	// Set up USB pins
	// This is the code generated by Atmel Start. I don't know whether it is all necessary.
	gpio_set_pin_direction(PortAPin(24), GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PortAPin(24), false);
	gpio_set_pin_pull_mode(PortAPin(24), GPIO_PULL_OFF);
	gpio_set_pin_function(PortAPin(24), PINMUX_PA24H_USB_DM);

	gpio_set_pin_direction(PortAPin(25), GPIO_DIRECTION_OUT);
	gpio_set_pin_level(PortAPin(25), false);
	gpio_set_pin_pull_mode(PortAPin(25), GPIO_PULL_OFF);
	gpio_set_pin_function(PortAPin(25), PINMUX_PA25H_USB_DP);
}

static void SdhcInit() noexcept
{
	// Set up SDHC clock
#if defined(DUET3MINI_V04)
	// Using SDHC 1
	hri_mclk_set_AHBMASK_SDHC1_bit(MCLK);
	hri_gclk_write_PCHCTRL_reg(GCLK, SDHC1_GCLK_ID, GCLK_PCHCTRL_GEN(GclkNum90MHz) | GCLK_PCHCTRL_CHEN);
	hri_gclk_write_PCHCTRL_reg(GCLK, SDHC1_GCLK_ID_SLOW, GCLK_PCHCTRL_GEN(GclkNum31KHz) | GCLK_PCHCTRL_CHEN);
#elif defined(DUET3MINI4)
	// Using SDHC 0
	hri_mclk_set_AHBMASK_SDHC0_bit(MCLK);
	hri_gclk_write_PCHCTRL_reg(GCLK, SDHC0_GCLK_ID, GCLK_PCHCTRL_GEN(GclkNum90MHz) | GCLK_PCHCTRL_CHEN);
	hri_gclk_write_PCHCTRL_reg(GCLK, SDHC0_GCLK_ID_SLOW, GCLK_PCHCTRL_GEN(GclkNum31KHz) | GCLK_PCHCTRL_CHEN);
#else
# error Unknown board
#endif

	// Setup SD card interface pins
	for (Pin p : SdMciPins)
	{
		SetPinFunction(p, SdMciPinsFunction);
	}
}

void DeviceInit() noexcept
{
	// Ensure the Ethernet PHY or WiFi module is held reset
	pinMode(EspResetPin, OUTPUT_LOW);

	UsbInit();
	SdhcInit();

	AnalogIn::Init(NvicPriorityAdc);
	AnalogOut::Init();
	analogInTask.Create(AnalogIn::TaskLoop, "AIN", nullptr, TaskPriority::AinPriority);
}

void StopAnalogTask() noexcept
{
	AnalogIn::Exit();								// make sure that the ISR doesn't try to wake the analog task after we terminate it
	analogInTask.TerminateAndUnlink();
}

// End
