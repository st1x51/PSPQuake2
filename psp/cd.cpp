/*
Copyright (C) 2007 Peter Mackay and Chris Swindle.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <cstddef>
#include <stdio.h>

#include <pspaudiolib.h>
#include <pspiofilemgr.h>


#include "mp3.h"

extern "C"
{
#include "../client/client.h"
}

cvar_t *m_volume;

#define MAX_TRACKS 128

namespace quake
{
	namespace cd
	{
		struct Sample
		{
			short left;
			short right;
		};
		
		static int	 last_track = 4;
		static bool	 playing  = false;
		static bool	 paused   = false;
		static bool	 enabled  = false;
	    static int   num_tracks = 0;
	    static char  tracks[MAX_TRACKS][MAX_QPATH];
	    static int   cd_suspended = 0;
	    static int   cd_loop = 0;
	    static int   cd_init = 0;
	    //static int   cd_pos = 0;
	    
	}
}


using namespace quake;
using namespace quake::cd;

static void CD_f (void)
{
	char	*command;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("commands:");
		Com_Printf("on, off, reset,\n");
		Com_Printf("play, stop, loop, pause, resume, \n");
		Com_Printf("next, prev, list, info\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (Q_strcasecmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		return;
	}
  
	if (Q_strcasecmp(command, "play") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), (qboolean) false);
		return;
	}

	if (Q_strcasecmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)atoi(Cmd_Argv (2)), (qboolean) true);
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}
  
	if (Q_strcasecmp(command, "next") == 0)
	{
		CDAudio_Next();
		return;
	}

	if (Q_strcasecmp(command, "prev") == 0)
	{
		CDAudio_Prev();
		return;
	}

	if (Q_strcasecmp(command, "list") == 0)
	{
        CDAudio_PrintMusicList();
		return;
	}
	
	if (Q_strcasecmp(command, "info") == 0)
	{
        Com_Printf("MP3 Player By Crow_bar\n");
		Com_Printf("Based On sceMp3 Lib\n");
		Com_Printf("- 2009 -\n");
		Com_Printf("\n");
		
		Com_Printf("%d tracks\n", num_tracks);
		if(playing)
		{
            if(!paused)
            {
				Com_Printf("Currently track %i:%s\n mode: %s\n",
					last_track, tracks[last_track], cd_loop ? "looping" : "auto change");
            }
			else
			{
				Com_Printf("Paused track %i:%s\n mode: %s\n",
					last_track, tracks[last_track], cd_loop ? "looping" : "auto change");
			}
		}
		return;
	}
}

void CDAudio_VolumeChange(float bgmvolume)
{
	if(!enabled)
        return;

	int volume = (int) (bgmvolume * (float) PSP_VOLUME_MAX);
	if(mp3_volume != volume)
	{
	   mp3_volume = volume;
    }
}

extern "C" int sceKernelDelayThread(int delay); 

void CDAudio_Play(byte track, qboolean looping)
{
    if(!enabled)
        return;
	
	CDAudio_Stop();
  
	if(track >= num_tracks || track <= 0)
	   track = 1;

	last_track = track;

	int ret;
	ret = mp3_start_play(tracks[track], 0);
	
	cd_loop = looping;
	if(ret != 2)
	{
		Com_Printf("Playing %s\n", tracks[track]);
		playing = true;
	}
	else
	{
		Com_Printf("Couldn't find %s\n", tracks[track]);
		playing = false;
		CDAudio_VolumeChange(0);
	}
	

	CDAudio_VolumeChange(m_volume->value);
}

void CDAudio_Stop(void)
{
	if(!enabled)
       return;
    
	mp3_job_started = 0;
	playing = false;
	
	CDAudio_VolumeChange(0);
}

void CDAudio_Pause(void)
{
    if(!enabled)
        return;
        
	paused = true;
}

void CDAudio_Resume(void)
{
	if(!enabled)
        return;
        
	paused = false;
}

void CDAudio_Update(void)
{
	  if(enabled == true)
	  {
			if(mp3_status == MP3_END)
		    {
			     if(cd_loop == 0)
					   last_track = last_track + 1;

					 CDAudio_Play(last_track, (qboolean)cd_loop);
		    }
		    else if(mp3_status == MP3_NEXT)
		    {
			    last_track = last_track + 1;
			    CDAudio_Play(last_track, (qboolean)cd_loop);
		    }
            CDAudio_VolumeChange(m_volume->value);
	  }
}

void CDAudio_Next(void)
{
   if(!enabled)
    return;
	
	 last_track = last_track + 1;
	 CDAudio_Play(last_track, (qboolean) false); 
}

void CDAudio_Prev(void)
{
   if(!enabled)
    return;
    
	 last_track = last_track - 1;
	 CDAudio_Play(last_track, (qboolean) false); 
}
/*
int CDAudio_Compare( const void* a, const void* b )
{  
     char* arg1 = (char*) a;
     char* arg2 = (char*) b;

	 if( *arg1 < *arg2 )
	     return -1;
     else if( *arg1 == *arg2 )
	     return 0;
     else
	     return 1;
}
*/
extern cvar_t	*fs_basedir;
int CDAudio_Init(void)
{
    m_volume = Cvar_Get ("m_volume", "0.5", CVAR_ARCHIVE);

	if(COM_CheckParm("-nomp3"))
	   return 0;
	  
	mp3_init();
	
	sceKernelDelayThread(5*10000);
  
	num_tracks = 1;

	char path[256];
	sprintf(path, "%s/MP3/", fs_basedir->string);
	SceUID dir = sceIoDopen(path);
	if(dir < 0) 
	{
		Com_Printf("/MP3/ Directory not found\n");
		return 0;
	}
	SceIoDirent dirent;
    memset(&dirent, 0, sizeof(SceIoDirent));
	while(sceIoDread(dir, &dirent) > 0) 
	{
		if(dirent.d_name[0] == '.') 
		{
			continue;
		}  
        if(!strcasecmp(COM_FileExtension (dirent.d_name),"mp3"))
	    {
			Com_Printf("music track: %s\n", dirent.d_name);
			sprintf(tracks[num_tracks],"%s%s",path, dirent.d_name);
			num_tracks++;
		}
		memset(&dirent, 0, sizeof(SceIoDirent));
    }
	sceIoDclose(dir);

	//qsort( *tracks, num_tracks, sizeof(char), CDAudio_Compare );

	enabled = true;
	cd_init = 1;

	Cmd_AddCommand ("cd", CD_f);
	Com_Printf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_PrintMusicList(void)
{
    Com_Printf("\n");
    Com_Printf("================== Music List ===================\n");
	for(int i = 1; i < num_tracks ; i++)
	{
	  Com_Printf("%i: %s\n", i, tracks[i]);
	}
    Com_Printf("=================================================\n");
    Com_Printf("\n");
}

void CDAudio_Shutdown(void)
{
    if(!cd_init)
		return;

	Com_Printf("CDAudio_Shutdown\n");

	CDAudio_Stop();
	sceKernelDelayThread(5*10000);
	mp3_deinit();

	cd_init = 0;
}

void CDAudio_SysSuspend (void)
{
	if(!cd_init)
	    return;

	if (playing)
	{
       //cd_pos = mp3_getpos();
	   CDAudio_Stop();
	   cd_suspended = 2; //for played music
	   sceKernelDelayThread(5*10000); //wait for mp3 thread
	}
	else
	{
       cd_suspended = 1; //for other
	}

}

void CDAudio_SysResume(void)
{
	if(!cd_init)
		return;

	if (cd_suspended == 2)
	{
	   CDAudio_Play(last_track, (qboolean)cd_loop); //resume last music
	}
	cd_suspended = 0;
}
