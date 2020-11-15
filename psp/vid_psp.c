/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <pspkernel.h>
#include "keyboard/danzeff.h"

#include "../client/client.h"

refexport_t	re;

static SceUID ref_library = 0;		// Handle to refresh DLL 

viddef_t	viddef;				// global video state; used by other modules

#define	MAXPRINTMSG	4096
void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	//static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
		Com_Printf ("%s", msg);
	else
		Com_DPrintf ("%s", msg);
}

void VID_Error (int err_level, char *fmt, ...)
{

	va_list		argptr;
	char		msg[MAXPRINTMSG];
	//static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Sys_Error("%s", msg);
}

//==========================================================================

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f(void) 
{
	// PSP_TODO
}

/*
==============
VID_LoadRefresh
==============
*/
#if 0
qboolean VID_LoadRefresh( char *name ) {
	refexport_t GetRefAPI(refimport_t);

	if(ref_library) {
		re.Shutdown();
		VID_FreeReflib();
	}

	refimport_t ri;
	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	//ri.Vid_MenuInit = VID_MenuInit;
	//ri.Vid_NewWindow = VID_NewWindow;

	re = GetRefAPI(ri);

	if(re.api_version != API_VERSION) {
		VID_FreeReflib ();
		Com_Error (ERR_FATAL, "renderer has incompatible api_version", name);
	}

	if(re.Init() == -1 )
	{
		re.Shutdown();
		VID_FreeReflib();
		return false;
	}

	ref_library = 1;
	return true;
}
#endif

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
#if 0
void VID_CheckChanges (void)
{
	char name[100];

	if ( vid_ref->modified )
	{
		S_StopAllSounds();
	}

	while (vid_ref->modified)
	{
		/*
		** refresh has changed
		*/
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		sprintf( name, "ref_%s.so", vid_ref->string );
		if ( !VID_LoadRefresh( name ) )
		{
			Com_Error (ERR_FATAL, "Couldn't fall back to software refresh!");
		}
		cls.disable_screen = false;
	}
}
#endif

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Add some console commands that we want to handle */
	Cmd_AddCommand("vid_restart", VID_Restart_f);

	refimport_t ri;

	if(ref_library) 
	{
		re.Shutdown();
		memset(&re, 0, sizeof(re));
		Sys_UnloadRef();
	}
 
	ri.Cmd_AddCommand    = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc          = Cmd_Argc;
	ri.Cmd_Argv          = Cmd_Argv;
	ri.Cmd_ExecuteText   = Cbuf_ExecuteText;
	ri.Con_Printf        = VID_Printf;
	ri.Sys_Error         = VID_Error;
	ri.FS_LoadFile       = FS_LoadFile;
	ri.FS_FreeFile       = FS_FreeFile;
	ri.FS_Gamedir        = FS_Gamedir;
	ri.Cvar_Get          = Cvar_Get;
	ri.Cvar_Set          = Cvar_Set;
	ri.Cvar_SetValue     = Cvar_SetValue;
	ri.Vid_NewWindow     = VID_NewWindow;
 /*
    ri.Vid_GetModeInfo   = VID_GetModeInfo;
	ri.Vid_MenuInit      = VID_MenuInit;
    ri.S_Update_         = S_Update_;
*/
 	//re = Sys_GetRefAPI(ri);

    refexport_t GetRefAPI(refimport_t);
	re = (refexport_t)GetRefAPI((refimport_t)ri);

	if(re.api_version != API_VERSION) 
    {
		Sys_Error("pspgu.prx has incompatible api_version");
	}

	if(!re.Init())  
    {
		re.Shutdown();
		Sys_Error("pspgu init error");
	}
	
	ref_library = 1;
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void) 
{
	re.Shutdown ();
	if(ref_library) 
	{
		memset(&re, 0, sizeof(re));
		ref_library = 0;
	}
}

void RW_InitKey (void)
{
    danzeff_load();
    danzeff_moveTo(330,0);
}

void RW_ShutdownKey (void)
{
    danzeff_free();
}
