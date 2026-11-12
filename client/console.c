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
// console.c

#include "client.h"

console_t	con;

cvar_t		*con_notifytime;
#ifdef KINGPIN_ENGINE
 cvar_t	*con_scale;
 cvar_t	*con_highres;
 cvar_t	*con_autosize;
float	con_scale_value = 1.0f;
#endif

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

void DrawString (int x, int y, char *s)
{
	while (*s)
	{
		re.DrawChar (x, y, *s);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, char *s)
{
	while (*s)
	{
		re.DrawChar (x, y, *s ^ 0x80);
		x+=8;
		s++;
	}
}


void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
#ifdef KINGPIN_ENGINE
	// kingpin.exe @00421bb0: the console is only available with developer set
	//if (!developer->value)
	//	return;
#endif
	SCR_EndLoadingPlaque ();	// get rid of loading plaque

	if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	if (cls.state == ca_disconnected)
	{	// start the demo loop again
		Cbuf_AddText ("d1\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff ();
		Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;	

		if (Cvar_VariableValue ("maxclients") == 1 
			&& Com_ServerState ())
			Cvar_Set ("paused", "1");
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	memset (con.text, ' ', CON_TEXTSIZE);
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	file_t	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Com_sprintf (name, sizeof(name), "%s.txt", Cmd_Argv(1));

	Com_Printf ("Dumped console text to %s.\n", name);
	f = FS_FOpen (name, FS_MODE_WRITE | FS_PATH_GAMEDIR | FS_FLAG_TEXT);
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		strncpy (buffer, line, con.linewidth);
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		FS_FPrintf (f, "%s\n", buffer);
	}

	FS_FClose (f);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con.times[i] = 0;
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

#ifdef KINGPIN_ENGINE
	/* kingpin.exe @00421ea0 */
	if (con_autosize && con_autosize->value &&
		con_highres && con_highres->value)
	{
		con_scale_value = viddef.width * (1.0f / 512.0f);
	}
	else if (con_scale && con_highres && con_highres->value)
	{
		con_scale_value = con_scale->value;
		if (con_scale_value < 1.0f)
			con_scale_value = 1.0f;
	}
	else
	{
		con_scale_value = 1.0f;
	}

	width = (int)((viddef.width >> 3) / con_scale_value) - 2;
#else
	width = (viddef.width >> 3) - 2;
#endif

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		memset (con.text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE);
		memset (con.text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
#ifdef KINGPIN_ENGINE
	cvar_t *gl_driver;
	char *autosize_default;
#endif
	con.linewidth = -1;

	Con_CheckResize ();
	
	Com_Printf ("Console initialized.\n");

//
// register our commands
//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);
#ifdef KINGPIN_ENGINE
	gl_driver = Cvar_Get ("gl_driver", "", 0);
	con_scale = Cvar_Get ("con_scale", "1", CVAR_ARCHIVE);
	con_highres = Cvar_Get ("con_highres", "0", CVAR_ARCHIVE);
	autosize_default = gl_driver->string[0] == '3' ? "0" : "1";
	con_autosize = Cvar_Get ("con_autosize", autosize_default, 0);
#endif

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	memset (&con.text[(con.current%con.totallines)*con.linewidth]
	, ' ', con.linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (!con.initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con.linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con.linewidth && (con.x + l > con.linewidth) )
			con.x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		
		if (!con.x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = c | mask | con.ormask;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
			break;
		}
		
	}
}


/*
==============
Con_CenteredPrint
==============
*/
void Con_CenteredPrint (char *text)
{
	int		l;
	char	buffer[1024];

	l = strlen(text);
	l = (con.linewidth-l)/2;
	if (l < 0)
		l = 0;
	memset (buffer, ' ', l);
	strcpy (buffer+l, text);
	strcat (buffer, "\n");
	Con_Print (buffer);
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		y;
	int		i;
	char	*text;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	text = key_lines[edit_line];
	
// add the cursor frame
	text[key_linepos] = 10+((int)(cls.realtime>>8)&1);
	
// fill out remainder with spaces
	for (i=key_linepos+1 ; i< con.linewidth ; i++)
		text[i] = ' ';
		
//	prestep if horizontally scrolling
	if (key_linepos >= con.linewidth)
		text += 1 + key_linepos - con.linewidth;
		
// draw it
	y = con.vislines-16;

	for (i=0 ; i<con.linewidth ; i++)
		re.DrawChar ( (i+1)<<3, con.vislines - 22, text[i]);

// remove cursor
	key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
#ifdef KINGPIN_ENGINE
// 1.21 @00423e80: separate wrapped chat history, twice notify duration,
// eight rows above a 180/640 screen-width baseline, fading during second half.
static int Con_ChatRows (char *text, int soft, int hard)
{
	int x = 0, rows = 1, i;
	for (i=0; i<con.linewidth; i++)
	{
		x++;
		if (i < con.linewidth-1 && (text[i+1]&127) != ' ' &&
			(x > hard || (x > soft && (text[i]&127) == ' ')))
		{
			rows++;
			x = 1;
		}
	}
	return rows;
}

static void Con_DrawChat (void)
{
	float scale = viddef.width / 640.0f, duration = con_notifytime->value * 1000;
	float color[3], alpha;
	int first = con.current, rows = 0, i, n, x, y, step, soft, hard, age;
	char *text;
	if (duration <= 0 || scale <= 0)
		return;
	soft = (int)(24 * scale); if (soft < 1) soft = 1;
	hard = (int)(30 * scale); if (hard < soft) hard = soft;
	step = (int)(9 * scale); if (step < 1) step = 1;
	for (i=con.current; i>=0 && i>con.current-NUM_CON_TIMES && rows<8; i--)
	{
		age = cls.realtime - con.times[i % NUM_CON_TIMES];
		text = con.text + (i % con.totallines) * con.linewidth;
		if (con.times[i % NUM_CON_TIMES] && age <= 2 * duration && (byte)text[0] >= 128)
		{
			rows += Con_ChatRows (text, soft, hard);
			first = i;
		}
	}
	y = (int)(180 * scale) - rows * step;
	for (i=first; rows && i<=con.current; i++)
	{
		age = cls.realtime - con.times[i % NUM_CON_TIMES];
		text = con.text + (i % con.totallines) * con.linewidth;
		if (!con.times[i % NUM_CON_TIMES] || age > 2*duration || (byte)text[0] < 128)
			continue;
		VectorSet (color, 1, 1, 1);
		alpha = 0.5f;
		if ((text[0]&127) == '>') {color[2] = 0.5f; alpha = 1;}
		if (age > duration) alpha *= 2 - age / duration;
		x = 0;
		for (n=0; n<con.linewidth; n++)
		{
			x++;
			if (n < con.linewidth-1 && (text[n+1]&127) != ' ' &&
				(x > hard || (x > soft && (text[n]&127) == ' ')))
			{
				y += step;
				x = 1;
			}
			if (!n && (text[n]&127) == ':') {x--; continue;}
			if (re.DrawChatChar)
				re.DrawChatChar (x*step, y, (byte)text[n], color, scale, alpha);
			else if (alpha > 0)
				re.DrawChar (x*step, y, (byte)text[n]);
		}
		y += step;
	}
	if (rows)
	{
		SCR_AddDirtyPoint (0, 0);
		SCR_AddDirtyPoint (viddef.width-1, (int)(191*scale));
	}
}
#endif

void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	int		time;
	char	*s;
	int		skip;

	v = 0;
	for (i= con.current-3 ; i<=con.current ; i++)
	{
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->value*1000)
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;
#ifdef KINGPIN_ENGINE
		if (!cl_oldchat->value && (byte)text[0] >= 128)
			continue;
#endif
		
		for (x = 0 ; x < con.linewidth ; x++)
			re.DrawChar ( (x+1)<<3, v, text[x]);

		v += 8;
	}


#ifdef KINGPIN_ENGINE
	if (!cl_oldchat->value)
		Con_DrawChat ();
#endif
	if (cls.key_dest == key_message)
	{
#ifdef KINGPIN_ENGINE
		float color[3] = {1, 1, 1};
		char *prompt = chat_team ? "say_team:" : "say:";
		int visible;
		skip = chat_team ? 9 : 5;
		for (x=0; prompt[x]; x++)
		{
			if (re.DrawChatChar)
				re.DrawChatChar (8+x*12, v, prompt[x], color, 1, 1);
			else
				re.DrawChar (8+x*12, v, prompt[x]);
		}
		visible = viddef.width/12 - skip - 1;
		if (visible < 0) visible = 0;
		s = chat_buffer;
		if (chat_bufferlen > visible) s += chat_bufferlen-visible;
		for (x=0; s[x]; x++)
		{
			if (re.DrawChatChar)
				re.DrawChatChar ((x+skip)*12, v, (byte)s[x], color, 1, 1);
			else
				re.DrawChar ((x+skip)*12, v, (byte)s[x]);
		}
		re.DrawChar ((x+skip)*12, v, 10+((cls.realtime>>8)&1));
		v += 18;
#else
		if (chat_team)
		{
			DrawString (8, v, "say_team:");
			skip = 11;
		}
		else
		{
			DrawString (8, v, "say:");
			skip = 5;
		}

		s = chat_buffer;
		if (chat_bufferlen > (viddef.width>>3)-(skip+1))
			s += chat_bufferlen - ((viddef.width>>3)-(skip+1));
		x = 0;
		while(s[x])
		{
			re.DrawChar ( (x+skip)<<3, v, s[x]);
			x++;
		}
		re.DrawChar ( (x+skip)<<3, v, 10+((cls.realtime>>8)&1));
		v += 8;
#endif
	}
	
	if (v)
	{
		SCR_AddDirtyPoint (0,0);
		SCR_AddDirtyPoint (viddef.width-1, v);
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac)
{
	int				i, j, x, y, n;
	int				rows;
	char			*text;
	int				row;
	int				lines;
	char			version[64];
	char			dlbar[1024];

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

// draw the background
	re.DrawStretchPic (0, -viddef.height+lines, viddef.width, viddef.height, "conback");
	SCR_AddDirtyPoint (0,0);
	SCR_AddDirtyPoint (viddef.width-1,lines-1);

#ifdef EXT_VERSION
#ifdef KINGPIN_ENGINE
	Com_sprintf (version, sizeof(version), "%s KP v%4.2f", EXT_VERSION, VERSION);
	i = strlen(version);
	for (x = 0; x < i; x++)
		re.DrawChar ((int)(viddef.width / con_scale_value - ((i + 1 - x) << 3)), 
			(int)(lines / con_scale_value - con_scale_value * 4 - 8),
			128 + version[x]);
#else
	Com_sprintf (version, sizeof(version), "%s Q2 v%4.2f", EXT_VERSION, VERSION);
	i = strlen(version);
	for (x = 0; x < i; x++)
		re.DrawChar (viddef.width - ((i + 1 - x) << 3), lines-12, 128 + version[x]);
#endif
#else
	Com_sprintf (version, sizeof(version), "v%4.2f", VERSION);
#ifdef KINGPIN_ENGINE
	for (x=0 ; x<5 ; x++)
	{
		re.DrawChar ((int)(viddef.width / con_scale_value -
			con_scale_value * 20 - 24 + x * 8),
			(int)(lines / con_scale_value - con_scale_value * 4 - 8),
			128 + version[x]);
	}
#else
	for (x=0 ; x<5 ; x++)
		re.DrawChar (viddef.width-44+x*8, lines-12, 128 + version[x] );
#endif
#endif

// draw the text
#ifdef KINGPIN_ENGINE
	con.vislines = (int)(lines / con_scale_value);
#else
	con.vislines = lines;
#endif
	
#if 0
	rows = (lines-8)>>3;		// rows of text to draw

	y = lines - 24;
#else
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;
#endif

// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con.linewidth ; x+=4)
			re.DrawChar ( (x+1)<<3, y, '^');
	
		y -= 8;
		rows--;
	}
	
	row = con.display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point
			
		text = con.text + (row % con.totallines)*con.linewidth;

		for (x=0 ; x<con.linewidth ; x++)
			re.DrawChar ( (x+1)<<3, y, text[x]);
	}

//ZOID
	// draw the download bar
	// figure out width
	if (cls.download) {
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - strlen(text) - 8;
		i = con.linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			strncpy(dlbar, text, i);
			dlbar[i] = 0;
			strcat(dlbar, "...");
		} else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		sprintf(dlbar + strlen(dlbar), " %02d%%", cls.downloadpercent);

		// draw it
		y = con.vislines-12;
		for (i = 0; i < strlen(dlbar); i++)
			re.DrawChar ( (i+1)<<3, y, dlbar[i]);
	}
//ZOID

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}


