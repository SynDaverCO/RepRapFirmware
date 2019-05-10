/*
 * MessageFormats.h
 *
 *  Created on: 29 Mar 2019
 *      Author: Christian
 */

#ifndef SRC_LINUX_MESSAGEFORMATS_H_
#define SRC_LINUX_MESSAGEFORMATS_H_

#include <cstddef>
#include <cstdint>
#include <ctime>

#include "RepRapFirmware.h"
#include "MessageType.h"

constexpr uint8_t LinuxFormatCode = 0x5F;
constexpr uint8_t InvalidFormatCode = 0xC9;			// must be different from any other format code

constexpr uint16_t LinuxProtocolVersion = 1;

constexpr size_t LinuxTransferBufferSize = 2048;	// maximum length of a data transfer. Must be a multiple of 4 and kept in sync with Duet Control Server!
static_assert(LinuxTransferBufferSize % sizeof(uint32_t) == 0, "LinuxBufferSize must be a whole number of dwords");

constexpr size_t MaxCodeBufferSize = 192;			// maximum length of a G/M/T-code in binary encoding
static_assert(MaxCodeBufferSize % sizeof(uint32_t) == 0, "MaxCodeBufferSize must be a whole number of dwords");
static_assert(MaxCodeBufferSize >= GCODE_LENGTH, "MaxCodeBufferSize must be at least as big as GCODE_LENGTH");

constexpr uint32_t SpiTransferTimeout = 500;		// maximum allowed delay between data exchanges during a full transfer (in ms)
constexpr uint32_t SpiConnectionTimeout = 4000;		// maximum time to wait for the next transfer (in ms)

// Shared structures
struct TransferHeader
{
	uint8_t formatCode;
	uint8_t numPackets;
	uint16_t protocolVersion;
	uint16_t sequenceNumber;
	uint16_t dataLength;
	uint16_t checksumData;
	uint16_t checksumHeader;
};

enum TransferResponse : uint32_t
{
	Success = 1,
	BadFormat = 2,
	BadProtocolVersion = 3,
	BadDataLength = 4,
	BadHeaderChecksum = 5,
	BadDataChecksum = 6,

	BadResponse = 0xFEFEFEFE
};

struct PacketHeader
{
	uint16_t request;
	uint16_t id;
	uint16_t length;
	uint16_t resendPacketId;
};

enum class CodeChannel : uint8_t
{
	http = 0,
	telnet = 1,
	file = 2,
	serial = 3,
	aux = 4,
	daemon = 5,
	queue = 6,
	lcd = 7,
	spi = 8,
	autoPause = 9
};

enum class DataType : uint8_t
{
    Int = 0,				// int32_t
    UInt = 1,				// uint32_t
    Float = 2,				// float
    IntArray = 3,			// int32_t[]
    UIntArray = 4,			// uint32_t[]
    FloatArray = 5,			// float[]
    String = 6,				// char[]
    Expression = 7			// char[] but containing '['...']'
};

struct ObjectModelHeader
{
	uint16_t length;
	uint8_t module;
	uint8_t padding;
};

struct HeightMapHeader
{
	float xMin;
	float xMax;
	float xSpacing;
	float yMin;
	float yMax;
	float ySpacing;
	float radius;
	uint16_t numX;
	uint16_t numY;
};

struct LockUnlockHeader
{
	CodeChannel channel;
	uint8_t paddingA;
	uint16_t paddingB;
};

// RepRapFirmware to Linux
enum class FirmwareRequest : uint16_t
{
	ResendPacket = 0,		// Request the retransmission of the given packet
	ReportState = 1,		// Response to the state request
	ObjectModel = 2,		// Response to an object model request
	CodeReply = 3,			// Response to a G/M/T-code
	ExecuteMacro = 4,		// Request execution of a macro file
	AbortFile = 5,			// Request the current file to be closed
	StackEvent = 6,			// Stack has been changed
	PrintPaused = 7,		// Print has been paused
	HeightMap = 8,			// Response to a heightmap request
	Locked = 9				// Movement has been locked and machine is in standstill
};

