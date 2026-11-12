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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

  full screen console
  put up loading plaque
  blanked background with loading plaque
  blanked background with menu
  cinematics
  full screen image for quit and victory

  end of unit intermissions

  */

#include "client.h"

float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

qboolean	scr_initialized;		// ready to draw

int			scr_draw_loading;

vrect_t		scr_vrect;		// position of render window on screen


cvar_t		*scr_viewsize;
#ifdef KINGPIN_ENGINE
cvar_t		*scr_hud;
#endif
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;
cvar_t		*scr_drawall;
cvar_t		*scr_drawfps;

typedef struct
{
	int		x1, y1, x2, y2;
} dirty_t;

dirty_t		scr_dirty, scr_old_dirty[2];

char		crosshair_pic[MAX_QPATH];
int			crosshair_width, crosshair_height;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);


/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph (void)
{
	int		i;
	int		in;
	int		ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if (scr_debuggraph->value || scr_timegraph->value)
		return;

	for (i=0 ; i<cls.netchan.dropped ; i++)
		SCR_DebugGraph (30, 0x40);

	for (i=0 ; i<cl.surpressCount ; i++)
		SCR_DebugGraph (30, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & (CMD_BACKUP-1);
	ping = cls.realtime - cl.cmd_time[in];
	ping /= 30;
	if (ping > 30)
		ping = 30;
	SCR_DebugGraph (ping, 0xd0);
}


typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	//
	// draw the graph
	//
	w = scr_vrect.width;

	x = scr_vrect.x;
	y = scr_vrect.y+scr_vrect.height;
	re.DrawFill (x, y-scr_graphheight->value,
		w, scr_graphheight->value, 8);

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v*scr_graphscale->value + scr_graphshift->value;

		if (v < 0)
			v += scr_graphheight->value * (1+(int)(-v/scr_graphheight->value));
		h = (int)v % (int)scr_graphheight->value;
		re.DrawFill (x+w-1-a, y - h, 1,	h, color);
	}
}

/*
==============
SCR_DrawFPS
==============
*/
#define FPS_AVG_FRAMES	60
void SCR_DrawFPS (void)
{
	static int		len = 0;
	static char		buffer[64];
	static float	avgfps = 0.0f;

	int				i;

	if((cls.framecount % FPS_AVG_FRAMES) == 0)
	{
		len = snprintf(buffer, sizeof(buffer), "%d fps", (int)(FPS_AVG_FRAMES / avgfps));
		avgfps = 0;
	}
	else
	{
		avgfps += cls.frametime;
	}

	for(i = 0; i < len; i++)
		re.DrawChar(viddef.width - (len - i) * 8, 0, buffer[i] + 128);
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (char *str)
{
	char	*s;
	char	line[64];
	int		i, j, l;

	strncpy (scr_centerstring, str, sizeof(scr_centerstring)-1);
	scr_centertime_off = scr_centertime->value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	s = str;
	while (*s)
	{
		if (*s == '\n')
			scr_center_lines++;
		s++;
	}

	// echo it to the console
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

	s = str;
	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (s[l] == '\n' || !s[l])
				break;
		for (i=0 ; i<(40-l)/2 ; i++)
			line[i] = ' ';

		for (j=0 ; j<l ; j++)
		{
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i+1] = 0;

		Com_Printf ("%s", line);

		while (*s && *s != '\n')
			s++;

		if (!*s)
			break;
		s++;		// skip the \n
	} while (1);
	Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify ();
}


void SCR_DrawCenterString (void)
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

// the finale prints the characters one at a time
	remaining = 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = viddef.height*0.35;
	else
		y = 48;

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (viddef.width - l*8)/2;
		SCR_AddDirtyPoint (x, y);
		for (j=0 ; j<l ; j++, x+=8)
		{
			re.DrawChar (x, y, start[j]);
			if (!remaining--)
				return;
		}
		SCR_AddDirtyPoint (x, y+8);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

void SCR_CheckDrawCenterString (void)
{
	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0)
		return;

	SCR_DrawCenterString ();
}

//=============================================================================

/*
=================
SCR_CalcVrect

Sets scr_vrect, the coordinates of the rendered window
=================
*/
static void SCR_CalcVrect (void)
{
	int		size;

	// bound viewsize
	if (scr_viewsize->value < 40)
		Cvar_Set ("viewsize","40");
	if (scr_viewsize->value > 100)
		Cvar_Set ("viewsize","100");

	size = scr_viewsize->value;

	scr_vrect.width = viddef.width*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = viddef.height*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (viddef.width - scr_vrect.width)/2;
	scr_vrect.y = (viddef.height - scr_vrect.height)/2;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize->value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValue ("viewsize",scr_viewsize->value-10);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed
=================
*/
void SCR_Sky_f (void)
{
	float	rotate;
	vec3_t	axis;

#ifdef KINGPIN_ENGINE
	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: sky <basename>\n");
		return;
	}
	re.SetSky (Cmd_Argv(1));
#else
	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: sky <basename> <rotate> <axis x y z>\n");
		return;
	}
	if (Cmd_Argc() > 2)
		rotate = atof(Cmd_Argv(2));
	else
		rotate = 0;
	if (Cmd_Argc() == 6)
	{
		axis[0] = atof(Cmd_Argv(3));
		axis[1] = atof(Cmd_Argv(4));
		axis[2] = atof(Cmd_Argv(5));
	}
	else
	{
		axis[0] = 0;
		axis[1] = 0;
		axis[2] = 1;
	}

	re.SetSky (Cmd_Argv(1), rotate, axis);
#endif
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
#ifdef KINGPIN_ENGINE
	scr_hud = Cvar_Get ("scr_hud", "2", 0);
#endif
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);
	scr_drawall = Cvar_Get ("scr_drawall", "0", 0);
	scr_drawfps = Cvar_Get ("scr_drawfps", "0", CVAR_ARCHIVE);

//
// register our commands
//
	Cmd_AddCommand ("timerefresh",SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading",SCR_Loading_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
	Cmd_AddCommand ("sky",SCR_Sky_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged
		< CMD_BACKUP-1)
		return;

	re.DrawPic (scr_vrect.x+64, scr_vrect.y, "net");
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	int		w, h;

	if (!scr_showpause->value)		// turn off for screenshots
		return;

	if (!cl_paused->value)
		return;

	re.DrawGetPicSize (&w, &h, "pause");
	re.DrawPic ((viddef.width-w)/2, viddef.height/2 + 8, "pause");
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	int		w, h;

	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;
#ifdef KINGPIN_ENGINE
	// kingpin.exe @004123e0: the processing plaque replaces the entire
	// frame, rather than drawing Quake II's "loading" picture on top.
	re.DrawStretchPic (0, 0, viddef.width, viddef.height, "conback");
	re.DrawGetPicSize (&w, &h, "processing");
	re.DrawPic ((viddef.width-w)/2, (viddef.height-h)/2, "processing");
#else
	re.DrawGetPicSize (&w, &h, "loading");
	re.DrawPic ((viddef.width-w)/2, (viddef.height-h)/2, "loading");
#endif
}

