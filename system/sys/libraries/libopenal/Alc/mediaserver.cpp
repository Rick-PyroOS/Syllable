#include <atheos/types.h>
#include <atheos/kernel.h>

#include <servers/mediaserver/manager.h>
#include <servers/mediaserver/input.h>
#include <servers/mediaserver/output.h>

#include <posix/stat.h>

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"

using namespace os;

static char *media_device;
static char *media_device_capture;

class MediaDriver
{
public:
		MediaDriver();
virtual	~MediaDriver();

static	ALCboolean 	OpenPlayback(ALCdevice*, const ALCchar*);
static	void		ClosePlayback(ALCdevice*);
static	ALCboolean 	OpenCapture(ALCdevice*, const ALCchar*, ALCuint, ALCenum, ALCsizei);
static	void		CloseCapture(ALCdevice*);
static	void		StartCapture(ALCdevice*);
static	void		StopCapture(ALCdevice*);
static	void		CaptureSamples(ALCdevice*, void*, ALCuint);
static	ALCuint		CountAvailableSamples(ALCdevice*);
	
	// working threads
	ALuint	MediaProc(ALvoid *ptr);
	ALuint	MediaCaptureProc(ALvoid *ptr);
	
private:
	MediaManager	*m_pcManager;
	MediaInput		*m_pcAudioInput;
	MediaFormat_s	m_sAudioInputFormat;
	MediaOutput		*m_pcAudioOutput;
	MediaFormat_s	m_sAudioOutputFormat;
	bool			m_bPlayThread;
	thread_id		m_hWorkThread;
	uint64			m_nLastPosition;
};

MediaDriver::MediaDriver()
{
	CALLED();
}

MediaDriver::~MediaDriver()
{
	CALLED();
}

ALCboolean MediaDriver::OpenPlayback(ALCdevice*, const ALCchar*)
{
	CALLED();
	return false;
}

void MediaDriver::ClosePlayback(ALCdevice*)
{
	CALLED();
}

ALCboolean MediaDriver::OpenCapture(ALCdevice*, const ALCchar*, ALCuint, ALCenum, ALCsizei)
{
	CALLED();
	return false;
}

void MediaDriver::CloseCapture(ALCdevice*)
{
	CALLED();
}

void MediaDriver::StartCapture(ALCdevice*)
{
	CALLED();
}

void MediaDriver::StopCapture(ALCdevice*)
{
	CALLED();
}

void MediaDriver::CaptureSamples(ALCdevice*, void*, ALCuint)
{
	CALLED();
}

ALCuint MediaDriver::CountAvailableSamples(ALCdevice*)
{
	CALLED();
	return 0;
}

ALuint MediaDriver::MediaProc(ALvoid *ptr)
{
	CALLED();
	return 0;
}

ALuint MediaDriver::MediaCaptureProc(ALvoid *ptr)
{
	CALLED();
	return 0;
}

BackendFuncs media_funcs = {
    MediaDriver::OpenPlayback,
	MediaDriver::ClosePlayback,
	MediaDriver::OpenCapture,
	MediaDriver::CloseCapture,
	MediaDriver::StartCapture,
	MediaDriver::StopCapture,
	MediaDriver::CaptureSamples,
	MediaDriver::CountAvailableSamples
};

void alc_media_init(BackendFuncs *func_list)
{
    *func_list = media_funcs;

    media_device = AppendDeviceList("Media Server Software");
    AppendAllDeviceList(media_device);

    media_device_capture = AppendCaptureDeviceList("Media Server Capture");
}

