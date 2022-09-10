#include <windows.h>
#include <mmsystem.h>
#include <winreg.h>

#include <conio.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <math.h>

/* AUDIO LIBRARY INCLUDES START */

#include <bass/bass.h>

/* AUDIO LIBRARY INCLUDES END */

#define MAGIC_DEVICEID 0xBEEF
#define MAX_TRACKS 99

/* PROJECT LIBRARIES START */

/* PROJECT LIBRARIES END */

CRITICAL_SECTION cs;

#define dprintf(...) if (fh) { fprintf(fh, __VA_ARGS__); fflush(NULL); }
FILE *fh = NULL;

char musdll_path[2048];

/* CONFIG FILE DEFINES START */

int AudioLibrary;
int FileFormat;
int PlaybackMode;
char MusicFolder[255];
char strFileFormat[5];
TCHAR MusicFolderFullPath[MAX_PATH];
char strMusicFile[32];
TCHAR MusicFileFullPath[MAX_PATH];
TCHAR MusicFileStoredPath[MAX_PATH];
HANDLE findTracks = INVALID_HANDLE_VALUE;
WIN32_FIND_DATA MusicFiles;

/* defines for storing music data */

struct track_info
{
    char path[MAX_PATH];    /* full path to track */
};

static struct track_info tracks[MAX_TRACKS];

struct track_info *info;

int numTracks = 0;
int firstTrack = -1;
int lastTrack = 0;
int currentTrack = -1;
int nextTrack = 1;

/* CONFIG FILE DEFINES END */

/* WAVEOUT DEFINES START */

#define CHUNK_SIZE 2000
#define TWOPI (M_PI + M_PI)

WAVEFORMATEX plr_fmt;
HWAVEOUT plr_hwo = NULL;
int	plr_cnt	= 0;
int	plr_vol	= 100;
WAVEHDR header[2] = {0};
int16_t chunks[2][CHUNK_SIZE];
bool chunk_swap = false;
float frequency = 400;
float wave_position = 0;
float wave_step;

/* WAVEOUT DEFINES END */

/* BASS PLAYER DEFINES START */
HWND win;

HSTREAM *strs;
int strc;
HMUSIC *mods;
int modc;
HSAMPLE *sams;
int samc;

HSTREAM str;
BASS_CHANNELINFO cinfo;

/* BASS PLAYER DEFINES END */

/* AUDIO PLAYBACK DEFINES START */

int opened = 0;
int paused = 1;
int stopped = 1;
int closed = 1;
int playing = 0;
int playeractive = 0;
int timeFormat = MCI_FORMAT_MILLISECONDS;

/* AUDIO PLAYBACK DEFINES END */

 
int sortstring(const void* a, const void* b)
{
    const char *ia = (const char *)a;
    const char *ib = (const char *)b;
    return strcmp(ia, ib);
}

BOOL FileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
 