//=============================================================================

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole (void)
{
// decide on the height of the console
	if (cls.key_dest == key_console)
		scr_conlines = 0.5;		// half screen
	else
		scr_conlines = 0;				// none visible

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value*cls.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value*cls.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	Con_CheckResize ();

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
	{	// forced full screen console
		Con_DrawConsole (1.0);
		return;
	}

	if (cls.state != ca_active || !cl.refresh_prepped)
	{	// connected, but can't render
		Con_DrawConsole (0.5);
		re.DrawFill (0, viddef.height/2, viddef.width, viddef.height/2, 0);
		return;
	}

	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current);
	}
	else
	{
		if (cls.key_dest == key_game || cls.key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;		// don't play ambients
	CDAudio_Stop ();
	if (cls.disable_screen)
		return;
	if (developer->value)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;
	if (cl.cinematictime > 0)
		scr_draw_loading = 2;	// clear to balack first
	else
		scr_draw_loading = 1;
	SCR_UpdateScreen ();
	cls.disable_screen = Sys_Milliseconds ();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
#ifdef KINGPIN_ENGINE
	com_stoprender = false;
#endif
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

/*
================
SCR_TimeRefresh_f
================
*/
int entitycmpfnc( const entity_t *a, const entity_t *b )
{
	/*
	** all other models are sorted by model then skin
	*/
	if ( a->model == b->model )
	{
		return ( ( int ) a->skin - ( int ) b->skin );
	}
	else
	{
		return ( ( int ) a->model - ( int ) b->model );
	}
}

void SCR_TimeRefresh_f (void)
{
	int		i;
	int		start, stop;
	float	time;

	if ( cls.state != ca_active )
		return;

	start = Sys_Milliseconds ();

	if (Cmd_Argc() == 2)
	{	// run without page flipping
		re.BeginFrame( 0 );
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;
			re.RenderFrame (&cl.refdef);
		}
		re.EndFrame();
	}
	else
	{
		for (i=0 ; i<128 ; i++)
		{
			cl.refdef.viewangles[1] = i/128.0*360.0;

			re.BeginFrame( 0 );
			re.RenderFrame (&cl.refdef);
			re.EndFrame();
		}
	}

	stop = Sys_Milliseconds ();
	time = (stop-start)/1000.0;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

/*
=================
SCR_AddDirtyPoint
=================
*/
void SCR_AddDirtyPoint (int x, int y)
{
	if (x < scr_dirty.x1)
		scr_dirty.x1 = x;
	if (x > scr_dirty.x2)
		scr_dirty.x2 = x;
	if (y < scr_dirty.y1)
		scr_dirty.y1 = y;
	if (y > scr_dirty.y2)
		scr_dirty.y2 = y;
}

void SCR_DirtyScreen (void)
{
	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear (void)
{
	int		i;
	int		top, bottom, left, right;
	dirty_t	clear;

	if (scr_drawall->value)
		SCR_DirtyScreen ();	// for power vr or broken page flippers...

	if (scr_con_current == 1.0)
		return;		// full screen console
	if (scr_viewsize->value == 100)
		return;		// full screen rendering
	if (cl.cinematictime > 0)
		return;		// full screen cinematic

	// erase rect will be the union of the past three frames
	// so tripple buffering works properly
	clear = scr_dirty;
	for (i=0 ; i<2 ; i++)
	{
		if (scr_old_dirty[i].x1 < clear.x1)
			clear.x1 = scr_old_dirty[i].x1;
		if (scr_old_dirty[i].x2 > clear.x2)
			clear.x2 = scr_old_dirty[i].x2;
		if (scr_old_dirty[i].y1 < clear.y1)
			clear.y1 = scr_old_dirty[i].y1;
		if (scr_old_dirty[i].y2 > clear.y2)
			clear.y2 = scr_old_dirty[i].y2;
	}

	scr_old_dirty[1] = scr_old_dirty[0];
	scr_old_dirty[0] = scr_dirty;

	scr_dirty.x1 = 9999;
	scr_dirty.x2 = -9999;
	scr_dirty.y1 = 9999;
	scr_dirty.y2 = -9999;

	// don't bother with anything convered by the console)
	top = scr_con_current*viddef.height;
	if (top >= clear.y1)
		clear.y1 = top;

	if (clear.y2 <= clear.y1)
		return;		// nothing disturbed

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	if (clear.y1 < top)
	{	// clear above view screen
		i = clear.y2 < top-1 ? clear.y2 : top-1;
		re.DrawTileClear (clear.x1 , clear.y1,
			clear.x2 - clear.x1 + 1, i - clear.y1+1, "backtile");
		clear.y1 = top;
	}
	if (clear.y2 > bottom)
	{	// clear below view screen
		i = clear.y1 > bottom+1 ? clear.y1 : bottom+1;
		re.DrawTileClear (clear.x1, i,
			clear.x2-clear.x1+1, clear.y2-i+1, "backtile");
		clear.y2 = bottom;
	}
	if (clear.x1 < left)
	{	// clear left of view screen
		i = clear.x2 < left-1 ? clear.x2 : left-1;
		re.DrawTileClear (clear.x1, clear.y1,
			i-clear.x1+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x1 = left;
	}
	if (clear.x2 > right)
	{	// clear left of view screen
		i = clear.x1 > right+1 ? clear.x1 : right+1;
		re.DrawTileClear (i, clear.y1,
			clear.x2-i+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x2 = right;
	}

}


//===============================================================


#define STAT_MINUS		10	// num frame for '-' stats digit
#ifdef KINGPIN_ENGINE
// white, green, red and dark red digits; only the first two are
// precached by SCR_TouchPics (kingpin.exe @00412e40)
char		*sb_nums[4][11] =
{
	{"hud_0", "hud_1", "hud_2", "hud_3", "hud_4", "hud_5",
	"hud_6", "hud_7", "hud_8", "hud_9", "hud_-"},
	{"ghud_0", "ghud_1", "ghud_2", "ghud_3", "ghud_4", "ghud_5",
	"ghud_6", "ghud_7", "ghud_8", "ghud_9", "ghud_-"},
	{"rhud_0", "rhud_1", "rhud_2", "rhud_3", "rhud_4", "rhud_5",
	"rhud_6", "rhud_7", "rhud_8", "rhud_9", "rhud_-"},
	{"rrhud_0", "rrhud_1", "rrhud_2", "rrhud_3", "rrhud_4", "rrhud_5",
	"rrhud_6", "rrhud_7", "rrhud_8", "rrhud_9", "rrhud_-"}
};
#else
char		*sb_nums[2][11] =
{
	{"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
	"num_6", "num_7", "num_8", "num_9", "num_minus"},
	{"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
	"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"}
};
#endif
#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8



/*
================
SizeHUDString

Allow embedded \n in the string
================
*/
void SizeHUDString (char *string, int *w, int *h)
{
	int		lines, width, current;

	lines = 1;
	width = 0;

	current = 0;
	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;
			if (current > width)
				width = current;
		}
		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}

void DrawHUDString (char *string, int x, int y, int centerwidth, int xor)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width*8)/2;
		else
			x = margin;
		for (i=0 ; i<width ; i++)
		{
			re.DrawChar (x, y, line[i]^xor);
			x += 8;
		}
		if (*string)
		{
			string++;	// skip the \n
			x = margin;
			y += 8;
		}
	}
}


/*
==============
HUD scaling

The layout code places elements in 640x480 units scaled to the screen, but
the pictures keep their pixel size. On a small screen (PSP) shrink them too.
==============
*/
static float SCR_HudScale (void)
{
	float	s = viddef.height * (1.0f/480);

	return s < 1.0f ? s : 1.0f;
}

static void SCR_HudStretch (int x, int y, int w, int h, char *name, float alpha)
{
	float	s = SCR_HudScale ();

	re.DrawStretchPicTransparent (x, y, (int)(w*s), (int)(h*s), name, alpha);
}

static void SCR_HudPic (int x, int y, char *name, float alpha)
{
	int		w, h;

	if (SCR_HudScale () >= 1.0f)
	{
		re.DrawPicTransparent (x, y, name, alpha);
		return;
	}

	re.DrawGetPicSize (&w, &h, name);
	if (w <= 0 || h <= 0)
	{
		re.DrawPicTransparent (x, y, name, alpha);	// let the renderer report it
		return;
	}
	SCR_HudStretch (x, y, w, h, name, alpha);
}

#ifdef KINGPIN_ENGINE
/*
==============
SCR_DrawField
==============
*/
/*
kingpin.exe @00412bc0. color picks one of the sb_nums rows. offsets, when
given, holds an extra (x, y) shift for every drawn digit (the slanted
health and cash counters) and widens the dirty box by 10 pixels.
*/
void SCR_DrawField (int x, int y, int color, int width, int value, int (*offsets)[2])
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	if (offsets)
	{
		SCR_AddDirtyPoint (x-10, y-10);
		SCR_AddDirtyPoint (x+width*CHAR_WIDTH+12, y+33);
	}
	else
	{
		SCR_AddDirtyPoint (x, y);
		SCR_AddDirtyPoint (x+width*CHAR_WIDTH+2, y+23);
	}

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += (int)((2 + CHAR_WIDTH*(width - l)) * SCR_HudScale ());

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		if (offsets)
		{
			SCR_HudPic (x + (int)((*offsets)[0] * SCR_HudScale ()), y + (int)((*offsets)[1] * SCR_HudScale ()), sb_nums[color][frame], 0.75f);
			offsets++;
		}
		else
			SCR_HudPic (x, y, sb_nums[color][frame], 0.75f);
		x += (int)(CHAR_WIDTH * SCR_HudScale ());
		ptr++;
		l--;
	}
}

