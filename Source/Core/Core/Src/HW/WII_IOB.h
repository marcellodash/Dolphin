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
#ifndef _WII_IOBRIDGE_H_
#define _WII_IOBRIDGE_H_

#include "Common.h"
class PointerWrap;

namespace WII_IOBridge
{

void Init();
void Shutdown();	
void DoState(PointerWrap &p);

void Update();

void Read8(u8& _rReturnValue, const u32 _Address);
void Read16(u16& _rReturnValue, const u32 _Address);
void Read32(u32& _rReturnValue, const u32 _Address);
void Read64(u64& _rReturnValue, const u32 _Address);

void Write8(const u8 _Value, const u32 _Address);
void Write16(const u16 _Value, const u32 _Address);
void Write32(const u32 _Value, const u32 _Address);
void Write64(const u64 _Value, const u32 _Address);

} // end of namespace AudioInterface

#endif