struct ReportStateHeader
{
	uint32_t busyChannels;
};

struct CodeReplyHeader
{
	MessageType messageType;
	uint16_t length;
	uint16_t padding;
};

struct ExecuteMacroHeader
{
	CodeChannel channel;
	bool reportMissing;
	uint8_t length;
	uint8_t padding;
};

struct AbortFileHeader
{
	CodeChannel channel;
	uint8_t paddingA;
	uint16_t paddingB;
};

enum StackEventFlags : uint16_t
{
	none = 0,
	drivesRelative = 1,
	axesRelative = 2,
	usingInches = 4
};

struct StackEventHeader
{
	CodeChannel channel;
	uint8_t depth;
	StackEventFlags flags;
	float feedrate;
};

enum class PrintPausedReason : uint8_t
{
	user = 1,
	gcode = 2,
	filamentChange = 3,
	trigger = 4,
	heaterFault = 5,
	filament = 6,
	stall = 7,
	lowVoltage = 8
};

struct PrintPausedHeader
{
	uint32_t filePosition;
	PrintPausedReason pauseReason;
	uint8_t paddingA;
	uint16_t paddingB;
};

// Linux to RepRapFirmware
enum class LinuxRequest : uint16_t
{
	GetState = 0,								// Request state of the GCodeBuffers
    EmergencyStop = 1,							// Perform immediate emergency stop
    Reset = 2,									// Reset the controller
    Code = 3,									// Request execution of a G/M/T-code
    GetObjectModel = 4,							// Request a part of the machine's object model
    SetObjectModel = 5,							// Set a value in the machine's object model
    PrintStarted = 6,							// Print has been started, set file print information
    PrintStopped = 7,							// Print has been stopped, reset file print information
    MacroCompleted = 8,							// Notification that a macro file has been fully processed
    GetHeightMap = 9,							// Request the heightmap coordinates as generated by G29 S0
    SetHeightMap = 10,							// Set the heightmap coordinates via G29 S1
    LockMovementAndWaitForStandstill = 11,		// Lock movement and wait for standstill
    Unlock = 12,								// Unlock occupied resources

	InvalidRequest = 12
};

enum CodeFlags : uint8_t
{
	NoMajorCommandNumber = 1,
	NoMinorCommandNumber = 2,
	FilePositionValid = 4,
	EnforceAbsolutePosition = 8
};

struct CodeHeader
{
	CodeChannel channel;
	CodeFlags flags;
	uint8_t numParameters;
	char letter;
	int32_t majorCode;
	int32_t minorCode;
	uint32_t filePosition;
};

struct CodeParameter
{
	char letter;
	DataType type;
	uint16_t padding;
	union
	{
		int32_t intValue;
		uint32_t uintValue;
		float floatValue;
	};
};

struct MacroCompleteHeader
{
	CodeChannel channel;
	bool error;
	uint16_t padding;
};

struct PrintStartedHeader
{
	uint16_t filenameLength;
	uint16_t generatedByLength;
	uint32_t numFilaments;
	time_t lastModifiedTime;
	uint32_t fileSize;
	float firstLayerHeight;
	float layerHeight;
	float objectHeight;
	uint32_t printTime;
	uint32_t simulatedTime;
};

// Keep this in sync with StopPrintReason in GCodes/GCodes.h
enum class PrintStoppedReason : uint8_t
{
    normalCompletion = 0,
    userCancelled = 1,
    abort = 2
};

struct PrintStoppedHeader
{
	PrintStoppedReason reason;
	uint8_t paddingA;
	uint16_t paddingB;
};

struct SetObjectModelHeader
{
	DataType type;
	uint8_t fieldLength;
	union
	{
		int32_t intValue;
		uint32_t uintValue;
		float floatValue;
	};
};

#endif /* SRC_LINUX_MESSAGEFORMATS_H_ */