/*
==============
SCR_DrawScaledField
==============
*/
/*
kingpin.exe @00412d10: SCR_DrawField with the digits stretched by scale
(the armor counters)
*/
void SCR_DrawScaledField (int x, int y, int color, int width, int value, float scale)
{
	scale *= SCR_HudScale ();
	char	num[16], *ptr;
	int		l;
	int		frame;
	int		w, h;

	if (width < 1)
		return;
	if (width > 5)
		width = 5;

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += (int)((2 + CHAR_WIDTH*(width - l)) * scale);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		re.DrawGetPicSize (&w, &h, sb_nums[color][frame]);
		SCR_HudStretch (x, y, (int)(w*scale), (int)(h*scale), sb_nums[color][frame], 0.75f);
		x += (int)(CHAR_WIDTH*scale);
		ptr++;
		l--;
	}
}
#else
/*
==============
SCR_DrawField
==============
*/
void SCR_DrawField (int x, int y, int color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	SCR_AddDirtyPoint (x, y);
	SCR_AddDirtyPoint (x+width*CHAR_WIDTH+2, y+23);

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		re.DrawPic (x,y,sb_nums[color][frame]);
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}
#endif

/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	int		i, j;

	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<11 ; j++)
			re.RegisterPic (sb_nums[i][j]);

	if (crosshair->value)
	{
		if (crosshair->value > 3 || crosshair->value < 0)
			crosshair->value = 3;

#ifdef KINGPIN_ENGINE
		Com_sprintf (crosshair_pic, sizeof(crosshair_pic), "/pics/ch%i.pcx", (int)(crosshair->value));
#else
		Com_sprintf (crosshair_pic, sizeof(crosshair_pic), "ch%i", (int)(crosshair->value));
#endif
		re.DrawGetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);
		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

#ifdef KINGPIN_ENGINE
float CalcFov (float fov_x, float width, float height);

#include "notebook_ru.h"

const char *CL_LocalizeNotebookText (const char *text)
{
	int i;

	for (i = 0; i < sizeof(notebook_ru) / sizeof(notebook_ru[0]); i++)
		if (!strcmp (text, notebook_ru[i].english))
			return notebook_ru[i].russian;
	return text;
}

/*
================
DrawHUDString2

kingpin.exe @00412af0: DrawHUDString with the second font and 12 pixel lines
================
*/
static void DrawHUDString2 (const char *string, int x, int y, int centerwidth, int xor)
{
	int		margin;
	char	line[1024];
	int		width;
	int		i;

	margin = x;

	while (*string)
	{
		width = 0;
		while (*string && *string != '\n')
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width*8)/2;
		else
			x = margin;
		for (i=0 ; i<width ; i++)
		{
			re.DrawChar2 (x, y, line[i]^xor);
			x += 8;
		}
		if (*string)
		{
			string++;	// skip the \n
			y += 12;
		}
	}
}

/*
================
DrawBigString

kingpin.exe @00421b50: big font, 10 pixels per character, tinted
================
*/
static void DrawBigString (int x, int y, char *s, float *color)
{
	while (*s)
	{
		re.DrawBigChar (x, y, *s, color);
		x += 10;
		s++;
	}
}

/*
================
SCR_SetHUDModelLights

The two directional lights kingpin.exe gives models drawn on the HUD
================
*/
static void SCR_SetHUDModelLights (entity_t *ent, vec3_t org0, float intensity0, float dark)
{
	model_lighting_t	*ml;

	ml = &ent->model_lighting;
	ent->flags2 |= RF2_DIR_LIGHTS;
	ml->num_dir_lights = 2;
	VectorSet (ml->light_colors[0], 1, 1, 1);
	VectorCopy (org0, ml->light_orgs[0]);
	ml->light_intensities[0] = intensity0;
	VectorSet (ml->light_colors[1], dark, dark, dark);
	VectorClear (ml->light_orgs[1]);
	ml->light_intensities[1] = 1024;
}

