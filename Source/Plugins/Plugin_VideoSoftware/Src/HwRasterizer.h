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

#ifndef _HW_RASTERIZER_H
#define _HW_RASTERIZER_H

#include <map>

#include "BPMemLoader.h"
#include "../../Plugin_VideoOGL/Src/GLUtil.h"

struct OutputVertexData;

namespace HwRasterizer
{
    void Init();
	void Shutdown();
		
	void Prepare();

    void BeginTriangles();
    void EndTriangles();

    void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2);

    void Clear();

    struct TexCacheEntry
    {
        TexImage0 texImage0; 
        TexImage1 texImage1; 
        TexImage2 texImage2; 
        TexImage3 texImage3; 
        TexTLUT texTlut;

        GLuint texture;

        TexCacheEntry();

        void Create();
        void Destroy();
        void Update();
    };

    typedef std::map<u32, TexCacheEntry> TextureCache;
    static TextureCache textures;
}

#endif 
