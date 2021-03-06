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

#include "Common.h"

#include "DSPCore.h"
#include "DSPHost.h"
#include "DSPHWInterface.h"
#include "DSPInterpreter.h"

// The hardware adpcm decoder :)
static s16 ADPCM_Step(u32& _rSamplePos)
{
	const s16 *pCoefTable = (const s16 *)&g_dsp.ifx_regs[DSP_COEF_A1_0];

	if (((_rSamplePos) & 15) == 0)
	{
		g_dsp.ifx_regs[DSP_PRED_SCALE] = DSPHost_ReadHostMemory((_rSamplePos & ~15) >> 1);
		_rSamplePos += 2;
	}

	int scale = 1 << (g_dsp.ifx_regs[DSP_PRED_SCALE] & 0xF);
	int coef_idx = (g_dsp.ifx_regs[DSP_PRED_SCALE] >> 4) & 0x7;

	s32 coef1 = pCoefTable[coef_idx * 2 + 0];
	s32 coef2 = pCoefTable[coef_idx * 2 + 1];

	int temp = (_rSamplePos & 1) ?
		   (DSPHost_ReadHostMemory(_rSamplePos >> 1) & 0xF) :
		   (DSPHost_ReadHostMemory(_rSamplePos >> 1) >> 4);

	if (temp >= 8)
		temp -= 16;

	// 0x400 = 0.5  in 11-bit fixed point
	int val = (scale * temp) + ((0x400 + coef1 * (s16)g_dsp.ifx_regs[DSP_YN1] + coef2 * (s16)g_dsp.ifx_regs[DSP_YN2]) >> 11);

	// Clamp values.
	if (val > 0x7FFF)
		val = 0x7FFF;
	else if (val < -0x7FFF)
		val = -0x7FFF;

	g_dsp.ifx_regs[DSP_YN2] = g_dsp.ifx_regs[DSP_YN1];
	g_dsp.ifx_regs[DSP_YN1] = val;

	_rSamplePos++;

    // The advanced interpolation (linear, polyphase,...) is done by the UCode,
	// so we don't need to bother with it here.
	return val;
}

u16 dsp_read_aram_d3()
{
	// Zelda ucode reads ARAM through 0xffd3.
	const u32 EndAddress = (g_dsp.ifx_regs[DSP_ACEAH] << 16) | g_dsp.ifx_regs[DSP_ACEAL];
	u32 Address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];
	u16 val = 0;

	switch (g_dsp.ifx_regs[DSP_FORMAT]) {
		case 0x5:   // u8 reads
			val = DSPHost_ReadHostMemory(Address);
			Address++;
			break;
		case 0x6:   // u16 reads
		    val = (DSPHost_ReadHostMemory(Address*2) << 8) | DSPHost_ReadHostMemory(Address*2 + 1);
			Address++;
			break;
		default:
			ERROR_LOG(DSPLLE, "dsp_read_aram_d3() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
			break;
	}

	if (Address >= EndAddress) 
	{
		// Set address back to start address. (never seen this here!)
		Address = (g_dsp.ifx_regs[DSP_ACSAH] << 16) | g_dsp.ifx_regs[DSP_ACSAL];
	}

	g_dsp.ifx_regs[DSP_ACCAH] = Address >> 16;
	g_dsp.ifx_regs[DSP_ACCAL] = Address & 0xffff;
	return val;
}

void dsp_write_aram_d3(u16 value)
{
	// Zelda ucode writes a bunch of zeros to ARAM through d3 during
	// initialization.  Don't know if it ever does it later, too.
	// Pikmin 2 Wii writes non-stop to 0x10008000-0x1000801f (non-zero values too)
	// Zelda TP WII writes non-stop to 0x10000000-0x1000001f (non-zero values too)
	u32 Address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];

	switch (g_dsp.ifx_regs[DSP_FORMAT]) {
		case 0xA:   // u16 writes
			DSPHost_WriteHostMemory(value >> 8, Address*2);
			DSPHost_WriteHostMemory(value & 0xFF, Address*2 + 1);
			Address++;
			break;
		default:
			ERROR_LOG(DSPLLE, "dsp_write_aram_d3() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
			break;
	}

	g_dsp.ifx_regs[DSP_ACCAH] = Address >> 16;
	g_dsp.ifx_regs[DSP_ACCAL] = Address & 0xffff;
}

u16 dsp_read_accelerator()
{
	const u32 EndAddress = (g_dsp.ifx_regs[DSP_ACEAH] << 16) | g_dsp.ifx_regs[DSP_ACEAL];
	u32 Address = (g_dsp.ifx_regs[DSP_ACCAH] << 16) | g_dsp.ifx_regs[DSP_ACCAL];
	u16 val;

	// let's do the "hardware" decode DSP_FORMAT is interesting - the Zelda
	// ucode seems to indicate that the bottom two bits specify the "read size"
	// and the address multiplier.  The bits above that may be things like sign
	// extention and do/do not use ADPCM.  It also remains to be figured out
	// whether there's a difference between the usual accelerator "read
	// address" and 0xd3.
	switch (g_dsp.ifx_regs[DSP_FORMAT])
	{
	    case 0x00:  // ADPCM audio
		    val = ADPCM_Step(Address);
		    break;
	    case 0x0A:  // 16-bit PCM audio
		    val = (DSPHost_ReadHostMemory(Address*2) << 8) | DSPHost_ReadHostMemory(Address*2 + 1);
		    g_dsp.ifx_regs[DSP_YN2] = g_dsp.ifx_regs[DSP_YN1];
		    g_dsp.ifx_regs[DSP_YN1] = val;
		    Address++;
		    break;
	    case 0x19:  // 8-bit PCM audio
		    val = DSPHost_ReadHostMemory(Address) << 8; 
		    g_dsp.ifx_regs[DSP_YN2] = g_dsp.ifx_regs[DSP_YN1];
		    g_dsp.ifx_regs[DSP_YN1] = val;
		    Address++;
			break;
	    default:
		    ERROR_LOG(DSPLLE, "dsp_read_accelerator() - unknown format 0x%x", g_dsp.ifx_regs[DSP_FORMAT]);
		    Address++;
		    val = 0;
			break;
	}

	// TODO: Take GAIN into account
	// adpcm = 0, pcm8 = 0x100, pcm16 = 0x800
	// games using pcm8 : Phoenix Wright Ace Attorney (Wiiware), Megaman 9-10 (WiiWare)
	// games using pcm16: gc sega games, ...

	// Check for loop.
	// Somehow, YN1 and YN2 must be initialized with their "loop" values,
	// so yeah, it seems likely that we should raise an exception to let
	// the DSP program do that, at least if DSP_FORMAT == 0x0A.
	if (Address >= EndAddress)
	{
		// Set address back to start address.
		if ((Address & ~0x1f) == (EndAddress & ~0x1f))
			Address = (g_dsp.ifx_regs[DSP_ACSAH] << 16) | g_dsp.ifx_regs[DSP_ACSAL];
		DSPCore_SetException(EXP_ACCOV);
	}

	g_dsp.ifx_regs[DSP_ACCAH] = Address >> 16;
	g_dsp.ifx_regs[DSP_ACCAL] = Address & 0xffff;
	return val;
}