/*
================
SCR_DrawHUDModel

kingpin.exe @00412ef0: a slowly spinning model in a screen box (the
teamplay money bag)
================
*/
static void SCR_DrawHUDModel (int x, int y, char *model, int w, int h)
{
	refdef_t	refdef;
	entity_t	entity;
	vec3_t		org;

	memset (&refdef, 0, sizeof(refdef));
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov (40, w, h);
	refdef.time = cls.realtime * 0.001;

	memset (&entity, 0, sizeof(entity));
	entity.model = re.RegisterModel (model);
	entity.flags = RF_DEPTHHACK | RF_DIMLIGHT;
	entity.flags2 = RF2_NOSHADOW | RF2_DIR_LIGHTS | RF2_PASSLIGHT | 0x1f;
	VectorSet (entity.origin, 61, 10, 5);
	VectorCopy (entity.origin, entity.oldorigin);
	entity.angles[1] = cls.realtime * 0.001 * 22.5;
	VectorSet (org, 100, 100, 0);
	SCR_SetHUDModelLights (&entity, org, 512, 0.2f);

	refdef.rdflags = RDF_NOWORLDMODELALPHA;
	refdef.num_entities = 1;
	refdef.entities = &entity;

	re.RenderFrame (&refdef);
}

/*
================
SCR_DrawLeaderModel

kingpin.exe @00413110: the current leader's player model for the "leader"
layout command. It idles and now and then plays one of three gestures.
================
*/
static int leader_anims[4][2] = {{0, 31}, {37, 46}, {47, 62}, {63, 77}};

static void SCR_DrawLeaderModel (int x, int y, clientinfo_t *ci, int w, int h)
{
	static int	lasttime;
	static int	frame, oldframe;
	static int	anim_start, anim_end = 31;
	refdef_t	refdef;
	entity_t	entities[MAX_CLIENTMODELPARTS+1], *ent;
	vec3_t		forward, org;
	float		backlerp;
	int			i, j, n, r;

	memset (&refdef, 0, sizeof(refdef));
	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov (40, w, h);
	refdef.time = cls.realtime * 0.001;

	memset (entities, 0, sizeof(entities));

	// a frame every 200 ms
	if (!lasttime)
	{
		oldframe = anim_start;
		frame = anim_start + 1;
	}
	else if (cls.realtime/200 != lasttime/200)
	{
		oldframe = frame;
		frame++;
		if (frame > anim_end)
		{
			if (!anim_start && rand()%100 < 20)
			{
				r = rand()%3 + 1;
				anim_start = leader_anims[r][0];
				anim_end = leader_anims[r][1];
			}
			else
			{
				anim_start = leader_anims[0][0];
				anim_end = leader_anims[0][1];
			}
			frame = anim_start;
		}
	}
	lasttime = cls.realtime;
	backlerp = 1.0 - (cls.realtime*0.005 - cls.realtime/200);

	AngleVectors (cl.viewangles, forward, NULL, NULL);

	// the parts; like kingpin.exe the model and skin are taken from the
	// next filled slot (j) while the scan walks all slots (i)
	n = 0;
	for (i=0, j=0 ; i<MAX_CLIENTMODELPARTS ; i++)
	{
		if (!ci->model[i])
			continue;
		ent = &entities[n++];
		ent->model = ci->model[j];
		ent->skin = ci->skin[j];
		j++;
	}
	// and the weapon
	entities[n++].model = ci->weaponmodel[0];

	for (i=0 ; i<n ; i++)
	{
		ent = &entities[i];
		ent->skinnum = 0;
		ent->flags = RF_DEPTHHACK | RF_DIMLIGHT;
		ent->flags2 |= RF2_NOSHADOW | RF2_DIR_LIGHTS;
		ent->angles[1] = 200;
		VectorSet (ent->origin, 120, 8, -10);
		VectorCopy (ent->origin, ent->oldorigin);
		ent->frame = frame;
		ent->oldframe = oldframe;
		ent->backlerp = backlerp;

		VectorScale (forward, 128, org);
		SCR_SetHUDModelLights (ent, org, 384, 0.15f);
	}

	refdef.height += 4;
	refdef.rdflags = RDF_NOWORLDMODELALPHA;
	refdef.num_entities = n;
	refdef.entities = entities;

	re.RenderFrame (&refdef);
}

// per digit shifts of the slanted counters
static int hud_slant3[3][2] = {{0, 0}, {0, 3}, {0, 6}};
static int hud_slant4[4][2] = {{0, 0}, {0, 3}, {0, 6}, {0, 9}};
static int hud_slantup3[3][2] = {{0, 0}, {0, -3}, {0, -6}};
static int hud_slantup2[2][2] = {{0, 0}, {0, -3}};
static int hud_noslant[5][2];

/*
================
SCR_BlinkColor

red (2) and dark red (3) digits alternating about every quarter second
================
*/
static int SCR_BlinkColor (void)
{
	return ((cl.time/255) & 1) ? 2 : 3;
}

/*
================
SCR_TeamScoreColor

kingpin.exe teams/tscore: STAT_TEAMn_FLASH 0 and 1 = green, 2 = flashing red
================
*/
static int SCR_TeamScoreColor (int flash)
{
	if (flash == 2)
		return flash + ((cl.time/255) & 1);
	if (flash == 0)
		return 1;
	return flash;
}

/*
================
SCR_DrawArmorPart

one piece of the body armor figure (legs, body or head); 1024 is added to
the value for heavy armor
================
*/
static void SCR_DrawArmorPart (int x, int y, int value, char *part, float alpha)
{
	char	name[MAX_QPATH];

	if (value > 1024)
	{
		Com_sprintf (name, sizeof(name), "armor_hv_%s", part);
		SCR_HudPic (x, y, name, alpha);
		value -= 1024;
	}
	else if (value)
	{
		Com_sprintf (name, sizeof(name), "armor_lt_%s", part);
		SCR_HudPic (x, y, name, alpha);
	}

	// the colored overlay: green, yellow, red, flashing red
	if (value >= 75)
		Com_sprintf (name, sizeof(name), "armor_%cg", part[0]);
	else if (value >= 50)
		Com_sprintf (name, sizeof(name), "armor_%cy", part[0]);
	else if (value >= 25 || (value && !((cl.time/255) & 1)))
		Com_sprintf (name, sizeof(name), "armor_%cr", part[0]);
	else if (value)
		Com_sprintf (name, sizeof(name), "armor_%cr2", part[0]);
	else
		return;
	SCR_HudPic (x, y, name, 1.0);
}

/*
================
SCR_DrawHireling

kingpin.exe "hire": a hired gang member's health figure and order
================
*/
static void SCR_DrawHireling (int x, int y, int health, int order)
{
	char	name[MAX_QPATH];
	int		step;

	if (health == 100)
		step = 100;
	else if (health >= 90)
		step = 90;
	else if (health >= 10)
		step = health/10*10;
	else
		step = 0;
	if (step >= 10)
		Com_sprintf (name, sizeof(name), "h_man%i", step);
	else
		strcpy (name, "h_man00");
	SCR_HudPic (x, y, name, 0.8f);

	// 1 = attack, 2 = hold position, 3 = flashing attack
	if (order == 1 || (order == 3 && (cl.frame.serverframe & 2)))
		SCR_HudPic (x, y, "h_man_at", 0.8f);
	else if (order == 2)
		SCR_HudPic (x, y, "h_man_hold", 0.8f);
}

