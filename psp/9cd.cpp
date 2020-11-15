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

#include "mp3.h"

extern "C"
{
#include "../client/client.h"
#include "cd.h"
}



//extern 	cvar_t bgmtype;
//extern	cvar_t bgmvolume;

static int	     file = -1;
static int last_track = 4;
static bool	 playing  = false;
static bool	 paused   = false;
static bool	 enabled  = false;
static float cdvolume = 0;


struct play_list
{
  const char *path;
}

play_list[] =
{
  NULL,
	{"01)id Logo.mp3"},
  {"02)Intro.mp3"},
  {"03)Ntro.mp3"},
 	{"04)Operation Overlord.mp3"},
  {"05)Rage.mp3"},
  {"06)Kill Ratio.mp3"},
	{"07)March of the Stroggs.mp3"},
  {"08)The Underworld.mp3"},
  {"09)Quad Machine.mp3"},
  {"10)Big Gun.mp3"},
  {"11)Descent into Cerberon.mp3"},
  {"12)Climb.mp3"},
  {"13)Showdown.mp3"},
 	{"14)End.mp3"},
  {"15)Gravity Well.mp3"},
  {"16)Counter Attack.mp3"},
	{"17)Stealth Frag.mp3"},
  {"18)Crashed Up Again.mp3"},
  {"19)Adrenaline Junkie.mp3"},
  {"20)ETF.mp3"},
  {"21)Complex 13.mp3"},
  {"22)Pressure Point 1.mp3"},
  {"23)Pressure Point 2.mp3"},
  {"24)Ground Zero.mp3"}
};

static void CD_f (void)
{
	char	*command;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("commands:");
		Com_Printf("on, off, reset, remap, \n");
		Com_Printf("play, stop, loop, pause, resume\n");
		Com_Printf("eject, close, info\n");
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

	if (Q_strcasecmp(command, "remap") == 0)
	{
		return;
	}

	if (Q_strcasecmp(command, "close") == 0)
	{
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

	if (Q_strcasecmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		return;
	}

	if (Q_strcasecmp(command, "info") == 0)
	{
		return;
	}
}

void CDAudio_RandomPlay(void)
{
}

void CDAudio_VolumeChange(float bgmvolume)
{
	int volume = (int) (bgmvolume * (float) PSP_VOLUME_MAX);
	mp3_volume = volume;
	changeMp3Volume=0;
	cdvolume = bgmvolume;
}

extern "C" int sceKernelDelayThread(int delay); 

void CDAudio_Play(byte track, qboolean looping)
{
	if(track == 25)
	   track == 1;
	
	last_track = track;
	
	CDAudio_Stop();

	char path[256];
	sprintf(path, "ms0:/q2mp3/%s", play_list[last_track].path);
	
	int ret;
	ret = mp3_start_play(path, 0);
	
	if(ret != 2)
	{
		Com_Printf("Playing %s\n", path);
		playing = true;
	}
	else
	{
		Com_Printf("Couldn't find %s\n", path);
		playing = false;
		CDAudio_VolumeChange(0);
	}
	

	CDAudio_VolumeChange(1);
}

void CDAudio_Stop(void)
{
	mp3_job_started = 0;
	playing = false;
	CDAudio_VolumeChange(0);
}

void CDAudio_Pause(void)
{
	CDAudio_VolumeChange(0);
	paused = true;
}

void CDAudio_Resume(void)
{
	CDAudio_VolumeChange(1);
	paused = false;
}

int from_play = 0;

void CDAudio_Update(void)
{

 if(mp3_stat == MP3_PLAY)
 {
	 from_play = 1;
 }
 else if(mp3_stat == MP3_STOP)
 {
  //if(from_play == 1)
  //{
    CDAudio_Play(last_track + 1, false);
  //}
 }
 else if(mp3_stat == MP3_NONE)
 {
   from_play = 1;
 }

 

/*
	// Fill the input buffer.
	if (strcmpi(bgmtype.string,"cd") == 0) 
      {
		if (playing == false) 
            {
			CDAudio_Play(last_track, (qboolean) false);
		}
		if (paused == true) 
            {
			CDAudio_Resume();
		}
	
	} 
      else 
      {
		if (paused == false) 
            {
			CDAudio_Pause();
		}
		if (playing == true) 
            {
			CDAudio_Stop();
		}
	}
*/}

int CDAudio_Init(void)
{
	mp3_init();
	
      sceKernelDelayThread(5*10000);

	enabled = true;
	
	Cmd_AddCommand ("cd", CD_f);
	
	Com_Printf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown(void)
{
	Com_Printf("CDAudio_Shutdown\n");
	
	CDAudio_Stop();

	sceKernelDelayThread(5*10000);
	mp3_deinit();
}