/* Get audio settings from <exe dir>/wgmmus.ini, set in global variables */
void mmusi_config()
{

	TCHAR ConfigFileNameFullPath[MAX_PATH];
	char *last = strrchr(musdll_path, '\\');
	if (last)
	{
		*last = '\0';
	}
	strncat(musdll_path, "\\", sizeof musdll_path - 1);
	strcpy(ConfigFileNameFullPath, musdll_path);
	LPCSTR ConfigFileName = "wgmus.ini";

	*(strrchr(ConfigFileNameFullPath, '\\')+1)=0;
	strcat(ConfigFileNameFullPath,ConfigFileName);
	
	if(FileExists(ConfigFileNameFullPath)) { dprintf("Reading audio settings from: %s\r\n", ConfigFileNameFullPath); }
	else { dprintf("Audio settings file %s does not exist.\r\n", ConfigFileNameFullPath); }
	
	AudioLibrary = GetPrivateProfileInt("Settings", "AudioLibrary", 0, ConfigFileNameFullPath);
	FileFormat = GetPrivateProfileInt("Settings", "FileFormat", 0, ConfigFileNameFullPath);
	PlaybackMode = GetPrivateProfileInt("Settings", "PlaybackMode", 0, ConfigFileNameFullPath);
	GetPrivateProfileString("Settings", "MusicFolder", "tamus", MusicFolder, MAX_PATH, ConfigFileNameFullPath);
	dprintf("	AudioLibrary = %d\r\n", AudioLibrary);
	dprintf("	FileFormat = %d\r\n", FileFormat);
	dprintf("	PlaybackMode = %d\r\n", PlaybackMode);
	dprintf("	MusicFolder = %s\r\n", MusicFolder);
	
	if(FileFormat == 0)
	{
		strcpy(strFileFormat, ".wav");
		dprintf("	File Format is wav\r\n");
	}
	else
	if(FileFormat == 1)
	{
		strcpy(strFileFormat, ".mp3");
		dprintf("	File Format is mp3\r\n");
	}
	else
	if(FileFormat == 2)
	{
		strcpy(strFileFormat, ".ogg");
		dprintf("	File Format is ogg\r\n");
	}
	else
	if(FileFormat == 3)
	{
		strcpy(strFileFormat, ".flac");
		dprintf("	File Format is FLAC\r\n");
	}
	else
	if(FileFormat == 4)
	{
		strcpy(strFileFormat, ".aiff");
		dprintf("	File Format is AIFF\r\n");
	}

	strcpy(MusicFolderFullPath, musdll_path);
	*(strrchr(MusicFolderFullPath, '\\')+1)=0;
	strcat(MusicFolderFullPath, MusicFolder);
	dprintf("	Reading music files from: %s\r\n", MusicFolderFullPath);
	strcpy(MusicFileFullPath, MusicFolderFullPath);
	strcat(MusicFileFullPath, "\\");
	dprintf("	Music folder is: %s\r\n", MusicFileFullPath);
	strcpy(strMusicFile, "*");
	strcat(strMusicFile, strFileFormat);
	strcat(MusicFileFullPath, strMusicFile);
	findTracks = FindFirstFileA(MusicFileFullPath, &MusicFiles);
	int i = 0;
	do
	{
		numTracks++;
		dprintf("	Number of tracks is: %d\r\n", numTracks);
		dprintf("	Music track being read is: %s\r\n", MusicFiles.cFileName);
		strcpy(MusicFileStoredPath, MusicFolderFullPath);
		strcat(MusicFileStoredPath, "\\");
		strcat(MusicFileStoredPath, MusicFiles.cFileName);
		snprintf(tracks[i].path, sizeof tracks[i].path, MusicFileStoredPath, MusicFolderFullPath, i);
		dprintf("	Music track being stored in track info is: %s\r\n", tracks[i].path);
		i++;
	} while (FindNextFileA(findTracks, &MusicFiles) != 0);
	FindClose(findTracks);
	if (numTracks > 0)
	{
		firstTrack = 1;
		lastTrack = numTracks;
		currentTrack = 1;
		if (numTracks > 1)
		{
			nextTrack = 2;
		}
		else
		nextTrack = 1;
		dprintf("	Assigned First, Last, Current, and Next tracks\r\n");
		dprintf("	First track %d\r\n", firstTrack);
		dprintf("	Last track %d\r\n", lastTrack);
		dprintf("	Current track %d\r\n", currentTrack);
		dprintf("	Next track %d\r\n", nextTrack);
	}
	
	return;
}