/*
================
SCR_ExecuteLayoutString

kingpin.exe @004135a0. Most placement is in 640x480 units scaled to the
screen. hud is the scr_hud detail level (STAT_FORCE_HUD raises it to 2).
Quirks of the original are kept: "pic" does nothing, "ym" sets x and a few
commands leave x and y at the values they drew with.
================
*/
static char *hud_enemy_talk[4] = {"pos", "negx", "neg", "posq"};
static char *hud_talk_suffix[6] = {"", "1", "2", "3", "2", "1"};

void SCR_ExecuteLayoutString (char *s, int hud)
{
	static int hide_hud_frames;
	int		x, y;
	int		value;
	char	*token;
	int		index;
	int		i;
	float	sx, sy;
	short	*stats;
	clientinfo_t	*ci;
	char	name[MAX_QPATH];
	char	string[80];
	vec3_t	color;
	int		w, h;
	float	alpha;

	sx = viddef.width * (1.0/640);
	sy = viddef.height * (1.0/480);
	stats = cl.frame.playerstate.stats;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	// suppress status and layout drawing while STAT_HIDE_HUD is set (unless
	// the end picture is fading in), then wait eight calls before drawing
	if (stats[STAT_HIDE_HUD] && !stats[STAT_ENDPIC])
	{
		hide_hud_frames = 8;
		return;
	}
	if (hide_hud_frames)
	{
		--hide_hud_frames;
		return;
	}

	x = 0;
	y = 0;
	if (stats[STAT_FORCE_HUD])
		hud = 2;

	while (s)
	{
		token = COM_Parse (&s);
		if (!strcmp(token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi(token);
			continue;
		}
		if (!strcmp(token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.width + atoi(token);
			continue;
		}
		if (!strcmp(token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			continue;
		}

		if (!strcmp(token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi(token);
			continue;
		}
		if (!strcmp(token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.height + atoi(token);
			continue;
		}
		if (!strcmp(token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			continue;
		}
		if (!strcmp(token, "xm"))
		{
			token = COM_Parse (&s);
			x = viddef.width/2 + atoi(token);
			continue;
		}
		if (!strcmp(token, "ym"))
		{	// sic: sets x
			token = COM_Parse (&s);
			x = viddef.height/2 + atoi(token);
			continue;
		}

		if (!strcmp(token, "pic"))
			continue;	// a no-op; the stat number that follows is skipped as an unknown command

		if (!strcmp(token, "leader"))
		{	// the player model and name of the client in the lead
			token = COM_Parse (&s);
			index = atoi(token);
			if (index >= MAX_CLIENTS || index < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[index];
			if (!ci->model[0])
				ci = &cl.baseclientinfo;
			SCR_DrawLeaderModel (0, 0, ci, viddef.width*2/3, viddef.height);

			Com_sprintf (string, sizeof(string), "Current Kingpin");
			VectorSet (color, 0.4f, 0.4f, 0.2f);
			DrawBigString (viddef.width/4 - (int)(strlen(string)/2)*10, 30, string, color);
			Com_sprintf (string, sizeof(string), "%s", ci->name);
			VectorSet (color, 1, 1, 1);
			DrawBigString (viddef.width/4 - (int)(strlen(string)/2)*10, viddef.height - 80, string, color);
			continue;
		}

		if (!strcmp(token, "ds"))
		{	// 1.21 indexed scoreboard row; dmstr remains the legacy text path.
			char playername[14], numbers[64], *separator;
			int values[3];

			token = COM_Parse (&s);
			if (strlen(token) < 3)
				Com_Error (ERR_DROP, "ds: invalid color");
			for (i = 0; i < 3; i++)
				color[i] = (token[i] - '0') * 0.1f;
			token = COM_Parse (&s);
			index = atoi(token);
			playername[0] = 0;
			if (index != 999)
			{
				if (index < 0 || index >= MAX_CLIENTS)
					Com_Error (ERR_DROP, "ds: client >= MAX_CLIENTS");
				separator = strchr (cl.configstrings[CS_PLAYERSKINS + index], '\\');
				if (separator)
				{
					int length = separator - cl.configstrings[CS_PLAYERSKINS + index];
					if (length > 13) length = 13;
					memcpy (playername, cl.configstrings[CS_PLAYERSKINS + index], length);
					playername[length] = 0;
				}
				else
					strcpy (playername, "(unknown)");
				while (strlen(playername) < 13)
					strcat (playername, " ");
			}
			for (i = 0; i < 3; i++)
			{
				token = COM_Parse (&s);
				values[i] = atoi(token);
			}
			Com_sprintf (numbers, sizeof(numbers), "%4i %4i %5i", values[0], values[1], values[2]);
			Com_sprintf (string, sizeof(string), "%s%s", playername, numbers);
			DrawBigString (x, y, string, color);
			continue;
		}

		if (!strcmp(token, "dmstr"))
		{	// "rgb" digits 1..9 give the color in tenths, then the text
			token = COM_Parse (&s);
			color[0] = (token[0] - '1') * 0.1;
			color[1] = (token[1] - '1') * 0.1;
			color[2] = (token[2] - '1') * 0.1;
			token = COM_Parse (&s);
			DrawBigString (x, y, token, color);
			continue;
		}

		if (!strcmp(token, "teampic"))
		{
			token = COM_Parse (&s);
			Com_sprintf (name, sizeof(name), "team%i_icon", atoi(token));
			SCR_HudPic (x + 80, y + 20, name, 0.9f);
			continue;
		}

		if (!strcmp(token, "teams"))
		{	// both team icons and scores at the right edge
			float	ty;
			int		flash;

			ty = (int)(sy * 260);
			SCR_HudPic (viddef.width - 37, (int)ty, "team1_icon", 0.75f);
			flash = stats[STAT_TEAM1_FLASH];
			if (flash != 1 || ((cl.time/255) & 1))	// 1 = flashing green
				SCR_DrawField (viddef.width - 117, (int)(ty + 4), SCR_TeamScoreColor (flash),
					5, stats[STAT_TEAM1_SCORE], NULL);

			ty += 92;
			SCR_HudPic (viddef.width - 37, (int)ty, "team2_icon", 0.75f);
			flash = stats[STAT_TEAM2_FLASH];
			if (flash != 1 || ((cl.time/255) & 1))
				SCR_DrawField (viddef.width - 117, (int)(ty + 4), SCR_TeamScoreColor (flash),
					5, stats[STAT_TEAM2_SCORE], NULL);
			continue;
		}

		if (!strcmp(token, "tscore"))
		{	// tscore <team 1|2>
			token = COM_Parse (&s);
			index = atoi(token);
			SCR_DrawField (x, y, SCR_TeamScoreColor (stats[STAT_FORCE_HUD+2+index]),
				5, stats[STAT_FORCE_HUD+index], NULL);
			continue;
		}

		if (!strcmp(token, "bagcash"))
		{	// money in the teamplay bag, with a spinning bag next to it
			value = stats[STAT_BAGCASH];
			i = (int)(sy * 32);
			SCR_DrawField ((int)(sx * 570), i, 1, 4, value, NULL);
			if (value < 10)
				x = 585;
			else if (value < 100)
				x = 570;
			else if (value < 1000)
				x = 555;
			else
				x = 540;
			SCR_DrawHUDModel ((int)(x * sx), i, "models/pu_icon/money/money_lg.md2", 32, 32);
			continue;
		}

		if (!strcmp(token, "client"))
		{	// draw a deathmatch client block
			int		score, ping, time;

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);		// unused
			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];
			if (!ci->model[0])
				ci = &cl.baseclientinfo;

			token = COM_Parse (&s);
			score = atoi(token);
			token = COM_Parse (&s);
			ping = atoi(token);
			token = COM_Parse (&s);
			time = atoi(token);

			DrawAltString (x+32, y, ci->name);
			DrawString (x+32, y+8,  "Score: ");
			DrawAltString (x+32+7*8, y+8,  va("%i", score));
			DrawString (x+32, y+16, va("Ping:  %i", ping));
			DrawString (x+32, y+24, va("Time:  %i", time));
			continue;
		}

		if (!strcmp(token, "ctf"))
		{	// draw a ctf client block
			int		score, ping;

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);
			token = COM_Parse (&s);
			ping = atoi(token);
			if (ping > 999)
				ping = 999;

			Com_sprintf (string, sizeof(string), "%3d %3d %-12.12s", score, ping, ci->name);
			if (value == cl.playernum)
				DrawAltString (x, y, string);
			else
				DrawString (x, y, string);
			continue;
		}

		if (!strcmp(token, "picn"))
		{	// draw a pic from a name
			token = COM_Parse (&s);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+23, y+23);
			SCR_HudPic (x, y, token, 0.75f);
			continue;
		}

		if (!strcmp(token, "picnote"))
		{	// a note with two lines of text
			x = -120 - (int)(sx * -325);
			y = -160 - (int)(sy * -300);
			SCR_HudPic (x, y, "note", 0.5f);
			token = COM_Parse (&s);
			DrawHUDString2 (CL_LocalizeNotebookText (token), x + 16, y + 27, 0, 0x80);
			token = COM_Parse (&s);
			DrawHUDString2 (CL_LocalizeNotebookText (token), x + 16, y + 75, 0, 0x80);
			continue;
		}

		if (!strcmp(token, "num"))
		{	// draw a number
			token = COM_Parse (&s);
			i = atoi(token);
			token = COM_Parse (&s);
			value = stats[atoi(token)];
			SCR_DrawField (x, y, 0, i, value, NULL);
			continue;
		}

		if (!strcmp(token, "hnum"))
		{	// health and the armor figure
			int		ay;

			value = stats[STAT_ENDPIC];
			if (value)
			{	// the end picture fades in over everything
				SCR_HudStretch (0, 0, viddef.width, viddef.height,
					"end", value == 100 ? 0 : value * 0.1);
				return;
			}

			value = stats[STAT_HEALTH];
			SCR_DrawField ((int)(sx * 63), (int)(sy * 448), value > 25 ? 0 : SCR_BlinkColor (),
				3, value, hud_slant3);

			x = (int)(sx * 20);
			y = (int)(sy * 384);
			SCR_HudPic (x, y, "armor", 0.75f);
			ay = (int)(sy * -384);

			// the counters go at the left edge; sic: x is (int)sx
			SCR_DrawArmorPart (x, y, stats[STAT_ARMOR3], "l", 0.75f);
			value = stats[STAT_ARMOR3];
			if (value > 1024)
				value -= 1024;
			SCR_DrawScaledField ((int)sx, 55 - ay, 0, 3, value, 0.5);

			SCR_DrawArmorPart (x, y, stats[STAT_ARMOR2], "b", 1.0);
			value = stats[STAT_ARMOR2];
			if (value > 1024)
				value -= 1024;
			SCR_DrawScaledField ((int)sx, 35 - ay, 0, 3, value, 0.5);

			SCR_DrawArmorPart (x, y, stats[STAT_ARMOR1], "h", 1.0);
			value = stats[STAT_ARMOR1];
			if (value > 1024)
				value -= 1024;
			SCR_DrawScaledField ((int)sx, 15 - ay, 0, 3, value, 0.5);

			x = (int)sx;
			y = 15 - ay;
			continue;
		}

		if (!strcmp(token, "frags"))
		{	// cash in deathmatch
			value = stats[STAT_CASH];
			SCR_DrawField ((int)(sy * 570), (int)(sy * 6), value >= 0 ? 1 : SCR_BlinkColor (),
				4, value, hud_slant4);
			continue;
		}

		if (!strcmp(token, "cnum"))
		{	// cash, notepad and the inventory items (full hud only)
			int		iy;

			if (hud < 2)
				continue;

			iy = (int)(sy * 35);
			if (stats[STAT_HELPICON])
				SCR_HudPic ((int)(sx * 630 - 40), (int)(sy * 375),
					"h_notepad3", stats[STAT_HELPICON] * 0.0025);

			value = stats[STAT_CASH];
			if (value >= 0 && value <= 9999)
			{
				SCR_DrawField ((int)(sy * 570), (int)(sy * 6), 1, 4, value, hud_slant4);
				if (value < 10)
					x = 585;
				else if (value < 100)
					x = 570;
				else if (value < 1000)
					x = 555;
				else
					x = 540;
				SCR_HudPic ((int)(x * sx), (int)(sy * 3), "h_money", 0.25f);
			}
			else
			{	// broke or rich: the money burns
				SCR_HudPic ((int)(sx * 540), (int)(sy * 3), "h_money", 1.0);
				for (i=0 ; i<2 ; i++)
				{
					h = (int)(((rand()&15) - 64) * sy);
					SCR_HudPic ((int)(((rand()&7) - 4) * sx + 625), h, "/sprites/fire.tga", 0.5f);
				}
			}

			x = (int)(sx * 600);
			y = (int)(sy * 10);
			value = stats[STAT_HUD_INV];
			for (i=0 ; i<10 ; i++)
			{
				static char *items[10] = {"bat", "coil", "watch", "safedocs", "fuse",
					"valve", "lizzyhead", "bottle", "oilcan", "ticket"};

				if (!(value & (1<<i)))
					continue;
				y += iy;
				Com_sprintf (name, sizeof(name), "h_%s", items[i]);
				re.DrawGetPicSize (&w, &h, name);
				SCR_HudStretch (x, y, (int)(w*0.6), (int)(h*0.6), name, 0.75f);
			}
			continue;
		}

		if (!strcmp(token, "hire"))
		{	// up to two hired gang members
			if (!hudguys->value)
				continue;
			if (stats[STAT_HUD_HIRE1])
				SCR_DrawHireling ((int)(sx * 482), (int)(sy * 417),
					stats[STAT_HUD_HIRE1], stats[STAT_HUD_HIRE1_CMD]);
			x = 514;
			y = 417;
			if (stats[STAT_HUD_HIRE2])
				SCR_DrawHireling ((int)(sx * 514), (int)(sy * 417),
					stats[STAT_HUD_HIRE2], stats[STAT_HUD_HIRE2_CMD]);
			continue;
		}

		if (!strcmp(token, "talk"))
		{	// talk faces: the other character's animated one and our own
			if (!hudtalk->value)
				continue;

			alpha = 0;
			value = stats[STAT_HUD_ENEMY_TALK];
			if (value)
				alpha = stats[STAT_HUD_ENEMY_TALK_TIME] * (1.0/255) * 0.6;
			if (value >= 1 && value <= 4)
			{	// six frames, two server frames each
				i = (cl.frame.serverframe >> 1) % 6;
				Com_sprintf (name, sizeof(name), "head_%s%s", hud_enemy_talk[value-1], hud_talk_suffix[i]);
				SCR_HudPic ((int)(sx * 168), (int)(sy * 438), name, alpha);
			}

			value = stats[STAT_HUD_SELF_TALK];
			if (value)
				alpha = stats[STAT_HUD_SELF_TALK_TIME] * (1.0/255);
			x = 118;
			y = 438;
			switch (value)
			{
			case 1:
			case 8:
				SCR_HudPic ((int)(sx * 118), (int)(sy * 438), "pos", alpha);
				break;
			case 3:
			case 7:
				SCR_HudPic ((int)(sx * 118), (int)(sy * 436), "neg", alpha);
				break;
			case 5:
				SCR_HudPic ((int)(sx * 118), (int)(sy * 438), "neutral", alpha);
				break;
			case 9:
				break;
			default:
				continue;
			}
			SCR_HudPic ((int)(sx * 118), (int)(sy * 438), "head", alpha * 0.5);
			continue;
		}

		if (!strcmp(token, "anum"))
		{	// ammo count and the ammo icon from the given stat
			value = stats[STAT_AMMO];
			if (value < 0)
				continue;
			x = (int)(sx * 562);
			y = (int)(sy * 453);
			SCR_DrawField (x, y, value > 5 ? 0 : SCR_BlinkColor (), 3, value, hud_slantup3);

			token = COM_Parse (&s);
			index = stats[atoi(token)];
			if (index >= MAX_IMAGES)
				Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");
			token = cl.configstrings[CS_IMAGES+index];
			re.DrawGetPicSize (&w, &h, token);
			SCR_HudStretch ((int)(sx * 606), (int)(sy * 438),
				(int)(w*0.75), (int)(h*0.75), token, 0.75f);
			continue;
		}

		if (!strcmp(token, "wanum"))
		{	// rounds left in the clip
			value = stats[STAT_CLIP];
			if (value < 0)
				continue;
			SCR_DrawField ((int)(sx * 598), (int)(sy * 414), value > 1 ? 0 : SCR_BlinkColor (),
				2, value, hud_slantup2);
			continue;
		}

		if (!strcmp(token, "stat_string"))
		{	// kingpin: the picked up item's icon and count
			token = COM_Parse (&s);
			index = atoi(token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			index = stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");

			token = COM_Parse (&s);
			index = stats[atoi(token)];
			if (index >= MAX_IMAGES)
				Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");
			token = cl.configstrings[CS_IMAGES+index];
			re.DrawGetPicSize (&w, &h, token);
			SCR_HudStretch ((int)(sx * 302), (int)(sy * 438),
				(int)(w*0.8), (int)(h*0.8), token, stats[STAT_PICKUP_COUNT] * (1.0/255));

			if (stats[STAT_CASH_PICKUP])
				SCR_DrawField ((int)(sx * 335), (int)(sy * 445), 1, 3, stats[STAT_CASH_PICKUP], hud_noslant);
			continue;
		}

		if (!strcmp(token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, 0);
			continue;
		}

		if (!strcmp(token, "string"))
		{
			token = COM_Parse (&s);
			DrawString (x, y, token);
			continue;
		}

		if (!strcmp(token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320,0x80);
			continue;
		}

		if (!strcmp(token, "string2"))
		{
			token = COM_Parse (&s);
			DrawAltString (x, y, token);
			continue;
		}

		if (!strcmp(token, "if"))
		{	// draw a number
			token = COM_Parse (&s);
			value = stats[atoi(token)];
			if (!value)
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}

			continue;
		}
	}
}
#else
/*
================
SCR_ExecuteLayoutString

================
*/
void SCR_ExecuteLayoutString (char *s)
{
	int		x, y;
	int		value;
	char	*token;
	int		width;
	int		index;
	clientinfo_t	*ci;

	if (cls.state != ca_active || !cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	x = 0;
	y = 0;
	width = 3;

	while (s)
	{
		token = COM_Parse (&s);
		if (!strcmp(token, "xl"))
		{
			token = COM_Parse (&s);
			x = atoi(token);
			continue;
		}
		if (!strcmp(token, "xr"))
		{
			token = COM_Parse (&s);
			x = viddef.width + atoi(token);
			continue;
		}
		if (!strcmp(token, "xv"))
		{
			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			continue;
		}

		if (!strcmp(token, "yt"))
		{
			token = COM_Parse (&s);
			y = atoi(token);
			continue;
		}
		if (!strcmp(token, "yb"))
		{
			token = COM_Parse (&s);
			y = viddef.height + atoi(token);
			continue;
		}
		if (!strcmp(token, "yv"))
		{
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			continue;
		}

		if (!strcmp(token, "pic"))
		{	// draw a pic from a stat number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (value >= MAX_IMAGES)
				Com_Error (ERR_DROP, "Pic >= MAX_IMAGES");
			if (cl.configstrings[CS_IMAGES+value])
			{
				SCR_AddDirtyPoint (x, y);
				SCR_AddDirtyPoint (x+23, y+23);
				re.DrawPic (x, y, cl.configstrings[CS_IMAGES+value]);
			}
			continue;
		}

		if (!strcmp(token, "client"))
		{	// draw a deathmatch client block
			int		score, ping, time;

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);

			token = COM_Parse (&s);
			time = atoi(token);

			DrawAltString (x+32, y, ci->name);
			DrawString (x+32, y+8,  "Score: ");
			DrawAltString (x+32+7*8, y+8,  va("%i", score));
			DrawString (x+32, y+16, va("Ping:  %i", ping));
			DrawString (x+32, y+24, va("Time:  %i", time));

			if (!ci->icon)
				ci = &cl.baseclientinfo;
			re.DrawPic (x, y, ci->iconname);
			continue;
		}

		if (!strcmp(token, "ctf"))
		{	// draw a ctf client block
			int		score, ping;
			char	block[80];

			token = COM_Parse (&s);
			x = viddef.width/2 - 160 + atoi(token);
			token = COM_Parse (&s);
			y = viddef.height/2 - 120 + atoi(token);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+159, y+31);

			token = COM_Parse (&s);
			value = atoi(token);
			if (value >= MAX_CLIENTS || value < 0)
				Com_Error (ERR_DROP, "client >= MAX_CLIENTS");
			ci = &cl.clientinfo[value];

			token = COM_Parse (&s);
			score = atoi(token);

			token = COM_Parse (&s);
			ping = atoi(token);
			if (ping > 999)
				ping = 999;

			sprintf(block, "%3d %3d %-12.12s", score, ping, ci->name);

			if (value == cl.playernum)
				DrawAltString (x, y, block);
			else
				DrawString (x, y, block);
			continue;
		}

		if (!strcmp(token, "picn"))
		{	// draw a pic from a name
			token = COM_Parse (&s);
			SCR_AddDirtyPoint (x, y);
			SCR_AddDirtyPoint (x+23, y+23);
			re.DrawPic (x, y, token);
			continue;
		}

		if (!strcmp(token, "num"))
		{	// draw a number
			token = COM_Parse (&s);
			width = atoi(token);
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			SCR_DrawField (x, y, 0, width, value);
			continue;
		}

		if (!strcmp(token, "hnum"))
		{	// health number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_HEALTH];
			if (value > 25)
				color = 0;	// green
			else if (value > 0)
				color = (cl.frame.serverframe>>2) & 1;		// flash
			else
				color = 1;

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
				re.DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(token, "anum"))
		{	// ammo number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_AMMO];
			if (value > 5)
				color = 0;	// green
			else if (value >= 0)
				color = (cl.frame.serverframe>>2) & 1;		// flash
			else
				continue;	// negative number = don't show

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
				re.DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}

		if (!strcmp(token, "rnum"))
		{	// armor number
			int		color;

			width = 3;
			value = cl.frame.playerstate.stats[STAT_ARMOR];
			if (value < 1)
				continue;

			color = 0;	// green

			if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
				re.DrawPic (x, y, "field_3");

			SCR_DrawField (x, y, color, width, value);
			continue;
		}


		if (!strcmp(token, "stat_string"))
		{
			token = COM_Parse (&s);
			index = atoi(token);
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			index = cl.frame.playerstate.stats[index];
			if (index < 0 || index >= MAX_CONFIGSTRINGS)
				Com_Error (ERR_DROP, "Bad stat_string index");
			DrawString (x, y, cl.configstrings[index]);
			continue;
		}

		if (!strcmp(token, "cstring"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320, 0);
			continue;
		}

		if (!strcmp(token, "string"))
		{
			token = COM_Parse (&s);
			DrawString (x, y, token);
			continue;
		}

		if (!strcmp(token, "cstring2"))
		{
			token = COM_Parse (&s);
			DrawHUDString (token, x, y, 320,0x80);
			continue;
		}

		if (!strcmp(token, "string2"))
		{
			token = COM_Parse (&s);
			DrawAltString (x, y, token);
			continue;
		}

		if (!strcmp(token, "if"))
		{	// draw a number
			token = COM_Parse (&s);
			value = cl.frame.playerstate.stats[atoi(token)];
			if (!value)
			{	// skip to endif
				while (s && strcmp(token, "endif") )
				{
					token = COM_Parse (&s);
				}
			}

			continue;
		}


	}
}
#endif


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
#ifdef KINGPIN_ENGINE
	// kingpin.exe @00415f60: scr_hud 0 suppresses the status-bar layout,
	// otherwise it is the detail level passed to the interpreter.
	if (!scr_hud->value)
		return;
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR], (int)scr_hud->value);
#else
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR]);
#endif
}


