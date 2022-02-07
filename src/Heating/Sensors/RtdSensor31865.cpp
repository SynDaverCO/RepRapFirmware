/*
 * RtdSensor31865.cpp
 *
 *  Created on: 8 Jun 2017
 *      Author: David
 */

#include "RtdSensor31865.h"

#if SUPPORT_SPI_SENSORS

#include <Platform/RepRap.h>
#include <Platform/Platform.h>
#include <GCodes/GCodeBuffer/GCodeBuffer.h>

#if SUPPORT_REMOTE_COMMANDS
# include <CanMessageGenericParser.h>
#endif

const uint32_t MAX31865_Frequency = 4000000;	// maximum for MAX31865 is 5MHz

// SPI modes:
// If the inactive state of SCL is LOW (CPOL = 0) (in the case of the MAX31865, this is sampled on the falling edge of CS):
// The MAX31865 changes data after the rising edge of CLK, and samples input data on the falling edge.
// This requires NCPHA = 0.
const SpiMode MAX31865_SpiMode = SPI_MODE_1;

// Define the minimum interval between readings. The MAX31865 needs 62.5ms in 50Hz filter mode.
const uint32_t MinimumReadInterval = 100;		// minimum interval between reads, in milliseconds

// Default configuration register
// Note that to get the MAX31865 to do continuous conversions, we need to set the bias bit as well as the continuous-conversion bit
//  Vbias=1
//  Conversion mode=1
//	1shot = 0
//	3wire=0
//	Fault detection=00 no action
//	Fault status=1 clear any existing fault
//	50/60Hz reject=1 for 50Hz (0 for 60Hz)
const uint8_t DefaultCr0 = 0b11000011;
const uint8_t Cr0ReadMask = 0b11011101;		// bits 1 and 5 auto clear, so ignore the value read

const uint16_t DefaultRef = 400;

RtdSensor31865::RtdSensor31865(unsigned int sensorNum) noexcept
	: SpiTemperatureSensor(sensorNum, "PT100 (MAX31865)", MAX31865_SpiMode, MAX31865_Frequency),
	  rref(DefaultRef), cr0(DefaultCr0)
{
}

// Configure this temperature sensor
GCodeResult RtdSensor31865::Configure(GCodeBuffer& gb, const StringRef& reply, bool& changed)
{
	if (!ConfigurePort(gb, reply, changed))
	{
		return GCodeResult::error;
	}
	TryConfigureSensorName(gb, changed);
	if (gb.Seen('F'))
	{
		changed = true;
		if (gb.GetIValue() == 60)
		{
			cr0 &= ~0x01;		// set 60Hz rejection
		}
		else
		{
			cr0 |= 0x01;		// default to 50Hz rejection
		}
	}

	if (gb.Seen('W'))
	{
		changed = true;
		if (gb.GetUIValue() == 3)
		{
			cr0 |= 0x10;		// 3 wire configuration
		}
		else
		{
			cr0 &= ~0x10;		// 2 or 4 wire configuration
		}
	}

	if (gb.Seen('R'))
	{
		changed = true;
		rref = (uint16_t)gb.GetUIValue();
	}

	return FinishConfiguring(changed, reply);
}

#if SUPPORT_REMOTE_COMMANDS

GCodeResult RtdSensor31865::Configure(const CanMessageGenericParser& parser, const StringRef& reply) noexcept
{
	bool seen = false;
	if (!ConfigurePort(parser, reply, seen))
	{
		return GCodeResult::error;
	}

	uint8_t paramF;
	if (parser.GetUintParam('F', paramF))
	{
		seen = true;
		if (paramF == 60)
		{
			cr0 &= ~0x01;		// set 60Hz rejection
		}
		else
		{
			cr0 |= 0x01;		// default to 50Hz rejection
		}
	}

	uint8_t paramW;
	if (parser.GetUintParam('W', paramW))
	{
		seen = true;
		if (paramW == 3)
		{
			cr0 |= 0x10;		// 3 wire configuration
		}
		else
		{
			cr0 &= ~0x10;		// 2 or 4 wire configuration
		}
	}

	float paramR;
	if (parser.GetFloatParam('R', paramR))
	{
		seen = true;
		rref = (uint16_t)paramR;
	}

	return FinishConfiguring(seen, reply);
}