int bass_play(const char *path)
{
	bool strFree = BASS_ChannelFree(str);
	str = BASS_StreamCreateFile(FALSE, tracks[currentTrack].path, 0, 0, BASS_SAMPLE_LOOP | BASS_STREAM_PRESCAN);
	if (str) 
	{
		strc++;
		strs = (HSTREAM*)realloc((void*)strs, strc * sizeof(*strs));
		strs[strc - 1] = str;
		dprintf("	BASS_StreamCreateFile\r\n");
	}
	else
	dprintf("	BASS cannot stream the file!\r\n");

	bool strPlay = BASS_ChannelPlay(str, FALSE);
	DWORD bassDeviceCheck = BASS_ChannelGetDevice(str);
	int bassError = BASS_ErrorGetCode();
	dprintf("	BASS device: %d\r\n", bassDeviceCheck);
	
	float fft[32768];
	DWORD strBuf = BASS_ChannelGetData(str, fft, BASS_DATA_FFT_INDIVIDUAL);
	dprintf("	BASS Error: %d\r\n", bassError);
	BASS_ChannelGetInfo(str, &cinfo);
						
	plr_fmt.wFormatTag 		= WAVE_FORMAT_PCM;
	plr_fmt.nChannels       = cinfo.chans;
	plr_fmt.nSamplesPerSec  = cinfo.freq;
	plr_fmt.wBitsPerSample  = (cinfo.flags & BASS_SAMPLE_8BITS ? 8 : 16);
	plr_fmt.nBlockAlign     = plr_fmt.nChannels * (plr_fmt.wBitsPerSample / 8);
	plr_fmt.nAvgBytesPerSec = plr_fmt.nBlockAlign * plr_fmt.nSamplesPerSec;
	plr_fmt.cbSize          = 0;
	
	dprintf("	waveformatex tag is: %d\r\n", plr_fmt.wFormatTag);
	dprintf("	waveformatex has this many channels: %d\r\n", plr_fmt.nChannels);
	dprintf("	waveformatex frequency is: %d\r\n", plr_fmt.nSamplesPerSec);
	dprintf("	waveformatex bits per sample is: %d\r\n", plr_fmt.wBitsPerSample);	
	dprintf("	waveformatex block align is: %d\r\n", plr_fmt.nBlockAlign);		
	dprintf("	waveformatex average bytes per second are: %d\r\n", plr_fmt.nAvgBytesPerSec);	

	wave_step = TWOPI / ((float)plr_fmt.nSamplesPerSec / frequency);

	for(int i = 0; i < 2; ++i) {
		for(int j = 0; j < CHUNK_SIZE; ++j) 
		{
			dprintf("	Bass sample length in bytes: %d\r\n", strBuf);
			chunks[i][j] = strBuf;
		}
		header[i].lpData = (CHAR*)chunks[i];
		header[i].dwBufferLength = CHUNK_SIZE * 2;
		if(waveOutPrepareHeader(plr_hwo, &header[i], sizeof(header[i])) != MMSYSERR_NOERROR) 
		{
			dprintf("	waveOutPrepareHeader[%d] failed\r\n", i);
			return -1;
		}
		if(waveOutWrite(plr_hwo, &header[i], sizeof(header[i])) != MMSYSERR_NOERROR) 
		{
			dprintf("	waveOutWrite[%d] failed\r\n", i);
			return -1;
		}
	}
	return 0;
}

void CALLBACK WaveOutProc(HWAVEOUT wave_out_handle, UINT message, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
	switch(message) 
	{
		case WOM_CLOSE: printf("WOM_CLOSE\n"); break;
		case WOM_OPEN:  printf("WOM_OPEN\n");  break;
		case WOM_DONE:{ printf("WOM_DONE\n");
			for(int i = 0; i < CHUNK_SIZE; ++i) 
			{
				DWORD strBuf = BASS_ChannelGetData(str, NULL, BASS_DATA_AVAILABLE);
				dprintf("	Bass sample length in bytes: %d\r\n", strBuf);
				chunks[chunk_swap][i] = strBuf;
			}
			if(waveOutWrite(plr_hwo, &header[chunk_swap], sizeof(header[chunk_swap])) != MMSYSERR_NOERROR) {
				dprintf("	waveOutWrite failed\r\n");
			}
			chunk_swap = !chunk_swap;
		} break;
	}
}

int mmusi_main()
{
	mmusi_config();
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		fh = fopen("mmusi.log", "w"); /* Renamed to .log*/

		GetModuleFileName(hinstDLL, musdll_path, sizeof musdll_path);
		dprintf("	dll attached\r\n");
		dprintf("	musdll_path = %s\r\n", musdll_path);

		InitializeCriticalSection(&cs);
		mmusi_config();
	}

	if (fdwReason == DLL_PROCESS_DETACH)
	{
        if (fh)
        {
            fclose(fh);
            fh = NULL;
        }
		if (AudioLibrary == 2)
		{
			mciSendCommandA(MAGIC_DEVICEID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
		}
		if (AudioLibrary == 5)
		{
			BASS_Free();
		}
    }

    return TRUE;
}

