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

#ifndef _XAUDIO2STREAM_H_
#define _XAUDIO2STREAM_H_

#include "SoundStream.h"

#ifdef _WIN32
#include "Thread.h"
#include <xaudio2.h>
#include <memory>

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
private:
	CMixer* const m_mixer;
	Common::Event& m_sound_sync_event;
	IXAudio2SourceVoice* m_source_voice;
	std::unique_ptr<BYTE[]> xaudio_buffer;

	void SubmitBuffer(PBYTE buf_data);

public:
	StreamingVoiceContext(IXAudio2 *pXAudio2, CMixer *pMixer, Common::Event& pSyncEvent);
	
	~StreamingVoiceContext();
	
	void StreamingVoiceContext::Stop();
	void StreamingVoiceContext::Play();
	
	STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) {}
	STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
	STDMETHOD_(void, OnBufferStart) (void*) {}
	STDMETHOD_(void, OnLoopEnd) (void*) {}   
	STDMETHOD_(void, OnStreamEnd) () {}

	STDMETHOD_(void, OnBufferEnd) (void* context);
};

#endif

class XAudio2 : public SoundStream
{
#ifdef _WIN32

	class Releaser
	{
	public:
		template <typename R>
		void operator()(R* ptr)
		{
			ptr->Release();
		}
	};

private:
	std::unique_ptr<IXAudio2, Releaser> m_xaudio2;
	std::unique_ptr<StreamingVoiceContext> m_voice_context;
	IXAudio2MasteringVoice *m_mastering_voice;

	Common::Event m_sound_sync_event;
	float m_volume;

	const bool m_cleanup_com;

public:
	XAudio2(CMixer *mixer) 
		: SoundStream(mixer)
		, m_mastering_voice(nullptr)
		, m_volume(1.0f)
		, m_cleanup_com(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
	{}

	virtual ~XAudio2()
	{
		Stop();
		if (m_cleanup_com)
			CoUninitialize();
	}
 
	virtual bool Start();
	virtual void Stop();

	virtual void Update();
	virtual void Clear(bool mute);
	virtual void SetVolume(int volume);
	virtual bool usesMixer() const { return true; }

	static bool isValid() { return true; }

#else

public:
	XAudio2(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
	{}
#endif
};

#endif //_XAUDIO2STREAM_H_
