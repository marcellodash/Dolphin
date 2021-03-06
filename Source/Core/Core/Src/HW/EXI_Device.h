// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _EXIDEVICE_H
#define _EXIDEVICE_H

#include "Common.h"
#include "ChunkFile.h"

enum TEXIDevices
{
	EXIDEVICE_DUMMY,
	EXIDEVICE_MEMORYCARD,
	EXIDEVICE_MASKROM,
	EXIDEVICE_AD16,
	EXIDEVICE_MIC,
	EXIDEVICE_ETH,
	EXIDEVICE_AM_BASEBOARD,
	EXIDEVICE_GECKO,
	EXIDEVICE_NONE = (u8)-1
};

class IEXIDevice
{
private:
	// Byte transfer function for this device
	virtual void TransferByte(u8&) {}

public:
	// Immediate copy functions
	virtual void ImmWrite(u32 _uData,  u32 _uSize);
	virtual u32  ImmRead(u32 _uSize);
	virtual void ImmReadWrite(u32 &/*_uData*/, u32 /*_uSize*/) {}

	// DMA copy functions
	virtual void DMAWrite(u32 _uAddr, u32 _uSize);
	virtual void DMARead (u32 _uAddr, u32 _uSize);

	virtual bool IsPresent() {return false;}
	virtual void SetCS(int) {}
	virtual void DoState(PointerWrap&) {}
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) {}
	virtual IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1) { return (device_type == m_deviceType) ? this : NULL; }

	// Update
	virtual void Update() {}

	// Is generating interrupt ?
	virtual bool IsInterruptSet() {return false;}
	virtual ~IEXIDevice() {}

	// for savestates. storing it here seemed cleaner than requiring each implementation to report its type.
	// I know this class is set up like an interface, but no code requires it to be strictly such.
	TEXIDevices m_deviceType;
};

extern IEXIDevice* EXIDevice_Create(const TEXIDevices device_type, const int channel_num);

#endif