MCIERROR WINAPI mmusi_mciSendCommandA(MCIDEVICEID deviceID, UINT uintMsg, DWORD_PTR dwptrCmd, DWORD_PTR dwParam)
{
	if(TRUE)
	{
		dprintf("mciSendCommandA(deviceID=%p, uintMsg=%p, dwptrCmd=%p, dwParam=%p)\r\n", deviceID, uintMsg, dwptrCmd, dwParam);

		if (uintMsg == MCI_OPEN)
		{
			dprintf("  MCI_OPEN\r\n");
			if(opened == 0)
			{
				opened = 1;
				closed = 0;
				if(AudioLibrary == 5)
				{				
					if (playeractive == 0)
					{
						dprintf("	Audio library for commands is: BASS\r\n");	
						BASS_Init(1, 44100, 0, win, NULL);
						/*BASS_SetDevice(MAGIC_DEVICEID);*/
						playeractive = 1;
						dprintf("	BASS_Init\r\n");
					}
					else
					if (playeractive == 1)
					{
						dprintf("	BASS already initialized, doing nothing\r\n");
					}
				}
			}
			return 0;
		}
		else
		if (uintMsg == MCI_PAUSE)
		{
			dprintf("  MCI_PAUSE\r\n");
			if(paused == 0)
			{
				paused = 1;
				playing = 0;
				if (AudioLibrary == 5)
				{
					BASS_Pause();
					dprintf("	BASS_Pause\r\n");
				}
			}
			return 0;
		}
		else
		if (uintMsg == MCI_STOP)
		{
			dprintf("  MCI_STOP\r\n");
			if(stopped == 0)
			{
				stopped = 1;
				playing = 0;
				if (AudioLibrary == 5)
				{
					BASS_Pause();
					BASS_Stop();
					dprintf("	BASS_Stop\r\n");
				}
				currentTrack = 1; /* Reset current track */
				if (numTracks > 1)
				{
					nextTrack = 2; /* Reset next track */
				}
				else
				nextTrack = 1; /* Reset next track */
			}
			return 0;
		}
		else
		if (uintMsg == MCI_CLOSE)
		{
			dprintf("  MCI_CLOSE\r\n");
			if(closed == 0)
			{
				closed = 1;
				opened = 0;
				dprintf("	Ignoring close command since TA will still send commands after it, potentially causing freezes\r\n");
			}
			return 0;
		}
		else
		if (uintMsg == MCI_STATUS)
		{
			LPMCI_STATUS_PARMS parms = (LPVOID)dwParam;

			dprintf("  MCI_STATUS\r\n");

			parms->dwReturn = 0;
			
			if (parms->dwItem == MCI_STATUS_NUMBER_OF_TRACKS)
			{
				dprintf("      MCI_STATUS_NUMBER_OF_TRACKS %d\r\n", numTracks);
				parms->dwReturn = numTracks;
			}
			else
			if (parms->dwItem == MCI_CDA_STATUS_TYPE_TRACK)
			{
				dprintf("      MCI_CDA_STATUS_TYPE_TRACK\r\n");
				if((parms->dwTrack > 0) &&  (parms->dwTrack , MAX_TRACKS))
				{
					parms->dwReturn = MCI_CDA_TRACK_AUDIO;
				}
			}
			else
			if (parms->dwItem == MCI_STATUS_MODE)
			{
				dprintf("      MCI_STATUS_MODE\r\n");
				if(opened)
				{
					dprintf("        we are open\r\n");
					parms->dwReturn = MCI_MODE_OPEN;
				}
				else
				if(paused)
				{
					dprintf("        we are paused\r\n");
					parms->dwReturn = MCI_MODE_PAUSE;
				}
				else
				if(stopped)
				{
					dprintf("        we are stopped\r\n");
					parms->dwReturn = MCI_MODE_STOP;
				}
				else
				if(playing)
				{
					dprintf("        we are playing\r\n");
					parms->dwReturn = MCI_MODE_PLAY;
				}
			}
			return 0;
		}
		else
		if (uintMsg == MCI_SET)
		{
			LPMCI_SET_PARMS parms = (LPVOID)dwParam;
			
			dprintf("  MCI_SET\r\n");
			
			if (dwptrCmd & MCI_SET_TIME_FORMAT)
			{
				dprintf("    MCI_SET_TIME_FORMAT\r\n");
				timeFormat = parms->dwTimeFormat;
				if (parms->dwTimeFormat == MCI_FORMAT_MILLISECONDS)
				{
					dprintf("      MCI_FORMAT_MILLISECONDS\r\n");
				}
				else
				if (parms->dwTimeFormat == MCI_FORMAT_TMSF)
				{
					dprintf("      MCI_FORMAT_TMSF\r\n");
				}
			}
			return 0;
		}
		else
		if (uintMsg == MCI_PLAY)
		{
			dprintf("  MCI_PLAY\r\n");
			
			LPMCI_PLAY_PARMS parms = (LPVOID)dwParam;
			if (paused == 1)
			{
				paused = 0;
				if(playing == 1)
				{
					if (AudioLibrary == 5)
					{
						BASS_Start();
						dprintf("	BASS_Start from paused\r\n");
					}
				}
			}
			else
			if (stopped == 1)
			{
				stopped = 0;
				if(playing == 1)
				{
					if (AudioLibrary == 5)
					{
						BASS_Start();
						dprintf("	BASS_Start from stopped\r\n");
					}
				}
			}
			
			if (dwptrCmd & MCI_FROM)
			{
				dprintf("  MCI_FROM\r\n");
				currentTrack = (int)(parms->dwFrom);
				dprintf("	From value: %d\r\n", parms->dwFrom);
				dprintf("	Current track int value is: %d\r\n", currentTrack);
				dprintf("	Current track is: %s\r\n", tracks[currentTrack].path);
				if (AudioLibrary == 5)
				{
					playing = 1;
					stopped = 0;
					paused = 0;
					closed = 0;
					bass_play(tracks[currentTrack].path);
				}
			}
			else
			if (dwptrCmd & MCI_TO)
			{
				dprintf("  MCI_TO\r\n");
				nextTrack = parms->dwTo;
				dprintf("	Next track is: %s\r\n", tracks[nextTrack].path);
				if (playing == 1)
				{
					if (AudioLibrary == 5)
					{
						BASS_StreamPutFileData(str, tracks[nextTrack].path, BASS_FILEDATA_END);
						dprintf("	BASS_StreamPutFileData\r\n");
					}
				}
			}
			return 0;
		}
	}
	return MCIERR_UNRECOGNIZED_COMMAND;
}

