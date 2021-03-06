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
#include "State.h"
#include "Core.h"
#include "ConfigManager.h"
#include "StringUtil.h"
#include "Thread.h"
#include "CoreTiming.h"
#include "Movie.h"
#include "HW/Wiimote.h"
#include "HW/DSP.h"
#include "HW/HW.h"
#include "HW/CPU.h"
#include "PowerPC/JitCommon/JitBase.h"
#include "VideoBackendBase.h"

#include <lzo/lzo1x.h>
#include "HW/Memmap.h"
#include "HW/VideoInterface.h"
#include "HW/SystemTimers.h"

namespace State
{

#if defined(__LZO_STRICT_16BIT)
static const u32 IN_LEN = 8 * 1024u;
#elif defined(LZO_ARCH_I086) && !defined(LZO_HAVE_MM_HUGE_ARRAY)
static const u32 IN_LEN = 60 * 1024u;
#else
static const u32 IN_LEN = 128 * 1024u;
#endif

static const u32 OUT_LEN = IN_LEN + (IN_LEN / 16) + 64 + 3;

static unsigned char __LZO_MMODEL out[OUT_LEN];

#define HEAP_ALLOC(var, size) \
	lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

static std::string g_last_filename;

static CallbackFunc g_onAfterLoadCb = NULL;

// Temporary undo state buffer
static std::vector<u8> g_undo_load_buffer;
static std::vector<u8> g_current_buffer;
static int g_loadDepth = 0;

static std::mutex g_cs_undo_load_buffer;
static std::mutex g_cs_current_buffer;
static Common::Event g_compressAndDumpStateSyncEvent;

static std::thread g_save_thread;

// Don't forget to increase this after doing changes on the savestate system 
static const u32 STATE_VERSION = 10;

struct StateHeader
{
	u8 gameID[6];
	size_t size;
};

enum
{
	STATE_NONE = 0,
	STATE_SAVE = 1,
	STATE_LOAD = 2,
};

static bool g_use_compression = true;

void EnableCompression(bool compression)
{
	g_use_compression = compression;
}

void DoState(PointerWrap &p)
{
	u32 version = STATE_VERSION;
	{
 		static const u32 COOKIE_BASE = 0xBAADBABE;
		u32 cookie = version + COOKIE_BASE;
		p.Do(cookie);
		version = cookie - COOKIE_BASE;
	}

	if (version != STATE_VERSION)
	{
		// if the version doesn't match, fail.
		// this will trigger a message like "Can't load state from other revisions"
		// we could use the version numbers to maintain some level of backward compatibility, but currently don't.
		p.SetMode(PointerWrap::MODE_MEASURE);
		return;
	}

	p.DoMarker("Version");

	// Begin with video backend, so that it gets a chance to clear it's caches and writeback modified things to RAM
	g_video_backend->DoState(p);
	p.DoMarker("video_backend");

	if (Core::g_CoreStartupParameter.bWii)
		Wiimote::DoState(p.GetPPtr(), p.GetMode());
	p.DoMarker("Wiimote");

	PowerPC::DoState(p);
	p.DoMarker("PowerPC");
	HW::DoState(p);
	p.DoMarker("HW");
	CoreTiming::DoState(p);
	p.DoMarker("CoreTiming");
	Movie::DoState(p);
	p.DoMarker("Movie");
}

void LoadFromBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = &buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_READ);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

void SaveToBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = NULL;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);

	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);
	buffer.resize(buffer_size);
	
	ptr = &buffer[0];
	p.SetMode(PointerWrap::MODE_WRITE);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

void VerifyBuffer(std::vector<u8>& buffer)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	u8* ptr = &buffer[0];
	PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
	DoState(p);

	Core::PauseAndLock(false, wasUnpaused);
}

struct CompressAndDumpState_args
{
	std::vector<u8>* buffer_vector;
	std::mutex* buffer_mutex;
	std::string filename;
};