/*
================
SCR_DrawLayout

================
*/
#define	STAT_LAYOUTS		13

void SCR_DrawLayout (void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;
#ifdef KINGPIN_ENGINE
	SCR_ExecuteLayoutString (cl.layout, (int)scr_hud->value);	// @00415f90
#else
	SCR_ExecuteLayoutString (cl.layout);
#endif
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	int numframes;
	int i;
	float separation[2] = { 0, 0 };

	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf ("Loading plaque timed out.\n");
		}
		return;
	}

#ifdef KINGPIN_ENGINE
	if (com_stoprender)
		scr_draw_loading = 2;
#endif

	if (!scr_initialized || !con.initialized)
		return;				// not initialized yet

	/*
	** range check cl_camera_separation so we don't inadvertently fry someone's
	** brain
	*/
	if ( cl_stereo_separation->value > 1.0 )
		Cvar_SetValue( "cl_stereo_separation", 1.0 );
	else if ( cl_stereo_separation->value < 0 )
		Cvar_SetValue( "cl_stereo_separation", 0.0 );

	if ( cl_stereo->value )
	{
		numframes = 2;
		separation[0] = -cl_stereo_separation->value / 2;
		separation[1] =  cl_stereo_separation->value / 2;
	}
	else
	{
		separation[0] = 0;
		separation[1] = 0;
		numframes = 1;
	}

	for ( i = 0; i < numframes; i++ )
	{
		re.BeginFrame( separation[i] );

		if (scr_draw_loading == 2)
		{	//  loading plaque over black screen
			int		w, h;

			re.CinematicSetPalette(NULL);
			scr_draw_loading = false;
#ifdef KINGPIN_ENGINE
			re.DrawStretchPic (0, 0, viddef.width, viddef.height, "conback");
			re.DrawGetPicSize (&w, &h, "processing");
			re.DrawPic ((viddef.width-w)/2, (viddef.height-h)/2, "processing");
#else
			re.DrawGetPicSize (&w, &h, "loading");
			re.DrawPic ((viddef.width-w)/2, (viddef.height-h)/2, "loading");
#endif
//			re.EndFrame();
//			return;
		}
		// if a cinematic is supposed to be running, handle menus
		// and console specially
		else if (cl.cinematictime > 0)
		{
			if (cls.key_dest == key_menu)
			{
				if (cl.cinematicpalette_active)
				{
					re.CinematicSetPalette(NULL);
					cl.cinematicpalette_active = false;
				}
				M_Draw ();
				OSK_Draw ();
//				re.EndFrame();
//				return;
			}
			else if (cls.key_dest == key_console)
			{
				if (cl.cinematicpalette_active)
				{
					re.CinematicSetPalette(NULL);
					cl.cinematicpalette_active = false;
				}
				SCR_DrawConsole ();
				OSK_Draw ();
//				re.EndFrame();
//				return;
			}
			else
			{
				SCR_DrawCinematic();
//				re.EndFrame();
//				return;
			}
		}
		else
		{

			// make sure the game palette is active
			if (cl.cinematicpalette_active)
			{
				re.CinematicSetPalette(NULL);
				cl.cinematicpalette_active = false;
			}

			// do 3D refresh drawing, and then update the screen
			SCR_CalcVrect ();

			// clear any dirty part of the background
			SCR_TileClear ();

			V_RenderView ( separation[i] );

			SCR_DrawStats ();
			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 1)
				SCR_DrawLayout ();
			if (cl.frame.playerstate.stats[STAT_LAYOUTS] & 2)
				CL_DrawInventory ();

			SCR_DrawNet ();
			SCR_CheckDrawCenterString ();

			if (scr_timegraph->value)
				SCR_DebugGraph (cls.frametime*300, 0);

			if (scr_debuggraph->value || scr_timegraph->value || scr_netgraph->value)
				SCR_DrawDebugGraph ();

			if(scr_drawfps->value)
				SCR_DrawFPS ();

			SCR_DrawPause ();

			SCR_DrawConsole ();

			M_Draw ();

			OSK_Draw ();

			SCR_DrawLoading ();
		}
	}
	re.EndFrame();
}