MCIERROR WINAPI mmusi_mciSendStringA(LPCTSTR lpszCmd, LPTSTR lpszRetStr, UINT cchReturn, HANDLE  hwndCallback)
{
	MCIERROR err;
	if(TRUE) 
	{
		dprintf("[MCI String = %s]\n", lpszCmd);
		
		for (int i = 0; lpszCmd[i]; i++)
		{
			tolower(lpszCmd[i]);
		}
		
		if (strstr(lpszCmd, "open cdaudio"))
		{
			static MCI_WAVE_OPEN_PARMS waveParms;
			dprintf("  MCI_OPEN\r\n");
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_OPEN, 0, (DWORD_PTR)NULL);
			dprintf("	OPEN COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "pause cdaudio"))
		{
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_PAUSE, 0, (DWORD_PTR)NULL);
			dprintf("	PAUSE COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "stop cdaudio"))
		{
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_STOP, 0, (DWORD_PTR)NULL);
			dprintf("	STOP COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "close cdaudio"))
		{
			mmusi_mciSendCommandA(0, MCI_CLOSE, 0, (DWORD_PTR)NULL);
			dprintf("	CLOSE COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "set cdaudio time format milliseconds"))
		{
			static MCI_SET_PARMS parms;
			parms.dwTimeFormat = MCI_FORMAT_MILLISECONDS;	
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&parms);
			dprintf("	TIME FORMAT MILLISECONDS COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "set cdaudio time format tmsf"))
		{
			static MCI_SET_PARMS parms;
			parms.dwTimeFormat = MCI_FORMAT_TMSF;
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&parms);
			dprintf("	TIME FORMAT TMSF COMMAND SENT\r\n");
			return 0;
		}
		if (strstr(lpszCmd, "status cdaudio number of tracks"))
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
			mmusi_mciSendCommandA(0, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&parms);
			dprintf("	NUMBER OF TRACKS STATUS COMMAND SENT\r\n");
			sprintf(lpszRetStr, "%d", numTracks);
			return 0;
		}
		if (strstr(lpszCmd, "status cdaudio type track 1"))
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
			parms.dwTrack = 1;
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (DWORD_PTR)&parms);
			dprintf("	STATUS TYPE TRACK 1 COMMAND SENT\r\n");
			sprintf(lpszRetStr, "%d", parms.dwReturn);
			return 0;
		}
		if (strstr(lpszCmd, "status cdaudio mode"))
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_MODE;
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM|MCI_STATUS_MODE, (DWORD_PTR)&parms);
			dprintf("	STATUS MODE COMMAND SENT\r\n");
			return 0;
		}
		int from = -1, to = -1;
		if (sscanf(lpszCmd, "play cdaudio from %d to %d notify", &from, &to) == 2)
		{
			static MCI_PLAY_PARMS parms;
			parms.dwFrom = from;
			parms.dwTo = to;
			mmusi_mciSendCommandA(MAGIC_DEVICEID, MCI_PLAY, MCI_FROM|MCI_TO|MCI_NOTIFY, (DWORD_PTR)&parms);
			dprintf("	PLAY FROM TO COMMAND SENT\r\n");
			return 0;
		}
	}
	return err;
}

