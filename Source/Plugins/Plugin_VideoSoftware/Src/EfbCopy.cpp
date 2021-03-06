// Copyright (C) 2003-2009 Dolphin Project.

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

#include "BPMemLoader.h"
#include "EfbCopy.h"
#include "EfbInterface.h"
#include "SWRenderer.h"
#include "TextureEncoder.h"
#include "SWStatistics.h"
#include "SWVideoConfig.h"
#include "DebugUtil.h"
#include "HwRasterizer.h"
#include "SWCommandProcessor.h"
#include "HW/Memmap.h"
#include "Core.h"

namespace EfbCopy
{
    void CopyToXfb()
    {
        GLInterface->Update(); // just updates the render window position and the backbuffer size	

		if (!g_SWVideoConfig.bHwRasterizer)
        {
            // copy to open gl for rendering
            EfbInterface::UpdateColorTexture();
            SWRenderer::DrawTexture(EfbInterface::efbColorTexture, EFB_WIDTH, EFB_HEIGHT);
        }

        SWRenderer::SwapBuffer();

    }

    void CopyToRam()
    {
        if (!g_SWVideoConfig.bHwRasterizer)
		{
			u8 *dest_ptr = Memory::GetPointer(bpmem.copyTexDest << 5);

			TextureEncoder::Encode(dest_ptr);
		}
    }

    void ClearEfb()
    {
        u32 clearColor = (bpmem.clearcolorAR & 0xff) << 24 | bpmem.clearcolorGB << 8 | (bpmem.clearcolorAR & 0xff00) >> 8;

        int left   = bpmem.copyTexSrcXY.x;
	    int top    = bpmem.copyTexSrcXY.y;
        int right  = left + bpmem.copyTexSrcWH.x;
	    int bottom = top + bpmem.copyTexSrcWH.y;

        for (u16 y = top; y <= bottom; y++)
        {
            for (u16 x = left; x <= right; x++)
            {
                EfbInterface::SetColor(x, y, (u8*)(&clearColor));
                EfbInterface::SetDepth(x, y, bpmem.clearZValue);
            }
        }
    }

    void CopyEfb()
    {
        if (bpmem.triggerEFBCopy.copy_to_xfb)
            DebugUtil::OnFrameEnd();

        if (!g_bSkipCurrentFrame)
        {
            if (bpmem.triggerEFBCopy.copy_to_xfb)
            {
                CopyToXfb();
                Core::Callback_VideoCopiedToXFB(true);

                swstats.frameCount++;
            }
            else
            {
                CopyToRam();
            }

            if (bpmem.triggerEFBCopy.clear)
            {
                if (g_SWVideoConfig.bHwRasterizer)
                    HwRasterizer::Clear();
                else
                    ClearEfb();
            }
        }
        else
        {
            if (bpmem.triggerEFBCopy.copy_to_xfb)
            {
                // no frame rendered but tell that a frame has finished for frame skip counter
                Core::Callback_VideoCopiedToXFB(false);
            }
        }
    }    
}