#endif

GCodeResult RtdSensor31865::FinishConfiguring(bool changed, const StringRef& reply) noexcept
{
	if (changed)
	{
		// Initialise the sensor
		InitSpi();

		TemperatureError rslt;
		for (unsigned int i = 0; i < 3; ++i)		// try 3 times
		{
			rslt = TryInitRtd();
			if (rslt == TemperatureError::success)
			{
				break;
			}
			delay(MinimumReadInterval);
		}

		lastReadingTime = millis();
		lastResult = rslt;
		lastTemperature = 0.0;

		if (rslt != TemperatureError::success)
		{
			reply.printf("Failed to initialise RTD: %s", TemperatureErrorString(rslt));
			return GCodeResult::error;
		}
	}
	else
	{
		CopyBasicDetails(reply);
		reply.catf(", %s wires, reject %dHz, reference resistor %u ohms", (cr0 & 0x10) ? "3" : "2/4", (cr0 & 0x01) ? 50 : 60, (unsigned int)rref);
	}
	return GCodeResult::ok;
}

// Try to initialise the RTD
TemperatureError RtdSensor31865::TryInitRtd() const noexcept
{
	const uint8_t modeData[2] = { 0x80, cr0 };			// write register 0
	uint32_t rawVal;
	TemperatureError sts = DoSpiTransaction(modeData, ARRAY_SIZE(modeData), rawVal);

	if (sts == TemperatureError::success)
	{
		delayMicroseconds(1);
		static const uint8_t readData[2] = { 0x00, 0x00 };	// read register 0
		sts = DoSpiTransaction(readData, ARRAY_SIZE(readData), rawVal);
	}

	//debugPrintf("Status %d data %04x\n", (int)sts, (uint16_t)rawVal);
	if (sts == TemperatureError::success && (rawVal & Cr0ReadMask) != (cr0 & Cr0ReadMask))
	{
		sts = TemperatureError::badResponse;
	}

	return sts;
}

void RtdSensor31865::Poll() noexcept
{
	static const uint8_t dataOut[4] = {0, 0x55, 0x55, 0x55};			// read registers 0 (control), 1 (MSB) and 2 (LSB)
	uint32_t rawVal;
	TemperatureError sts = DoSpiTransaction(dataOut, ARRAY_SIZE(dataOut), rawVal);

	if (sts != TemperatureError::success)
	{
		SetResult(sts);
	}
	else
	{
		if (   (((rawVal >> 16) & Cr0ReadMask) != (cr0 & Cr0ReadMask))	// if control register not as expected
			|| (rawVal & 1) != 0										// or fault bit set
		   )
		{
			static const uint8_t faultDataOut[2] = {0x07, 0x55};
			if (DoSpiTransaction(faultDataOut, ARRAY_SIZE(faultDataOut), rawVal) == TemperatureError::success)	// read the fault register
			{
				sts = (rawVal & 0x04) ? TemperatureError::overOrUnderVoltage
							: (rawVal & 0x18) ? TemperatureError::openCircuit
								: TemperatureError::hardwareError;
			}
			else
			{
				sts = TemperatureError::hardwareError;
			}
			SetResult(sts);
			delayMicroseconds(1);										// MAX31865 requires CS to be high for 400ns minimum
			TryInitRtd();												// clear the fault and hope for better luck next time
		}
		else
		{
			const uint16_t ohmsx100 = (uint16_t)((((rawVal >> 1) & 0x7FFF) * rref * 100) >> 15);
			float t;
			sts = GetPT100Temperature(t, ohmsx100);
			SetResult(t, sts);
		}
	}
}

#endif // SUPPORT_SPI_SENSORS

// End