UINT WINAPI mmusi_auxGetNumDevs()
{
	return 1;
}

MMRESULT WINAPI mmusi_auxGetDevCapsA(UINT_PTR uintptrDeviceID, LPAUXCAPSA lpCapsa, UINT cbCaps)
{
	dprintf("mmusi_auxGetDevCapsA(uintptrDeviceID=%08X, lpCapsa=%p, cbCaps=%08X\n", uintptrDeviceID, lpCapsa, cbCaps);

	lpCapsa->wMid = 2 /*MM_CREATIVE*/;
	lpCapsa->wPid = 401 /*MM_CREATIVE_AUX_CD*/;
	lpCapsa->vDriverVersion = 1;
	strcpy(lpCapsa->szPname, "mmusi virtual CD");
	lpCapsa->wTechnology = AUXCAPS_CDAUDIO;
	lpCapsa->dwSupport = AUXCAPS_VOLUME;

	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI mmusi_auxGetVolume(UINT uintDeviceID, LPDWORD lpdwVolume)
{
	dprintf("mmusi_auxGetVolume(uintDeviceID=%08X, lpdwVolume=%p)\r\n", uintDeviceID, lpdwVolume);
	*lpdwVolume = 0x00000000;
	return MMSYSERR_NOERROR;
}


MMRESULT WINAPI mmusi_auxSetVolume(UINT uintDeviceID, DWORD dwVolume)
{
}


BOOL WINAPI mmusi_PlaySoundA(LPCTSTR lpctstrSound, HMODULE hmod, DWORD dwSound)
{
}


UINT WINAPI mmusi_waveOutGetNumDevs()
{
	return 1;
}

MMRESULT WINAPI mmusi_waveOutGetVolume(HWAVEOUT hwo, LPDWORD lpdwVolume)
{
}


MMRESULT WINAPI mmusi_waveOutSetVolume(HWAVEOUT hwo, DWORD dwVolume)
{
}