void CompressAndDumpState(CompressAndDumpState_args save_args)
{
	std::lock_guard<std::mutex> lk(*save_args.buffer_mutex);
	g_compressAndDumpStateSyncEvent.Set();

	const u8* const buffer_data = &(*(save_args.buffer_vector))[0];
	const size_t buffer_size = (save_args.buffer_vector)->size();
	std::string& filename = save_args.filename;

	// For easy debugging
	Common::SetCurrentThreadName("SaveState thread");

	// Moving to last overwritten save-state
	if (File::Exists(filename))
	{
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			File::Delete((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"));
		if (File::Exists(File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm"))
			File::Delete((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm"));

		if (!File::Rename(filename, File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav"))
			Core::DisplayMessage("Failed to move previous state to state undo backup", 1000);
		else 
			File::Rename(filename + ".dtm", File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav.dtm");
	}
	if ((Movie::IsRecordingInput() || Movie::IsPlayingInput()) && !Movie::IsJustStartingRecordingInputFromSaveState())
		Movie::SaveRecording((filename + ".dtm").c_str());
	else if (!Movie::IsRecordingInput() && !Movie::IsPlayingInput())
		File::Delete(filename + ".dtm");

	File::IOFile f(filename, "wb");
	if (!f)
	{
		Core::DisplayMessage("Could not save state", 2000);
		return;
	}

	// Setting up the header
	StateHeader header;
	memcpy(header.gameID, SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), 6);
	header.size = g_use_compression ? buffer_size : 0;

	f.WriteArray(&header, 1);

	if (0 != header.size)	// non-zero header size means the state is compressed
	{
		lzo_uint i = 0;
		while (true)
		{
			lzo_uint cur_len = 0;
			lzo_uint out_len = 0;

			if ((i + IN_LEN) >= buffer_size)
				cur_len = buffer_size - i;
			else
				cur_len = IN_LEN;

			if (lzo1x_1_compress(buffer_data + i, cur_len, out, &out_len, wrkmem) != LZO_E_OK)
				PanicAlertT("Internal LZO Error - compression failed");

			// The size of the data to write is 'out_len'
			f.WriteArray(&out_len, 1);
			f.WriteBytes(out, out_len);

			if (cur_len != IN_LEN)
				break;

			i += cur_len;
		}
	}
	else	// uncompressed
	{
		f.WriteBytes(buffer_data, buffer_size);
	}

	Core::DisplayMessage(StringFromFormat("Saved State to %s",
		filename.c_str()).c_str(), 2000);
}

void SaveAs(const std::string& filename)
{
	// Pause the core while we save the state
	bool wasUnpaused = Core::PauseAndLock(true);

	// Measure the size of the buffer.
	u8 *ptr = NULL;
	PointerWrap p(&ptr, PointerWrap::MODE_MEASURE);
	DoState(p);
	const size_t buffer_size = reinterpret_cast<size_t>(ptr);

	// Then actually do the write.
	{
		std::lock_guard<std::mutex> lk(g_cs_current_buffer);
		g_current_buffer.resize(buffer_size);
		ptr = &g_current_buffer[0];
		p.SetMode(PointerWrap::MODE_WRITE);
		DoState(p);
	}

	if (p.GetMode() == PointerWrap::MODE_WRITE)
	{
		Core::DisplayMessage("Saving State...", 1000);

		CompressAndDumpState_args save_args;
		save_args.buffer_vector = &g_current_buffer;
		save_args.buffer_mutex = &g_cs_current_buffer;
		save_args.filename = filename;

		Flush();
		g_save_thread = std::thread(CompressAndDumpState, save_args);
		g_compressAndDumpStateSyncEvent.Wait();

		g_last_filename = filename;
	}
	else
	{
		// someone aborted the save by changing the mode?
		Core::DisplayMessage("Unable to Save : Internal DoState Error", 4000);
	}

	// Resume the core and disable stepping
	Core::PauseAndLock(false, wasUnpaused);
}

void LoadFileStateData(const std::string& filename, std::vector<u8>& ret_data)
{
	Flush();
	File::IOFile f(filename, "rb");
	if (!f)
	{
		Core::DisplayMessage("State not found", 2000);
		return;
	}

	StateHeader header;
	f.ReadArray(&header, 1);
	
	if (memcmp(SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), header.gameID, 6)) 
	{
		Core::DisplayMessage(StringFromFormat("State belongs to a different game (ID %.*s)",
			6, header.gameID), 2000);
		return;
	}

	std::vector<u8> buffer;

	if (0 != header.size)	// non-zero size means the state is compressed
	{
		Core::DisplayMessage("Decompressing State...", 500);

		buffer.resize(header.size);

		lzo_uint i = 0;
		while (true)
		{
			lzo_uint cur_len = 0;  // number of bytes to read
			lzo_uint new_len = 0;  // number of bytes to write

			if (!f.ReadArray(&cur_len, 1))
				break;

			f.ReadBytes(out, cur_len);
			const int res = lzo1x_decompress(out, cur_len, &buffer[i], &new_len, NULL);
			if (res != LZO_E_OK)
			{
				// This doesn't seem to happen anymore.
				PanicAlertT("Internal LZO Error - decompression failed (%d) (%li, %li) \n"
					"Try loading the state again", res, i, new_len);
				return;
			}
	
			i += new_len;
		}
	}
	else	// uncompressed
	{
		const size_t size = (size_t)(f.GetSize() - sizeof(StateHeader));
		buffer.resize(size);

		if (!f.ReadBytes(&buffer[0], size))
		{
			PanicAlert("wtf? reading bytes: %i", (int)size);
			return;
		}
	}

	// all good
	ret_data.swap(buffer);
}

void LoadAs(const std::string& filename)
{
	// Stop the core while we load the state
	bool wasUnpaused = Core::PauseAndLock(true);

#if defined _DEBUG && defined _WIN32
	// we use _CRTDBG_DELAY_FREE_MEM_DF (as a speed hack?),
	// but it was causing us to leak gigantic amounts of memory here,
	// enough that only a few savestates could be loaded before crashing,
	// so let's disable it temporarily.
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	if (g_loadDepth == 0)
		_CrtSetDbgFlag(tmpflag & ~_CRTDBG_DELAY_FREE_MEM_DF);
#endif

	g_loadDepth++;

	// Save temp buffer for undo load state
	if (!Movie::IsJustStartingRecordingInputFromSaveState())
	{
		std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
		SaveToBuffer(g_undo_load_buffer);
		if (Movie::IsRecordingInput() || Movie::IsPlayingInput())
			Movie::SaveRecording("undo.dtm");
		else if (File::Exists("undo.dtm"))
			File::Delete("undo.dtm");
	}

	bool loaded = false;
	bool loadedSuccessfully = false;

	// brackets here are so buffer gets freed ASAP
	{
		std::vector<u8> buffer;
		LoadFileStateData(filename, buffer);

		if (!buffer.empty())
		{
			u8 *ptr = &buffer[0];
			PointerWrap p(&ptr, PointerWrap::MODE_READ);
			DoState(p);
			loaded = true;
			loadedSuccessfully = (p.GetMode() == PointerWrap::MODE_READ);
		}
	}

	if (loaded)
	{
		if (loadedSuccessfully)
		{
			Core::DisplayMessage(StringFromFormat("Loaded state from %s", filename.c_str()).c_str(), 2000);
			if (File::Exists(filename + ".dtm"))
				Movie::LoadInput((filename + ".dtm").c_str());
			else if (!Movie::IsJustStartingRecordingInputFromSaveState() && !Movie::IsJustStartingPlayingInputFromSaveState())
				Movie::EndPlayInput(false);
		}
		else
		{
			// failed to load
			Core::DisplayMessage("Unable to Load : Can't load state from other revisions !", 4000);

			// since we could be in an inconsistent state now (and might crash or whatever), undo.
			if (g_loadDepth < 2)
				UndoLoadState();
		}
	}

	if (g_onAfterLoadCb)
		g_onAfterLoadCb();

	g_loadDepth--;

#if defined _DEBUG && defined _WIN32
	// restore _CRTDBG_DELAY_FREE_MEM_DF
	if (g_loadDepth == 0)
		_CrtSetDbgFlag(tmpflag);
#endif

	// resume dat core
	Core::PauseAndLock(false, wasUnpaused);
}

void SetOnAfterLoadCallback(CallbackFunc callback)
{
	g_onAfterLoadCb = callback;
}

void VerifyAt(const std::string& filename)
{
	bool wasUnpaused = Core::PauseAndLock(true);

	std::vector<u8> buffer;
	LoadFileStateData(filename, buffer);

	if (!buffer.empty())
	{
		u8 *ptr = &buffer[0];
		PointerWrap p(&ptr, PointerWrap::MODE_VERIFY);
		DoState(p);

		if (p.GetMode() == PointerWrap::MODE_VERIFY)
			Core::DisplayMessage(StringFromFormat("Verified state at %s", filename.c_str()).c_str(), 2000);
		else
			Core::DisplayMessage("Unable to Verify : Can't verify state from other revisions !", 4000);
	}

	Core::PauseAndLock(false, wasUnpaused);
}


void Init()
{
	if (lzo_init() != LZO_E_OK)
		PanicAlertT("Internal LZO Error - lzo_init() failed");
}

void Shutdown()
{
	Flush();

	// swapping with an empty vector, rather than clear()ing
	// this gives a better guarantee to free the allocated memory right NOW (as opposed to, actually, never)
	{
		std::lock_guard<std::mutex> lk(g_cs_current_buffer);
		std::vector<u8>().swap(g_current_buffer);
	}

	{
		std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
		std::vector<u8>().swap(g_undo_load_buffer);
	}
}

static std::string MakeStateFilename(int number)
{
	return StringFromFormat("%s%s.s%02i", File::GetUserPath(D_STATESAVES_IDX).c_str(),
		SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), number);
}

void Save(int slot)
{
	SaveAs(MakeStateFilename(slot));
}

void Load(int slot)
{
	LoadAs(MakeStateFilename(slot));
}

void Verify(int slot)
{
	VerifyAt(MakeStateFilename(slot));
}

void LoadLastSaved()
{
	if (g_last_filename.empty())
		Core::DisplayMessage("There is no last saved state", 2000);
	else
		LoadAs(g_last_filename);
}

void Flush()
{
	// If already saving state, wait for it to finish
	if (g_save_thread.joinable())
		g_save_thread.join();
}

// Load the last state before loading the state
void UndoLoadState()
{
	std::lock_guard<std::mutex> lk(g_cs_undo_load_buffer);
	if (!g_undo_load_buffer.empty())
	{
		if (File::Exists("undo.dtm") || (!Movie::IsRecordingInput() && !Movie::IsPlayingInput()))
		{
			LoadFromBuffer(g_undo_load_buffer);
			if (Movie::IsRecordingInput() || Movie::IsPlayingInput())
				Movie::LoadInput("undo.dtm");
		}
		else
			PanicAlert("No undo.dtm found, aborting undo load state to prevent movie desyncs");
	}
	else
		PanicAlert("There is nothing to undo!");
}

// Load the state that the last save state overwritten on
void UndoSaveState()
{
		LoadAs((File::GetUserPath(D_STATESAVES_IDX) + "lastState.sav").c_str());
}

} // namespace State
