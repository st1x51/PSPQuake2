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

#ifdef __psp__
#include <psputility_netparam.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif
#include "client.h"
#include "../client/qmenu.h"
#include "../client/menu_ru.h"

static int	m_main_cursor;

#define NUM_CURSOR_FRAMES 15

#ifdef KINGPIN_ENGINE
// kingpin.exe @00459ce0
static char *menu_in_sound		= "misc/menu1.wav";
static char *menu_move_sound	= "misc/menu1.wav";
static char *menu_back_sound	= "misc/menu1.wav";
static char *menu_out_sound		= "misc/menu3.wav";
static char *menu_ambient_sound	= "misc/menuamb.wav";

qboolean	m_mainmenu_active;
#else
static char *menu_in_sound		= "misc/menu1.wav";
static char *menu_move_sound	= "misc/menu2.wav";
static char *menu_out_sound		= "misc/menu3.wav";
#endif

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

#ifdef KINGPIN_ENGINE
#define M_Banner(x)
#else
static void M_Banner( char *name )
{
	int w, h;

	re.DrawGetPicSize (&w, &h, name );
	re.DrawPic( viddef.width / 2 - w / 2, viddef.height / 2 - 110, name );
}
#endif

void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (Cvar_VariableValue ("maxclients") == 1
		&& Com_ServerState ())
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i=0 ; i<m_menudepth ; i++)
		if (m_layers[i].draw == draw &&
			m_layers[i].key == key)
		{
			m_menudepth = i;
		}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

void M_ForceMenuOff (void)
{
	m_drawfunc = 0;
	m_keyfunc = 0;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates ();
	Cvar_Set ("paused", "0");
#ifdef KINGPIN_ENGINE
	m_mainmenu_active = false;
#endif
}

void M_PopMenu (void)
{
#ifdef KINGPIN_ENGINE
	// kingpin.exe @00428560: the back sound, and the out sound only when
	// the last menu closes
	S_StartLocalSound( menu_back_sound );
#else
	S_StartLocalSound( menu_out_sound );
#endif
	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");
	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
	{
		M_ForceMenuOff ();
#ifdef KINGPIN_ENGINE
		S_StartLocalSound( menu_out_sound );
#endif
	}
}


const char *Default_MenuKey( menuframework_s *m, int key )
{
	const char *sound = NULL;
	menucommon_s *item;

	if ( m )
	{
		if ( ( item = Menu_ItemAtCursor( m ) ) != 0 )
		{
			if ( item->type == MTYPE_FIELD )
			{
				if ( Field_Key( ( menufield_s * ) item, key ) )
					return NULL;
			}
		}
	}

	switch ( key )
	{
	case K_ESCAPE:
	case K_START_BUTTON:
	case K_B_BUTTON:
		M_PopMenu();
#ifdef KINGPIN_ENGINE
		return menu_back_sound;
#else
		return menu_out_sound;
#endif
	case K_KP_UPARROW:
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_JOY1:
	case K_JOY2:
	case K_JOY3:
	case K_JOY4:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:

	case K_KP_ENTER:
	case K_ENTER:
	case K_A_BUTTON:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
#ifdef KINGPIN_ENGINE
	re.DrawCharUpper ( cx + ((viddef.width - 320)>>1), cy + ((viddef.height - 240)>>1), num);
#else
	re.DrawChar ( cx + ((viddef.width - 320)>>1), cy + ((viddef.height - 240)>>1), num);
#endif
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawPic (int x, int y, char *pic)
{
	re.DrawPic (x + ((viddef.width - 320)>>1), y + ((viddef.height - 240)>>1), pic);
}


/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y.  The pic will extend to the left of x,
and both above and below y.
=============
*/
void M_DrawCursor( int x, int y, int f )
{
	char	cursorname[80];
	static qboolean cached;

	if ( !cached )
	{
		int i;

		for ( i = 0; i < NUM_CURSOR_FRAMES; i++ )
		{
			Com_sprintf( cursorname, sizeof( cursorname ), "m_cursor%d", i );

			re.RegisterPic( cursorname );
		}
		cached = true;
	}

	Com_sprintf( cursorname, sizeof(cursorname), "m_cursor%d", f );
	re.DrawPic( x, y, cursorname );
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 4);
	}
	M_DrawCharacter (cx, cy+8, 7);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx, cy, 5);
		}
		M_DrawCharacter (cx, cy+8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 6);
	}
	M_DrawCharacter (cx, cy+8, 9);
}

/*
=============
M_DrawGraph
=============
*/
void M_DrawGraph (int x, int y, int width, int height, float min, float max, int graphsize, const float *graph)
{
	int		i;
	float	yscale;
	uint	frac, fracstep;
	int		x0, y0, x1, y1;

	// fill box
	re.DrawFill (x, y, width, height, 40);

	// up / buttom lines
	re.DrawLine (x, y, x + width, y, 152);
	re.DrawLine (x + width, y + height, x, y + height, 152);

	// left / right lines
	re.DrawLine (x, y + height, x, y, 152);
	re.DrawLine (x + width, y, x + width, y + height, 152);

	// center lines
	re.DrawLine (x + width * 0.5, y, x + width * 0.5, y + height, 152);
	re.DrawLine (x, y + height * 0.5, x + width, y + height * 0.5, 152);

	fracstep = (graphsize << 16) / width;
	yscale = height / (max - min);

	for (i = 0, frac = 0; i <= width; i++, frac += fracstep)
	{
		x1 = x + i;
		y1 = y + height - (int)((graph[frac >> 16] - min) * yscale);

		if (i > 0)
			re.DrawLine (x0, y0, x1, y1, 211);

		x0 = x1;
		y0 = y1;
	}
}


/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5

#ifdef KINGPIN_ENGINE

#define MENU_AMBIENT_INTERVAL	500		// msec between menuamb.wav restarts
#define MENU_PULSE_INTERVAL		25		// msec between animation ticks
#define MENU_FRAME_MAX			11		// max frame index for the active item

// menu item structure, matches the 16-byte layout in the original engine
typedef struct
{
	const char	*name;		// +0
	int			x;			// +4   base X on the 640x480 canvas
	int			y;			// +8   base Y on the 640x480 canvas
	int			frame;		// +12  animation frame counter
} menu_item_t;

// main menu items, coordinates taken from the original data segment
static menu_item_t	menu_main_items[] =
{
	{ "m_game",    108,  65, 0 },
	{ "m_gang",     64, 105, 0 },
	{ "m_option",   56, 155, 0 },
	{ "m_visuals", 100, 201, 0 },
	{ "m_exit",     98, 237, 0 },
	{ NULL, 0, 0, 0 }
};

// frame suffixes: "", "2", "3"
static const char *menu_item_suffix[MENU_FRAME_MAX + 1] =
{
	"", "", "", "", "",			// 0..4
	"2", "2", "2", "2", "2",	// 5..9
	"3", "3"					// 10..11
};

static int	menu_ambient_timeout = 0;
static int	menu_pulse_timeout = 0;


/*
=================
M_Main_Draw (FUN_0042ba40)
=================
*/
void M_Main_Draw (void)
{
	int			i;
	int			w, h;
	float		scale_x, scale_y;
	int			current_time;
	int			pulse_tick;
	int			jitter_state;
	int			jitter_offset;
	char		built_path[MAX_QPATH];
	const char	*suffix;
	const char	*name;
	menu_item_t	*item;

	// kingpin.exe @0042babc: suppress the 3D view behind the full-screen menu.
	m_mainmenu_active = true;

	// base canvas is 640x480, same as the original engine
	scale_x = (float)viddef.width / 640.0f;
	scale_y = (float)viddef.height / 480.0f;

	current_time = Sys_Milliseconds();

	// ambient loop
	if (menu_ambient_timeout == 0 || menu_ambient_timeout <= current_time)
	{
		S_StartLocalSound("misc/menuamb.wav");
		menu_ambient_timeout = current_time + MENU_AMBIENT_INTERVAL;
	}

	// pulse timer drives the item frame animation
	pulse_tick = 0;
	if (menu_pulse_timeout == 0 || menu_pulse_timeout <= current_time)
	{
		pulse_tick = 1;
		menu_pulse_timeout = current_time + MENU_PULSE_INTERVAL;
	}

	// random CRT jitter glitch, ~1/32 chance per frame
	jitter_state = ((rand() & 0x1f) == 0x16) ? 1 : 0;

	// screen background
	re.DrawStretchPic(0, 0, viddef.width, viddef.height, "backround");

	// left lantern screen frame console apparatus
	re.DrawGetPicSize(&w, &h, "main");
	re.DrawStretchPic(0, 0,
					  (int)(w * scale_x), (int)(h * scale_y),
					  "main");

	// kingpin.exe @0042bbb5: fixed 640x480-canvas position, alpha path.
	re.DrawGetPicSize(&w, &h, "kingpin");
	re.DrawStretchPicTransparent((int)(373 * scale_x), (int)(349 * scale_y),
								 (int)(w * scale_x), (int)(h * scale_y),
								 "kingpin", 1.0f);

	// main.tga contains the unlit labels; draw overlays only while fading out.
	for (i = 0; menu_main_items[i].name != NULL; i++)
	{
		item = &menu_main_items[i];

		if (i == m_main_cursor || !item->frame)
			continue;

		suffix = menu_item_suffix[item->frame];
		name = item->name;

		Com_sprintf(built_path, sizeof(built_path), "%s%s", name, suffix);

		re.DrawGetPicSize(&w, &h, built_path);
		re.DrawStretchPicTransparent((int)(item->x * scale_x),
									 (int)(item->y * scale_y),
									 (int)(w * scale_x), (int)(h * scale_y),
									 built_path, 1.0f);

		// kingpin.exe @0042bd1e: decrement after this frame was drawn.
		if (pulse_tick)
			item->frame--;
	}

	// draw the active focused item
	item = &menu_main_items[m_main_cursor];

	if (pulse_tick)
	{
		if (item->frame < MENU_FRAME_MAX)
			item->frame++;
	}

	// jitter shifts the active item on both axes
	jitter_offset = jitter_state * 2;

	suffix = menu_item_suffix[item->frame];
	name = item->name;

	Com_sprintf(built_path, sizeof(built_path), "%s%s", name, suffix);

	re.DrawGetPicSize(&w, &h, built_path);
	re.DrawStretchPicTransparent((int)(item->x * scale_x) + jitter_offset,
								 (int)(item->y * scale_y) + jitter_offset,
								 (int)(w * scale_x), (int)(h * scale_y),
								 built_path, 1.0f);
}

#else
/*
=================
M_Main_Draw
=================
*/
void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for ( i = 0; names[i] != 0; i++ )
	{
		re.DrawGetPicSize( &w, &h, names[i] );

		if ( w > widest )
			widest = w;
		totalheight += ( h + 12 );
	}

	ystart = ( viddef.height / 2 - 110 );
	xoffset = ( viddef.width - widest + 70 ) / 2;

	for ( i = 0; names[i] != 0; i++ )
	{
		if ( i != m_main_cursor )
			re.DrawPic( xoffset, ystart + i * 40 + 13, names[i] );
	}
	strcpy( litname, names[m_main_cursor] );
	strcat( litname, "_sel" );
	re.DrawPic( xoffset, ystart + m_main_cursor * 40 + 13, litname );

	M_DrawCursor( xoffset - 25, ystart + m_main_cursor * 40 + 11, (int)(cls.realtime / 100)%NUM_CURSOR_FRAMES );

	re.DrawGetPicSize( &w, &h, "m_main_plaque" );
	re.DrawPic( xoffset - 30 - w, ystart, "m_main_plaque" );

	re.DrawPic( xoffset - 30 - w, ystart + h + 5, "m_main_logo" );
}
#endif

const char *M_Main_Key (int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	case K_ESCAPE:
	case K_START_BUTTON:
	case K_B_BUTTON:
		M_PopMenu ();
		break;

	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
	case K_A_BUTTON:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_Game_f ();
			break;

		case 1:
			M_Menu_Multiplayer_f();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Video_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}

	return NULL;
}


void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/
static menuframework_s	s_multiplayer_menu;
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;
#if defined(KINGPIN_ENGINE) || !defined(__psp__) 
static menuaction_s		s_heat_action;
#endif

#ifdef __psp__
static menulist_s		s_network_mode_list;
static menulist_s		s_access_point_list;

static char **netaccesspoints_list = NULL;
static int	netaccesspoints_count;

static qboolean Multiplayer_BuildApList (void)
{
	int		i;
	char	buffer[128];
	int		listsize;

	if (netaccesspoints_list)
		return (netaccesspoints_count > 1);

	netaccesspoints_count = 1;
	while (1)
	{
		if (sceUtilityCheckNetParam (netaccesspoints_count))
			break;
		netaccesspoints_count++;
	}

	netaccesspoints_list = Z_Malloc (sizeof(char *) * (netaccesspoints_count + 1));
	memset (netaccesspoints_list, 0, sizeof(char *) * (netaccesspoints_count + 1));

	netaccesspoints_list[0] = CopyString (M_RUS("HET", "HET"));
	for (i = 1; i < netaccesspoints_count; i++)
	{
		if (!sceUtilityGetNetParam (i, PSP_NETPARAM_NAME, (void *)buffer))
			netaccesspoints_list[i] = CopyString (buffer);
	}

	return (netaccesspoints_count > 1);
}

static void Multiplayer_FreeApList (void)
{
	int	i;

	if (!netaccesspoints_list)
		return;

	for (i = 0; i < netaccesspoints_count; i++)
	{
		if (netaccesspoints_list[i])
			Z_Free (netaccesspoints_list[i]);
	}
	Z_Free (netaccesspoints_list);

	netaccesspoints_count = 0;
	netaccesspoints_list = NULL;
}

static void NetModeFunc( void *unused )
{
	Cvar_SetValue( "net_mode", s_network_mode_list.curvalue );
}

static void NetApFunc( void *unused )
{
	Cvar_SetValue( "net_accesspoint", s_access_point_list.curvalue );
}

#endif

static void Multiplayer_MenuDraw (void)
{
	M_Banner( "m_banner_multiplayer" );

	Menu_AdjustCursor( &s_multiplayer_menu, 1 );
	Menu_Draw( &s_multiplayer_menu );
}

static void PlayerSetupFunc( void *unused )
{
	M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc( void *unused )
{
	M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc( void *unused )
{
	M_Menu_StartServer_f ();
}

#if defined(KINGPIN_ENGINE) || !defined(__psp__) 
// kingpin.exe @00428fe0: hand over to the HEAT.NET launcher and quit
static void HeatFunc( void *unused )
{
#ifdef _WIN32
	WinExec (".\\heat\\gotoheat.exe 5185", SW_SHOWNORMAL);
#endif
	CL_Disconnect ();
	Com_Quit ();
}
#endif

void Multiplayer_MenuInit( void )
{
	int		offset_y;
#ifdef __psp__
	cvar_t	*net_accesspoint;
	cvar_t	*net_mode;

	static const char *netmode_items[] =
	{
		M_RUS("LOKAL|HO", "LOKAL|HO"),
		M_RUS("UHFPACTPYKTYPA", "TO4KA DOCTYIA"),
		M_RUS("AD-HOC", "IPRMAR CBR3|"),
		0
	};

	Multiplayer_BuildApList ();

	net_accesspoint = Cvar_Get ("net_accesspoint", "0", CVAR_ARCHIVE);
	net_mode = Cvar_Get ("net_mode", "0", CVAR_ARCHIVE);

	if (net_accesspoint->value >= netaccesspoints_count)
		Cvar_SetValue ("net_accesspoint", 0);

	s_multiplayer_menu.x = viddef.width / 2;
	s_multiplayer_menu.y = viddef.height / 2 - 58;
	s_multiplayer_menu.nitems = 0;

#ifdef KINGPIN_ENGINE
	s_multiplayer_menu.xoffset = 0;
	s_multiplayer_menu.yoffset = 15;
#endif

	offset_y = 30;

	s_network_mode_list.generic.type		= MTYPE_SPINCONTROL;
	s_network_mode_list.generic.x			= 0;
	s_network_mode_list.generic.y			= 0;
	s_network_mode_list.generic.name		= M_RUS(" PEJUM CETU", " PEJUM CETU");
	s_network_mode_list.generic.callback	= NetModeFunc;
	s_network_mode_list.itemnames			= netmode_items;
	s_network_mode_list.curvalue			= net_mode->value;

	s_access_point_list.generic.type		= MTYPE_SPINCONTROL;
	s_access_point_list.generic.x			= 0;
	s_access_point_list.generic.y			= 10;
	s_access_point_list.generic.name		= M_RUS(" TO4KA DOCTYIA", " TO4KA DOCTYIA");
	s_access_point_list.generic.callback	= NetApFunc;
	s_access_point_list.itemnames			= (const char **)netaccesspoints_list;
	s_access_point_list.curvalue			= net_accesspoint->value;
#else
	s_multiplayer_menu.x = viddef.width * 0.50 - 64;
	s_multiplayer_menu.nitems = 0;

	offset_y = 0;
#endif

	s_join_network_server_action.generic.type		= MTYPE_ACTION;
#ifndef __psp__
	s_join_network_server_action.generic.flags		= QMF_LEFT_JUSTIFY;
#endif
	s_join_network_server_action.generic.x			= 0;
	s_join_network_server_action.generic.y			= offset_y + 0;
	s_join_network_server_action.generic.name		= M_RUS(" BOUTU B CET|", " BOZTU B CET|");
	s_join_network_server_action.generic.callback	= JoinNetworkServerFunc;

	s_start_network_server_action.generic.type		= MTYPE_ACTION;
#ifndef __psp__
	s_start_network_server_action.generic.flags		= QMF_LEFT_JUSTIFY;
#endif
	s_start_network_server_action.generic.x			= 0;
	s_start_network_server_action.generic.y			= offset_y + 10;
	s_start_network_server_action.generic.name		= M_RUS(" CO3DAT| CEPBEP", " CO3DAT| CEPBEP");
	s_start_network_server_action.generic.callback	= StartNetworkServerFunc;

	s_player_setup_action.generic.type		= MTYPE_ACTION;
#ifndef __psp__
	s_player_setup_action.generic.flags		= QMF_LEFT_JUSTIFY;
#endif
	s_player_setup_action.generic.x			= 0;
	s_player_setup_action.generic.y			= offset_y + 20;
	s_player_setup_action.generic.name		= M_RUS(" HACTP. UGPAKA", " HACTP. UGPOKA");
	s_player_setup_action.generic.callback	= PlayerSetupFunc;

#ifdef __psp__
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_network_mode_list );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_access_point_list );
#endif

	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_join_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_start_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_player_setup_action );

#if defined(KINGPIN_ENGINE) || !defined(__psp__)
	// kingpin.exe @00428e50 offers HEAT.NET when the system language has
	// the default sub-language
#ifdef _WIN32
	if ((GetSystemDefaultLangID () >> 10) == SUBLANG_DEFAULT)
#else
	if (0)
#endif
	{
		s_heat_action.generic.type	= MTYPE_ACTION;
		s_heat_action.generic.flags  = QMF_LEFT_JUSTIFY;
		s_heat_action.generic.x		= 0;
		s_heat_action.generic.y		= offset_y + 40;
		s_heat_action.generic.name	= M_RUS(" play on heat.net", " UGPAT| B heat.net");
		s_heat_action.generic.callback = HeatFunc;
		Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_heat_action );
	}
#endif

	Menu_SetStatusBar( &s_multiplayer_menu, NULL );

	Menu_Center( &s_multiplayer_menu );
}

const char *Multiplayer_MenuKey( int key )
{
#ifdef __psp__
	if ( key == K_ESCAPE || key == K_START_BUTTON || key == K_B_BUTTON )
		Multiplayer_FreeApList ();
#endif

	return Default_MenuKey( &s_multiplayer_menu, key );
}

void M_Menu_Multiplayer_f( void )
{
	Multiplayer_MenuInit();
	M_PushMenu( Multiplayer_MenuDraw, Multiplayer_MenuKey );
}

/*
=======================================================================

KEYS MENU

=======================================================================
*/
#ifdef KINGPIN_ENGINE
// kingpin.exe @00459d58: Russian glyph slots; see menu_ru.h.
char *bindnames[][2] =
{
{"+attack",		"ATAKA"}, /* АТАКА */ /* АТАКА */
{"weapnext",		"CLED. OPYJUE"}, /* СЛЕД. ОРУЖИЕ */ /* СЛЕД. ОРУЖИЕ */
{"+forward",		"BIEPED"}, /* ВПЕРЕД */ /* ВПЕРЕД */
{"+back",		"HA3AD"}, /* НАЗАД */ /* НАЗАД */
{"+left",		"IOBOPOT BLEBO"}, /* ПОВОРОТ ВЛЕВО */ /* ПОВОРОТ ВЛЕВО */
{"+right",		"IOBOPOT BIPABO"}, /* ПОВОРОТ ВПРАВО */ /* ПОВОРОТ ВПРАВО */
{"+speed",		"VEG"}, /* БЕГ */ /* БЕГ */
{"+moveleft",		"WAG BLEBO"}, /* ШАГ ВЛЕВО */ /* ШАГ ВЛЕВО */
{"+moveright",		"WAG BIPABO"}, /* ШАГ ВПРАВО */ /* ШАГ ВПРАВО */
{"+strafe",		"CDBUG"}, /* СДВИГ */ /* СДВИГ */
{"+lookup",		"CMOTPET| BBEPX"}, /* СМОТРЕТЬ ВВЕРХ */ /* СМОТРЕТЬ ВВЕРХ */
{"+lookdown",		"CMOTPET| BHU3"}, /* СМОТРЕТЬ ВНИЗ */ /* СМОТРЕТЬ ВНИЗ */
{"centerview",		"B3GLRD IO QEHTPY"}, /* ВЗГЛЯД ПО ЦЕНТРУ */ /* ВЗГЛЯД ПО ЦЕНТРУ */
{"+mlook",		"OV3OP M}W|@"}, /* ОБЗОР МЫШЬЮ */ /* ОБЗОР МЫШЬЮ */
{"+klook",		"OV3OP KLABUWAMU"}, /* ОБЗОР КЛАВИШАМИ */ /* ОБЗОР КЛАВИШАМИ */
{"+moveup",		"BBEPX / IP}JOK"}, /* ВВЕРХ / ПРЫЖОК */ /* ВВЕРХ / ПРЫЖОК */
{"+movedown",		"BHU3 / IPUCECT|"}, /* ВНИЗ / ПРИСЕСТЬ */ /* ВНИЗ / ПРИСЕСТЬ */
{"inven",		"UHBEHTAP|"}, /* ИНВЕНТАРЬ */ /* ИНВЕНТАРЬ */
{"invuse",		"UCIOL|3OBAT|"}, /* ИСПОЛЬЗОВАТЬ */ /* ИСПОЛЬЗОВАТЬ */
{"invdrop",		"B}VPOCUT|"}, /* ВЫБРОСИТЬ */ /* ВЫБРОСИТЬ */
{"invprev",		"IPED. IPEDMET"}, /* ПРЕД. ПРЕДМЕТ */ /* ПРЕД. ПРЕДМЕТ */
{"invnext",		"CLED. IPEDMET"}, /* СЛЕД. ПРЕДМЕТ */ /* СЛЕД. ПРЕДМЕТ */
{"cmd help",		"3AIUCHAR KHUJKA"}, /* ЗАПИСНАЯ КНИЖКА */ /* ЗАПИСНАЯ КНИЖКА */
{"+activate",		"DEZCTBUE"}, /* ДЕЙСТВИЕ */ /* ДЕЙСТВИЕ */
{"holster",		"YVPAT| OPYJUE"}, /* УБРАТЬ ОРУЖИЕ */ /* УБРАТЬ ОРУЖИЕ */
{"flashlight",		"FOHAP|"}, /* ФОНАРЬ */ /* ФОНАРЬ */
{"hud",		"UHTEPFEZC"}, /* ИНТЕРФЕЙС */ /* ИНТЕРФЕЙС */
{"reload",		"IEPE3APRDKA"}, /* ПЕРЕЗАРЯДКА */ /* ПЕРЕЗАРЯДКА */
{"key1",		"DA / 3A MHOZ"}, /* ДА / ЗА МНОЙ */ /* ДА / ЗА МНОЙ */
{"key2",		"OCOV}Z IPUKA3"}, /* ОСОБЫЙ ПРИКАЗ */ /* ОСОБЫЙ ПРИКАЗ */
{"key3",		"HET / CTORT|"}, /* НЕТ / СТОЯТЬ */ /* НЕТ / СТОЯТЬ */
{ 0, 0 }
};
#define NUM_BINDNAMES	31
#else
char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"weapnext", 		"next weapon"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"up / jump"},
{"+movedown",		"down / crouch"},

{"inven",			"inventory"},
{"invuse",			"use item"},
{"invdrop",			"drop item"},
{"invprev",			"prev item"},
{"invnext",			"next item"},

{"cmd help", 		"help computer" },
{ 0, 0 }
};
#endif

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
#ifndef KINGPIN_ENGINE
static menuaction_s		s_keys_attack_action;
static menuaction_s		s_keys_change_weapon_action;
static menuaction_s		s_keys_walk_forward_action;
static menuaction_s		s_keys_backpedal_action;
static menuaction_s		s_keys_turn_left_action;
static menuaction_s		s_keys_turn_right_action;
static menuaction_s		s_keys_run_action;
static menuaction_s		s_keys_step_left_action;
static menuaction_s		s_keys_step_right_action;
static menuaction_s		s_keys_sidestep_action;
static menuaction_s		s_keys_look_up_action;
static menuaction_s		s_keys_look_down_action;
static menuaction_s		s_keys_center_view_action;
static menuaction_s		s_keys_mouse_look_action;
static menuaction_s		s_keys_keyboard_look_action;
static menuaction_s		s_keys_move_up_action;
static menuaction_s		s_keys_move_down_action;
static menuaction_s		s_keys_inventory_action;
static menuaction_s		s_keys_inv_use_action;
static menuaction_s		s_keys_inv_drop_action;
static menuaction_s		s_keys_inv_prev_action;
static menuaction_s		s_keys_inv_next_action;

static menuaction_s		s_keys_help_computer_action;
#else
static menuaction_s		s_keys_actions[NUM_BINDNAMES];
#endif

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc( menuframework_s *menu )
{
#ifdef KINGPIN_ENGINE
	// kingpin.exe @004299a0: shifted like the rest of the menu text
	if ( bind_grab )
		re.DrawChar( viddef.width/5 + menu->x, menu->y + menu->cursor * 9, '=' );
	else
		re.DrawChar( viddef.width/5 + menu->x, menu->y + menu->cursor * 9, 12 + ( ( int ) ( Sys_Milliseconds() / 250 ) & 1 ) );
#else
	if ( bind_grab )
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, '=' );
	else
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, 12 + ( ( int ) ( Sys_Milliseconds() / 250 ) & 1 ) );
#endif
}

// Display names only: binding commands and config files retain canonical keys.
static const char *M_KeyDisplayName (int key)
{
#ifdef KINGPIN_ENGINE
	switch (key)
	{
	case K_A_BUTTON: return "KPECT";       // КРЕСТ
	case K_B_BUTTON: return "KPYG";        // КРУГ
	case K_X_BUTTON: return "KBADPAT";     // КВАДРАТ
	case K_Y_BUTTON: return "TPEYGOL|HUK"; // ТРЕУГОЛЬНИК
	case K_L1_BUTTON: return "LEB.";
	case K_R1_BUTTON: return "IPAB.";
	case K_MODE_BUTTON: return "B}VOP";    // ВЫБОР
	case K_START_BUTTON: return "CTAPT";   // СТАРТ
	case K_BACK_BUTTON: return "HA3AD";   // НАЗАД
	case K_UPARROW: return "BBEPX";       // ВВЕРХ
	case K_DOWNARROW: return "BHU3";      // ВНИЗ
	case K_LEFTARROW: return "BLEBO";     // ВЛЕВО
	case K_RIGHTARROW: return "BIPABO";   // ВПРАВО
	case K_SPACE: return "IPOVEL";        // ПРОБЕЛ
	case K_ENTER: return "BBOD";          // ВВОД
	case K_ESCAPE: return "OTMEHA";       // ОТМЕНА
	case K_BACKSPACE: return "3AVOZ";     // ЗАБОЙ
	case K_MOUSE1: return "M}W| 1";
	case K_MOUSE2: return "M}W| 2";
	case K_MOUSE3: return "M}W| 3";
	}
#endif
	return Key_KeynumToString (key);
}

static void DrawKeyBindingFunc( void *self )
{
	int keys[2];
	menuaction_s *a = ( menuaction_s * ) self;

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys);

	if (keys[0] == -1)
	{
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???" );
	}
	else
	{
		int x;
		const char *name;

		name = M_KeyDisplayName (keys[0]);

		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name );

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString( a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, M_RUS("or", "ULU") );
			Menu_DrawString( a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, M_KeyDisplayName (keys[1]) );
		}
	}
}

static void KeyBindingFunc( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;
	int keys[2];

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys );

	if (keys[1] != -1)
		M_UnbindCommand( bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar( &s_keys_menu, M_RUS("HAJMUTE KLAVUWU DLR DEUSTBUR", "HAJMUTE KHOIKY DLR DEZCTBUR") );
}

#ifdef KINGPIN_ENGINE
// kingpin.exe @004290c0: one line per bindnames entry, 9 pixels apart
static void Keys_MenuInit( void )
{
	int i;

	s_keys_menu.x = viddef.width * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;
	s_keys_menu.xoffset = 10;
	s_keys_menu.yoffset = -30;

	for (i = 0; i < NUM_BINDNAMES; i++)
	{
		s_keys_actions[i].generic.type			= MTYPE_ACTION;
		s_keys_actions[i].generic.flags			= QMF_GRAYED;
		s_keys_actions[i].generic.x				= 0;
		s_keys_actions[i].generic.y				= i * 9;
		s_keys_actions[i].generic.ownerdraw		= DrawKeyBindingFunc;
		s_keys_actions[i].generic.localdata[0]	= i;
		s_keys_actions[i].generic.name			= bindnames[i][1];

		Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_actions[i] );
	}

#if __psp__
	Menu_SetStatusBar( &s_keys_menu, M_RUS("KPECT - U3MEHUT|, KBD - CVRAT|", "KPECT: U3MEHUT|, KBADPAT: CTEPET|") );
#else
	Menu_SetStatusBar( &s_keys_menu, M_RUS("enter to change, backspace to clear", "BBOD: U3MEHUT|, 3AVOZ: CTEPET|") );
#endif
	Menu_Center( &s_keys_menu );
}
#else
static void Keys_MenuInit( void )
{
	int y = 0;
	int i = 0;

	s_keys_menu.x = viddef.width * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	s_keys_attack_action.generic.type	= MTYPE_ACTION;
	s_keys_attack_action.generic.flags  = QMF_GRAYED;
	s_keys_attack_action.generic.x		= 0;
	s_keys_attack_action.generic.y		= y;
	s_keys_attack_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_attack_action.generic.localdata[0] = i;
	s_keys_attack_action.generic.name	= bindnames[s_keys_attack_action.generic.localdata[0]][1];

	s_keys_change_weapon_action.generic.type	= MTYPE_ACTION;
	s_keys_change_weapon_action.generic.flags  = QMF_GRAYED;
	s_keys_change_weapon_action.generic.x		= 0;
	s_keys_change_weapon_action.generic.y		= y += 9;
	s_keys_change_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_change_weapon_action.generic.localdata[0] = ++i;
	s_keys_change_weapon_action.generic.name	= bindnames[s_keys_change_weapon_action.generic.localdata[0]][1];

	s_keys_walk_forward_action.generic.type	= MTYPE_ACTION;
	s_keys_walk_forward_action.generic.flags  = QMF_GRAYED;
	s_keys_walk_forward_action.generic.x		= 0;
	s_keys_walk_forward_action.generic.y		= y += 9;
	s_keys_walk_forward_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_walk_forward_action.generic.localdata[0] = ++i;
	s_keys_walk_forward_action.generic.name	= bindnames[s_keys_walk_forward_action.generic.localdata[0]][1];

	s_keys_backpedal_action.generic.type	= MTYPE_ACTION;
	s_keys_backpedal_action.generic.flags  = QMF_GRAYED;
	s_keys_backpedal_action.generic.x		= 0;
	s_keys_backpedal_action.generic.y		= y += 9;
	s_keys_backpedal_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_backpedal_action.generic.localdata[0] = ++i;
	s_keys_backpedal_action.generic.name	= bindnames[s_keys_backpedal_action.generic.localdata[0]][1];

	s_keys_turn_left_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_left_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_left_action.generic.x		= 0;
	s_keys_turn_left_action.generic.y		= y += 9;
	s_keys_turn_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_left_action.generic.localdata[0] = ++i;
	s_keys_turn_left_action.generic.name	= bindnames[s_keys_turn_left_action.generic.localdata[0]][1];

	s_keys_turn_right_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_right_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_right_action.generic.x		= 0;
	s_keys_turn_right_action.generic.y		= y += 9;
	s_keys_turn_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_right_action.generic.localdata[0] = ++i;
	s_keys_turn_right_action.generic.name	= bindnames[s_keys_turn_right_action.generic.localdata[0]][1];

	s_keys_run_action.generic.type	= MTYPE_ACTION;
	s_keys_run_action.generic.flags  = QMF_GRAYED;
	s_keys_run_action.generic.x		= 0;
	s_keys_run_action.generic.y		= y += 9;
	s_keys_run_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_run_action.generic.localdata[0] = ++i;
	s_keys_run_action.generic.name	= bindnames[s_keys_run_action.generic.localdata[0]][1];

	s_keys_step_left_action.generic.type	= MTYPE_ACTION;
	s_keys_step_left_action.generic.flags  = QMF_GRAYED;
	s_keys_step_left_action.generic.x		= 0;
	s_keys_step_left_action.generic.y		= y += 9;
	s_keys_step_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_left_action.generic.localdata[0] = ++i;
	s_keys_step_left_action.generic.name	= bindnames[s_keys_step_left_action.generic.localdata[0]][1];

	s_keys_step_right_action.generic.type	= MTYPE_ACTION;
	s_keys_step_right_action.generic.flags  = QMF_GRAYED;
	s_keys_step_right_action.generic.x		= 0;
	s_keys_step_right_action.generic.y		= y += 9;
	s_keys_step_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_right_action.generic.localdata[0] = ++i;
	s_keys_step_right_action.generic.name	= bindnames[s_keys_step_right_action.generic.localdata[0]][1];

	s_keys_sidestep_action.generic.type	= MTYPE_ACTION;
	s_keys_sidestep_action.generic.flags  = QMF_GRAYED;
	s_keys_sidestep_action.generic.x		= 0;
	s_keys_sidestep_action.generic.y		= y += 9;
	s_keys_sidestep_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_sidestep_action.generic.localdata[0] = ++i;
	s_keys_sidestep_action.generic.name	= bindnames[s_keys_sidestep_action.generic.localdata[0]][1];

	s_keys_look_up_action.generic.type	= MTYPE_ACTION;
	s_keys_look_up_action.generic.flags  = QMF_GRAYED;
	s_keys_look_up_action.generic.x		= 0;
	s_keys_look_up_action.generic.y		= y += 9;
	s_keys_look_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_up_action.generic.localdata[0] = ++i;
	s_keys_look_up_action.generic.name	= bindnames[s_keys_look_up_action.generic.localdata[0]][1];

	s_keys_look_down_action.generic.type	= MTYPE_ACTION;
	s_keys_look_down_action.generic.flags  = QMF_GRAYED;
	s_keys_look_down_action.generic.x		= 0;
	s_keys_look_down_action.generic.y		= y += 9;
	s_keys_look_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_down_action.generic.localdata[0] = ++i;
	s_keys_look_down_action.generic.name	= bindnames[s_keys_look_down_action.generic.localdata[0]][1];

	s_keys_center_view_action.generic.type	= MTYPE_ACTION;
	s_keys_center_view_action.generic.flags  = QMF_GRAYED;
	s_keys_center_view_action.generic.x		= 0;
	s_keys_center_view_action.generic.y		= y += 9;
	s_keys_center_view_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_center_view_action.generic.localdata[0] = ++i;
	s_keys_center_view_action.generic.name	= bindnames[s_keys_center_view_action.generic.localdata[0]][1];

	s_keys_mouse_look_action.generic.type	= MTYPE_ACTION;
	s_keys_mouse_look_action.generic.flags  = QMF_GRAYED;
	s_keys_mouse_look_action.generic.x		= 0;
	s_keys_mouse_look_action.generic.y		= y += 9;
	s_keys_mouse_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_mouse_look_action.generic.localdata[0] = ++i;
	s_keys_mouse_look_action.generic.name	= bindnames[s_keys_mouse_look_action.generic.localdata[0]][1];

	s_keys_keyboard_look_action.generic.type	= MTYPE_ACTION;
	s_keys_keyboard_look_action.generic.flags  = QMF_GRAYED;
	s_keys_keyboard_look_action.generic.x		= 0;
	s_keys_keyboard_look_action.generic.y		= y += 9;
	s_keys_keyboard_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_keyboard_look_action.generic.localdata[0] = ++i;
	s_keys_keyboard_look_action.generic.name	= bindnames[s_keys_keyboard_look_action.generic.localdata[0]][1];

	s_keys_move_up_action.generic.type	= MTYPE_ACTION;
	s_keys_move_up_action.generic.flags  = QMF_GRAYED;
	s_keys_move_up_action.generic.x		= 0;
	s_keys_move_up_action.generic.y		= y += 9;
	s_keys_move_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_up_action.generic.localdata[0] = ++i;
	s_keys_move_up_action.generic.name	= bindnames[s_keys_move_up_action.generic.localdata[0]][1];

	s_keys_move_down_action.generic.type	= MTYPE_ACTION;
	s_keys_move_down_action.generic.flags  = QMF_GRAYED;
	s_keys_move_down_action.generic.x		= 0;
	s_keys_move_down_action.generic.y		= y += 9;
	s_keys_move_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_down_action.generic.localdata[0] = ++i;
	s_keys_move_down_action.generic.name	= bindnames[s_keys_move_down_action.generic.localdata[0]][1];

	s_keys_inventory_action.generic.type	= MTYPE_ACTION;
	s_keys_inventory_action.generic.flags  = QMF_GRAYED;
	s_keys_inventory_action.generic.x		= 0;
	s_keys_inventory_action.generic.y		= y += 9;
	s_keys_inventory_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inventory_action.generic.localdata[0] = ++i;
	s_keys_inventory_action.generic.name	= bindnames[s_keys_inventory_action.generic.localdata[0]][1];

	s_keys_inv_use_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_use_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_use_action.generic.x		= 0;
	s_keys_inv_use_action.generic.y		= y += 9;
	s_keys_inv_use_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_use_action.generic.localdata[0] = ++i;
	s_keys_inv_use_action.generic.name	= bindnames[s_keys_inv_use_action.generic.localdata[0]][1];

	s_keys_inv_drop_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_drop_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_drop_action.generic.x		= 0;
	s_keys_inv_drop_action.generic.y		= y += 9;
	s_keys_inv_drop_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_drop_action.generic.localdata[0] = ++i;
	s_keys_inv_drop_action.generic.name	= bindnames[s_keys_inv_drop_action.generic.localdata[0]][1];

	s_keys_inv_prev_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_prev_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_prev_action.generic.x		= 0;
	s_keys_inv_prev_action.generic.y		= y += 9;
	s_keys_inv_prev_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_prev_action.generic.localdata[0] = ++i;
	s_keys_inv_prev_action.generic.name	= bindnames[s_keys_inv_prev_action.generic.localdata[0]][1];

	s_keys_inv_next_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_next_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_next_action.generic.x		= 0;
	s_keys_inv_next_action.generic.y		= y += 9;
	s_keys_inv_next_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_next_action.generic.localdata[0] = ++i;
	s_keys_inv_next_action.generic.name	= bindnames[s_keys_inv_next_action.generic.localdata[0]][1];

	s_keys_help_computer_action.generic.type	= MTYPE_ACTION;
	s_keys_help_computer_action.generic.flags  = QMF_GRAYED;
	s_keys_help_computer_action.generic.x		= 0;
	s_keys_help_computer_action.generic.y		= y += 9;
	s_keys_help_computer_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_help_computer_action.generic.localdata[0] = ++i;
	s_keys_help_computer_action.generic.name	= bindnames[s_keys_help_computer_action.generic.localdata[0]][1];

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_attack_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_change_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_walk_forward_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_backpedal_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_run_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_sidestep_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_center_view_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_mouse_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_keyboard_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_down_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inventory_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_use_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_drop_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_prev_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_next_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_help_computer_action );

#if __psp__
	Menu_SetStatusBar( &s_keys_menu, M_RUS("CROSS to change, SQUARE to clear", "KPECT: U3MEHUT|, KBADPAT: CTEPET|") );
#else
	Menu_SetStatusBar( &s_keys_menu, M_RUS("enter to change, backspace to clear", "BBOD: U3MEHUT|, 3AVOZ: CTEPET|") );
#endif
	Menu_Center( &s_keys_menu );
}
#endif

static void Keys_MenuDraw (void)
{
	Menu_AdjustCursor( &s_keys_menu, 1 );
	Menu_Draw( &s_keys_menu );
}

static const char *Keys_MenuKey( int key )
{
	menuaction_s *item = ( menuaction_s * ) Menu_ItemAtCursor( &s_keys_menu );

	if ( bind_grab )
	{
		if ( key != K_ESCAPE && key != K_START_BUTTON && key != K_MODE_BUTTON && key != '`' )
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}

#if __psp__
		Menu_SetStatusBar( &s_keys_menu, M_RUS("KPECT - U3MEHUT|, KBD - CVRAT|", "KPECT: U3MEHUT|, KBADPAT: CTEPET|") );
#else
		Menu_SetStatusBar( &s_keys_menu, M_RUS("enter to change, backspace to clear", "BBOD: U3MEHUT|, 3AVOZ: CTEPET|") );
#endif
		bind_grab = false;
		return menu_out_sound;
	}

	switch ( key )
	{
	case K_KP_ENTER:
	case K_ENTER:
	case K_A_BUTTON:
		KeyBindingFunc( item );
		return menu_in_sound;
	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
	case K_X_BUTTON:
		M_UnbindCommand( bindnames[item->generic.localdata[0]][0] );
		return menu_out_sound;
	default:
		return Default_MenuKey( &s_keys_menu, key );
	}
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit();
	M_PushMenu( Keys_MenuDraw, Keys_MenuKey );
}


/*
=======================================================================

JOYSTICK MENU

=======================================================================
*/
#ifdef __psp__
static menuframework_s	s_joy_menu;
static menulist_s		s_joy_axisx_list;
static menuslider_s		s_joy_axisx_sensitivity_slider;
static menulist_s		s_joy_axisx_inv_box;

static menulist_s		s_joy_axisy_list;
static menuslider_s		s_joy_axisy_sensitivity_slider;
static menulist_s		s_joy_axisy_inv_box;

static menulist_s		s_joy_smooth_box;
static menuslider_s		s_joy_dz_min_slider;
static menuslider_s		s_joy_dz_max_slider;
static menuslider_s		s_joy_cv_power_slider;
static menuslider_s		s_joy_cv_expo_slider;

static char *joy_sensitivity_cvars[] =
{
	"",
	"joy_forwardsensitivity",
	"joy_pitchsensitivity",
	"joy_sidesensitivity",
	"joy_yawsensitivity",
	"joy_upsensitivity"
};

static void UpdateAxisXFunc( void *unused )
{
	float	joy_sensitivity_val;

	if (s_joy_axisx_list.curvalue)
	{
		joy_sensitivity_val = Cvar_VariableValue( joy_sensitivity_cvars[s_joy_axisx_list.curvalue] );
		s_joy_axisx_sensitivity_slider.generic.flags &= ~QMF_INACTIVE;
		s_joy_axisx_sensitivity_slider.curvalue	= fabsf( joy_sensitivity_val ) * 10;
		s_joy_axisx_inv_box.generic.flags &= ~QMF_INACTIVE;
		s_joy_axisx_inv_box.curvalue = ( joy_sensitivity_val < 0.0f );
	}
	else
	{
		s_joy_axisx_sensitivity_slider.generic.flags |= QMF_INACTIVE;
		s_joy_axisx_sensitivity_slider.curvalue	= 0;
		s_joy_axisx_inv_box.generic.flags |= QMF_INACTIVE;
		s_joy_axisx_inv_box.curvalue = 0;
	}

	Cvar_SetValue( "joy_axisx", s_joy_axisx_list.curvalue );
}
static void UpdateSensitivityXFunc( void *unused )
{
	if( s_joy_axisx_inv_box.curvalue )
		Cvar_SetValue( joy_sensitivity_cvars[s_joy_axisx_list.curvalue], -s_joy_axisx_sensitivity_slider.curvalue * 0.1 );
	else
		Cvar_SetValue( joy_sensitivity_cvars[s_joy_axisx_list.curvalue], s_joy_axisx_sensitivity_slider.curvalue * 0.1 );

	if (s_joy_axisx_list.curvalue == s_joy_axisy_list.curvalue)
	{
		s_joy_axisy_sensitivity_slider.curvalue = s_joy_axisx_sensitivity_slider.curvalue;
		s_joy_axisy_inv_box.curvalue = s_joy_axisx_inv_box.curvalue;
	}
}

static void UpdateAxisYFunc( void *unused )
{
	float	joy_sensitivity_val;

	if (s_joy_axisy_list.curvalue)
	{
		joy_sensitivity_val = Cvar_VariableValue( joy_sensitivity_cvars[s_joy_axisy_list.curvalue] );
		s_joy_axisy_sensitivity_slider.generic.flags &= ~QMF_INACTIVE;
		s_joy_axisy_sensitivity_slider.curvalue	= fabsf( joy_sensitivity_val ) * 10;
		s_joy_axisy_inv_box.generic.flags &= ~QMF_INACTIVE;
		s_joy_axisy_inv_box.curvalue = ( joy_sensitivity_val < 0.0f );
	}
	else
	{
		s_joy_axisy_sensitivity_slider.generic.flags |= QMF_INACTIVE;
		s_joy_axisy_sensitivity_slider.curvalue	= 0;
		s_joy_axisy_inv_box.generic.flags |= QMF_INACTIVE;
		s_joy_axisy_inv_box.curvalue = 0;
	}

	Cvar_SetValue( "joy_axisy", s_joy_axisy_list.curvalue );
}
static void UpdateSensitivityYFunc( void *unused )
{
	if( s_joy_axisy_inv_box.curvalue )
		Cvar_SetValue( joy_sensitivity_cvars[s_joy_axisy_list.curvalue], -s_joy_axisy_sensitivity_slider.curvalue * 0.1 );
	else
		Cvar_SetValue( joy_sensitivity_cvars[s_joy_axisy_list.curvalue], s_joy_axisy_sensitivity_slider.curvalue * 0.1 );

	if (s_joy_axisy_list.curvalue == s_joy_axisx_list.curvalue)
	{
		s_joy_axisx_sensitivity_slider.curvalue = s_joy_axisy_sensitivity_slider.curvalue;
		s_joy_axisx_inv_box.curvalue = s_joy_axisy_inv_box.curvalue;
	}
}

static void UpdateSmoothFunc( void *unused )
{
	Cvar_SetValue( "joy_smooth", s_joy_smooth_box.curvalue );
}

static void UpdateDzMinFunc( void *unused )
{
	Cvar_SetValue( "joy_dz_min", s_joy_dz_min_slider.curvalue );
}
static void UpdateDzMaxFunc( void *unused )
{
	Cvar_SetValue( "joy_dz_max", s_joy_dz_max_slider.curvalue );
}
static void UpdateCvPowerFunc( void *unused )
{
	Cvar_SetValue( "joy_cv_power", s_joy_cv_power_slider.curvalue );
}
static void UpdateCvExpoFunc( void *unused )
{
	Cvar_SetValue( "joy_cv_expo", s_joy_cv_expo_slider.curvalue * 0.025 );
}


void Joy_MenuInit( void )
{
	int		position_y;
	float	joy_sensitivity_val;

	static const char *yesno_names[] =
	{
		M_RUS("HET", "HET"),
		M_RUS("DA", "DA"),
		0
	};

	static const char *joy_axis_items[] =
	{
		M_RUS("HET", "HET"), M_RUS("BIEPED", "BIEPED"), M_RUS("OB3OP", "OV3OP"), M_RUS("CTPAUF", "CDBUG"), M_RUS("IOBOPOT", "IOBOPOT"), M_RUS("BBEPX", "BBEPX"), 0
	};

	/*
	** configure controls menu and menu items
	*/
	s_joy_menu.x = viddef.width / 2;
	s_joy_menu.y = viddef.height / 2 - 58;
	s_joy_menu.nitems = 0;

	position_y = 0;

	s_joy_axisx_list.generic.type					= MTYPE_SPINCONTROL;
	s_joy_axisx_list.generic.x						= 0;
	s_joy_axisx_list.generic.y						= position_y;
	s_joy_axisx_list.generic.name					= M_RUS("OC| X", "OC| X");
	s_joy_axisx_list.generic.callback				= UpdateAxisXFunc;
	s_joy_axisx_list.itemnames						= joy_axis_items;
	s_joy_axisx_list.curvalue						= Cvar_VariableValue( "joy_axisx" );

	position_y += 10;

	joy_sensitivity_val = Cvar_VariableValue( joy_sensitivity_cvars[s_joy_axisx_list.curvalue] );

	s_joy_axisx_sensitivity_slider.generic.type		= MTYPE_SLIDER;
	s_joy_axisx_sensitivity_slider.generic.flags	= (s_joy_axisx_list.curvalue ? 0 : QMF_INACTIVE);
	s_joy_axisx_sensitivity_slider.generic.x		= 0;
	s_joy_axisx_sensitivity_slider.generic.y		= position_y;
	s_joy_axisx_sensitivity_slider.generic.name		= M_RUS("4YBCTB. X", "4YBCTB. X");
	s_joy_axisx_sensitivity_slider.generic.callback	= UpdateSensitivityXFunc;
	s_joy_axisx_sensitivity_slider.minvalue			= 0;
	s_joy_axisx_sensitivity_slider.maxvalue			= 10;
	s_joy_axisx_sensitivity_slider.curvalue			= fabsf( joy_sensitivity_val ) * 10;

	position_y += 10;

	s_joy_axisx_inv_box.generic.type				= MTYPE_SPINCONTROL;
	s_joy_axisx_inv_box.generic.flags				= (s_joy_axisx_list.curvalue ? 0 : QMF_INACTIVE);
	s_joy_axisx_inv_box.generic.x					= 0;
	s_joy_axisx_inv_box.generic.y					= position_y;
	s_joy_axisx_inv_box.generic.name				= M_RUS("UHB. X", "UHBEPCUR X");
	s_joy_axisx_inv_box.generic.callback			= UpdateSensitivityXFunc;
	s_joy_axisx_inv_box.itemnames					= yesno_names;
	s_joy_axisx_inv_box.curvalue					= ( joy_sensitivity_val < 0.0f );

	position_y += 20;

	s_joy_axisy_list.generic.type					= MTYPE_SPINCONTROL;
	s_joy_axisy_list.generic.x						= 0;
	s_joy_axisy_list.generic.y						= position_y;
	s_joy_axisy_list.generic.name					= M_RUS("OC| Y", "OC| Y");
	s_joy_axisy_list.generic.callback				= UpdateAxisYFunc;
	s_joy_axisy_list.itemnames						= joy_axis_items;
	s_joy_axisy_list.curvalue						= Cvar_VariableValue( "joy_axisy" );

	position_y += 10;

	joy_sensitivity_val = Cvar_VariableValue( joy_sensitivity_cvars[s_joy_axisy_list.curvalue] );

	s_joy_axisy_sensitivity_slider.generic.type		= MTYPE_SLIDER;
	s_joy_axisy_sensitivity_slider.generic.flags	= (s_joy_axisy_list.curvalue ? 0 : QMF_INACTIVE);
	s_joy_axisy_sensitivity_slider.generic.x		= 0;
	s_joy_axisy_sensitivity_slider.generic.y		= position_y;
	s_joy_axisy_sensitivity_slider.generic.name		= M_RUS("4YBCTB. Y", "4YBCTB. Y");
	s_joy_axisy_sensitivity_slider.generic.callback	= UpdateSensitivityYFunc;
	s_joy_axisy_sensitivity_slider.minvalue			= 0;
	s_joy_axisy_sensitivity_slider.maxvalue			= 10;
	s_joy_axisy_sensitivity_slider.curvalue			= fabsf( joy_sensitivity_val ) * 10;

	position_y += 10;

	s_joy_axisy_inv_box.generic.type				= MTYPE_SPINCONTROL;
	s_joy_axisy_inv_box.generic.flags				= (s_joy_axisy_list.curvalue ? 0 : QMF_INACTIVE);
	s_joy_axisy_inv_box.generic.x					= 0;
	s_joy_axisy_inv_box.generic.y					= position_y;
	s_joy_axisy_inv_box.generic.name				= M_RUS("UHB. Y", "UHBEPCUR Y");
	s_joy_axisy_inv_box.generic.callback			= UpdateSensitivityYFunc;
	s_joy_axisy_inv_box.itemnames					= yesno_names;
	s_joy_axisy_inv_box.curvalue					= ( joy_sensitivity_val < 0.0f );

	position_y += 20;

	s_joy_smooth_box.generic.type					= MTYPE_SPINCONTROL;
	s_joy_smooth_box.generic.x						= 0;
	s_joy_smooth_box.generic.y						= position_y;
	s_joy_smooth_box.generic.name					= M_RUS("SGLAJUBAHUE", "CGLAJUBAHUE");
	s_joy_smooth_box.generic.callback				= UpdateSmoothFunc;
	s_joy_smooth_box.itemnames						= yesno_names;
	s_joy_smooth_box.curvalue						= Cvar_VariableValue( "joy_smooth" );

	position_y += 20;

	s_joy_dz_min_slider.generic.type				= MTYPE_SLIDER;
	s_joy_dz_min_slider.generic.x					= 0;
	s_joy_dz_min_slider.generic.y					= position_y;
	s_joy_dz_min_slider.generic.name				= M_RUS("MEPTB. 3OHA MIN", "MEPTB. 3OHA MUH.");
	s_joy_dz_min_slider.generic.callback			= UpdateDzMinFunc;
	s_joy_dz_min_slider.minvalue					= 0;
	s_joy_dz_min_slider.maxvalue					= 32;
	s_joy_dz_min_slider.curvalue					= Cvar_VariableValue( "joy_dz_min" );

	position_y += 10;

	s_joy_dz_max_slider.generic.type				= MTYPE_SLIDER;
	s_joy_dz_max_slider.generic.x					= 0;
	s_joy_dz_max_slider.generic.y					= position_y;
	s_joy_dz_max_slider.generic.name				= M_RUS("MEPTB. 3OHA MAX", "MEPTB. 3OHA MAKC.");
	s_joy_dz_max_slider.generic.callback			= UpdateDzMaxFunc;
	s_joy_dz_max_slider.minvalue					= 0;
	s_joy_dz_max_slider.maxvalue					= 32;
	s_joy_dz_max_slider.curvalue					= Cvar_VariableValue( "joy_dz_max" );

	position_y += 10;

	s_joy_cv_power_slider.generic.type				= MTYPE_SLIDER;
	s_joy_cv_power_slider.generic.x					= 0;
	s_joy_cv_power_slider.generic.y					= position_y;
	s_joy_cv_power_slider.generic.name				= M_RUS("Cv POWER", "KPUBAR: CTEIEH|");
	s_joy_cv_power_slider.generic.callback			= UpdateCvPowerFunc;
	s_joy_cv_power_slider.minvalue					= 2;
	s_joy_cv_power_slider.maxvalue					= 10;
	s_joy_cv_power_slider.curvalue					= Cvar_VariableValue( "joy_cv_power" );

	position_y += 10;

	s_joy_cv_expo_slider.generic.type				= MTYPE_SLIDER;
	s_joy_cv_expo_slider.generic.x					= 0;
	s_joy_cv_expo_slider.generic.y					= position_y;
	s_joy_cv_expo_slider.generic.name				= M_RUS("Cv EXPO", "KPUBAR: {KCIOHEHTA");
	s_joy_cv_expo_slider.generic.callback			= UpdateCvExpoFunc;
	s_joy_cv_expo_slider.minvalue					= 0;
	s_joy_cv_expo_slider.maxvalue					= 40;
	s_joy_cv_expo_slider.curvalue					= Cvar_VariableValue( "joy_cv_expo" ) * 40;

	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisx_list );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisx_sensitivity_slider );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisx_inv_box );

	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisy_list );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisy_sensitivity_slider );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_axisy_inv_box );

	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_smooth_box );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_dz_min_slider );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_dz_max_slider );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_cv_power_slider );
	Menu_AddItem( &s_joy_menu, ( void * ) &s_joy_cv_expo_slider );
}

void Joy_MenuDraw (void)
{
	Menu_AdjustCursor( &s_joy_menu, 1 );
	Menu_Draw( &s_joy_menu );

	M_DrawGraph (s_joy_menu.x + 120, s_joy_menu.y + 8, 103, 100, -1.0f, 1.0f, 256, IN_JoyGetCurve());
}

const char *Joy_MenuKey( int key )
{
	return Default_MenuKey( &s_joy_menu, key );
}

void M_Menu_Joy_f (void)
{
	Joy_MenuInit();
	M_PushMenu ( Joy_MenuDraw, Joy_MenuKey );
}
#endif // __psp__

/*
=======================================================================

CONTROLS MENU

=======================================================================
*/
#ifndef __psp__
static cvar_t *win_noalttab;
#endif
extern cvar_t *in_joystick;

static menuframework_s	s_options_menu;
static menuaction_s		s_options_defaults_action;
#ifdef __psp__
static menuaction_s		s_options_joystick_setup_action;
#endif
static menuaction_s		s_options_customize_options_action;
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
static menulist_s		s_options_noalttab_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
static menuslider_s		s_options_cdvolume_slider;
static menulist_s		s_options_joystick_box;
static menulist_s		s_options_cdenable_box;
#ifndef __psp__
static menulist_s		s_options_quality_list;
static menulist_s		s_options_compatibility_list;
#endif
static menulist_s		s_options_console_action;
#ifdef KINGPIN_ENGINE
static menulist_s s_options_hudtalk_box, s_options_hudguys_box;
static menulist_s s_options_captions_box, s_options_dirlight_box, s_options_props_box;
static menuslider_s s_options_fireworks_slider, s_options_bloodtime_slider;

static void HudTalkFunc (void *unused)
{
	Cvar_SetValue ("hudtalk", s_options_hudtalk_box.curvalue);
}
static void HudGuysFunc (void *unused)
{
	Cvar_SetValue ("hudguys", s_options_hudguys_box.curvalue);
}
static void CaptionsFunc (void *unused)
{
	Cvar_SetValue ("cl_captions", s_options_captions_box.curvalue);
}
static void FireworksFunc (void *unused)
{
	Cvar_SetValue ("cl_fireworks", s_options_fireworks_slider.curvalue);
}
static void DirLightFunc (void *unused)
{
	Cvar_SetValue ("r_directional_lighting", s_options_dirlight_box.curvalue);
}
static void BloodTimeFunc (void *unused)
{
	Cvar_SetValue ("cl_bloodtime", s_options_bloodtime_slider.curvalue);
}
static void PropsFunc (void *unused)
{
	Cvar_SetValue ("props", s_options_props_box.curvalue);
	Cbuf_AddText (s_options_props_box.curvalue ? "cmd hideprops\n" : "cmd showprops\n");
}

static void Options_InitSpin (menulist_s *box, int y, char *name,
		void (*callback)(void *), const char **itemnames)
{
	box->generic.type = MTYPE_SPINCONTROL;
	box->generic.x = 0;
	box->generic.y = y;
	box->generic.name = name;
	box->generic.callback = callback;
	box->itemnames = itemnames;
}
static void Options_InitSlider (menuslider_s *slider, int y, char *name,
		void (*callback)(void *), float minvalue, float maxvalue)
{
	slider->generic.type = MTYPE_SLIDER;
	slider->generic.x = 0;
	slider->generic.y = y;
	slider->generic.name = name;
	slider->generic.callback = callback;
	slider->minvalue = minvalue;
	slider->maxvalue = maxvalue;
}
#endif

static void CrosshairFunc( void *unused )
{
	Cvar_SetValue( "crosshair", s_options_crosshair_box.curvalue );
}

static void JoystickFunc( void *unused )
{
	Cvar_SetValue( "in_joystick", s_options_joystick_box.curvalue );
}

#ifdef __psp__
static void JoysticSetupFunc( void *unused )
{
	M_Menu_Joy_f();
}
#endif

static void CustomizeControlsFunc( void *unused )
{
	M_Menu_Keys_f();
}

static void AlwaysRunFunc( void *unused )
{
	Cvar_SetValue( "cl_run", s_options_alwaysrun_box.curvalue );
}

static void FreeLookFunc( void *unused )
{
	Cvar_SetValue( "freelook", s_options_freelook_box.curvalue );
}

static void MouseSpeedFunc( void *unused )
{
	Cvar_SetValue( "sensitivity", s_options_sensitivity_slider.curvalue / 2.0F );
}

#ifndef __psp__
static void NoAltTabFunc( void *unused )
{
	Cvar_SetValue( "win_noalttab", s_options_noalttab_box.curvalue );
}
#endif

static float ClampCvar( float min, float max, float value )
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

static void ControlsSetMenuItemValues( void )
{
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;
	s_options_cdvolume_slider.curvalue		= Cvar_VariableValue( "cd_volume" ) * 10;
	s_options_cdenable_box.curvalue 		= !Cvar_VariableValue("cd_nocd");
#ifndef __psp__
	s_options_quality_list.curvalue			= !Cvar_VariableValue( "s_loadas8bit" );
#endif
	s_options_sensitivity_slider.curvalue	= ( sensitivity->value ) * 2;

	Cvar_SetValue( "cl_run", ClampCvar( 0, 1, cl_run->value ) );
	s_options_alwaysrun_box.curvalue		= cl_run->value;

	s_options_invertmouse_box.curvalue		= m_pitch->value < 0;

	Cvar_SetValue( "lookspring", ClampCvar( 0, 1, lookspring->value ) );
	s_options_lookspring_box.curvalue		= lookspring->value;

	Cvar_SetValue( "lookstrafe", ClampCvar( 0, 1, lookstrafe->value ) );
	s_options_lookstrafe_box.curvalue		= lookstrafe->value;

	Cvar_SetValue( "freelook", ClampCvar( 0, 1, freelook->value ) );
	s_options_freelook_box.curvalue			= freelook->value;

	Cvar_SetValue( "crosshair", ClampCvar( 0, 3, crosshair->value ) );
	s_options_crosshair_box.curvalue		= crosshair->value;

#ifdef KINGPIN_ENGINE
	Cvar_SetValue ("hudtalk", ClampCvar (0, 1, hudtalk->value));
	s_options_hudtalk_box.curvalue = hudtalk->value;
	Cvar_SetValue ("hudguys", ClampCvar (0, 1, hudguys->value));
	s_options_hudguys_box.curvalue = hudguys->value;
	Cvar_SetValue ("cl_captions", ClampCvar (0, 1, Cvar_VariableValue ("cl_captions")));
	s_options_captions_box.curvalue = Cvar_VariableValue ("cl_captions");
	Cvar_SetValue ("r_directional_lighting", ClampCvar (0, 1, Cvar_VariableValue ("r_directional_lighting")));
	s_options_dirlight_box.curvalue = Cvar_VariableValue ("r_directional_lighting");
	Cvar_SetValue ("props", ClampCvar (0, 1, Cvar_VariableValue ("props")));
	s_options_props_box.curvalue = Cvar_VariableValue ("props");
	s_options_fireworks_slider.curvalue = ClampCvar (0, 3, Cvar_VariableValue ("cl_fireworks"));
	s_options_bloodtime_slider.curvalue = Cvar_VariableValue ("cl_bloodtime");
#endif

	Cvar_SetValue( "in_joystick", ClampCvar( 0, 1, in_joystick->value ) );
	s_options_joystick_box.curvalue		= in_joystick->value;

#ifndef __psp__
	s_options_noalttab_box.curvalue			= win_noalttab->value;
#endif
}

static void ControlsResetDefaultsFunc( void *unused )
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute();

	ControlsSetMenuItemValues();
}

static void InvertMouseFunc( void *unused )
{
	Cvar_SetValue( "m_pitch", -m_pitch->value );
}

static void LookspringFunc( void *unused )
{
	Cvar_SetValue( "lookspring", !lookspring->value );
}

static void LookstrafeFunc( void *unused )
{
	Cvar_SetValue( "lookstrafe", !lookstrafe->value );
}

static void UpdateVolumeFunc( void *unused )
{
	Cvar_SetValue( "s_volume", s_options_sfxvolume_slider.curvalue / 10 );
}

static void UpdateCDVolumeFunc( void *unused )
{
	Cvar_SetValue( "cd_volume", s_options_cdvolume_slider.curvalue / 10 );
}

static void UpdateCDEnableFunc( void *unused )
{
	Cvar_SetValue( "cd_nocd", !s_options_cdenable_box.curvalue );
}

static void ConsoleFunc( void *unused )
{
	/*
	** the proper way to do this is probably to have ToggleConsole_f accept a parameter
	*/
	extern void Key_ClearTyping( void );

	if ( cl.attractloop )
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;
}

#ifndef __psp__
static void UpdateSoundQualityFunc( void *unused )
{
	if ( s_options_quality_list.curvalue )
	{
		Cvar_SetValue( "s_khz", 22 );
		Cvar_SetValue( "s_loadas8bit", false );
	}
	else
	{
		Cvar_SetValue( "s_khz", 11 );
		Cvar_SetValue( "s_loadas8bit", true );
	}

	Cvar_SetValue( "s_primary", s_options_compatibility_list.curvalue );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  M_RUS("Restarting the sound system. This", "IEPE3AIYCK 3BYKA.") );
	M_Print( 16 + 16, 120 - 48 + 16, M_RUS("could take up to a minute, so", "{TO 3AZMET OKOLO MUHYT}.") );
	M_Print( 16 + 16, 120 - 48 + 24, M_RUS("please be patient.", "IOJALYZCTA, IODOJDUTE.") );

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	CL_Snd_Restart_f();
}
#endif

void Options_MenuInit( void )
{
	int		position_y;
	static const char *cd_music_items[] =
	{
		M_RUS("B}KLZ4EH", "B}KL@4EHO"),
		M_RUS("BKLZ4EH", "BKL@4EHO"),
		0
	};

#ifndef __psp__
	static const char *quality_items[] =
	{
		M_RUS("HU3", "HU3KOE"), M_RUS("B}C.", "B}COKOE"), 0
	};

	static const char *compatibility_items[] =
	{
		M_RUS("max COBMECTUMOCT|", "MAKC. COBMECTUMOCT|"), M_RUS("max CKOPOCT|", "MAKC. CKOPOCT|"), 0
	};
#endif

	static const char *yesno_names[] =
	{
		M_RUS("HET", "HET"),
		M_RUS("DA", "DA"),
		0
	};

	static const char *crosshair_names[] =
	{
		M_RUS("HET", "HET"),
		M_RUS("KPECT", "KPECT"),
		M_RUS("TO4KA", "TO4KA"),
		M_RUS("YGOL", "YGOL"),
		0
	};

#ifndef __psp__
	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );
#endif

	/*
	** configure controls menu and menu items
	*/
	s_options_menu.x = viddef.width / 2;
	s_options_menu.y = viddef.height / 2 - 58;
	s_options_menu.nitems = 0;

#ifdef KINGPIN_ENGINE
	s_options_menu.xoffset = 30;
	s_options_menu.yoffset = -15;
#endif

	position_y = 0;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x	= 0;
	s_options_sfxvolume_slider.generic.y	= position_y;
	s_options_sfxvolume_slider.generic.name	= M_RUS("effects volume", "GPOMK. {FFEKTOB");
	s_options_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue		= 0;
	s_options_sfxvolume_slider.maxvalue		= 10;
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;

	position_y += 10;

	s_options_cdvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_cdvolume_slider.generic.x		= 0;
	s_options_cdvolume_slider.generic.y		= position_y;
	s_options_cdvolume_slider.generic.name	= M_RUS("music volume", "GPOMK. MY3}KU");
	s_options_cdvolume_slider.generic.callback	= UpdateCDVolumeFunc;
	s_options_cdvolume_slider.minvalue		= 0;
	s_options_cdvolume_slider.maxvalue		= 10;
	s_options_cdvolume_slider.curvalue		= Cvar_VariableValue( "cd_volume" ) * 10;

	position_y += 10;

	s_options_cdenable_box.generic.type		= MTYPE_SPINCONTROL;
	s_options_cdenable_box.generic.x		= 0;
	s_options_cdenable_box.generic.y		= position_y;
#ifdef __psp__
	s_options_cdenable_box.generic.name		= M_RUS("MP3 music", "MY3}KA");
#else
	s_options_cdenable_box.generic.name		= M_RUS("CD music", "MY3}KA");
#endif
	s_options_cdenable_box.generic.callback	= UpdateCDEnableFunc;
	s_options_cdenable_box.itemnames		= cd_music_items;
	s_options_cdenable_box.curvalue 		= !Cvar_VariableValue("cd_nocd");

#ifndef __psp__
	position_y += 10;

	s_options_quality_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x		= 0;
	s_options_quality_list.generic.y		= position_y;
	s_options_quality_list.generic.name		= M_RUS("sound quality", "KA4ECTBO 3BYKA");
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames		= quality_items;
	s_options_quality_list.curvalue			= !Cvar_VariableValue( "s_loadas8bit" );

	position_y += 10;

	s_options_compatibility_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_compatibility_list.generic.x		= 0;
	s_options_compatibility_list.generic.y		= position_y;
	s_options_compatibility_list.generic.name	= M_RUS("sound compatibility", "COBMECTUMOCT| 3BYKA");
	s_options_compatibility_list.generic.callback = UpdateSoundQualityFunc;
	s_options_compatibility_list.itemnames		= compatibility_items;
	s_options_compatibility_list.curvalue		= Cvar_VariableValue( "s_primary" );
#endif

	position_y += 20;

	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= position_y;
	s_options_sensitivity_slider.generic.name	= M_RUS("mouse speed", "CKOPOCT| M}WU");
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 2;
	s_options_sensitivity_slider.maxvalue		= 22;

	position_y += 10;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x	= 0;
	s_options_alwaysrun_box.generic.y	= position_y;
	s_options_alwaysrun_box.generic.name	= M_RUS("always run", "VEG BCEGDA");
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	position_y += 10;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x	= 0;
	s_options_invertmouse_box.generic.y	= position_y;
	s_options_invertmouse_box.generic.name	= M_RUS("invert mouse", "UHBEPCUR M}WU");
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	position_y += 10;

	s_options_lookspring_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookspring_box.generic.x	= 0;
	s_options_lookspring_box.generic.y	= position_y;
	s_options_lookspring_box.generic.name	= M_RUS("lookspring", "BO3BPAT B3GLRDA");
	s_options_lookspring_box.generic.callback = LookspringFunc;
	s_options_lookspring_box.itemnames = yesno_names;

	position_y += 10;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x	= 0;
	s_options_lookstrafe_box.generic.y	= position_y;
	s_options_lookstrafe_box.generic.name	= M_RUS("lookstrafe", "M}W|: CDBUG");
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	position_y += 10;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x	= 0;
	s_options_freelook_box.generic.y	= position_y;
	s_options_freelook_box.generic.name	= M_RUS("free look", "CBOVODH}Z OV3OP");
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	position_y += 10;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x	= 0;
	s_options_crosshair_box.generic.y	= position_y;
	s_options_crosshair_box.generic.name	= M_RUS("crosshair", "IPUQEL");
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

#ifdef KINGPIN_ENGINE
	position_y += 10;
	Options_InitSpin (&s_options_hudtalk_box, position_y, M_RUS("talk icons", "3HA4KU PA3GOBOPA"), HudTalkFunc, yesno_names);
	position_y += 10;
	Options_InitSpin (&s_options_hudguys_box, position_y, M_RUS("hire icons", "3HA4KU HAEMHUKOB"), HudGuysFunc, yesno_names);
	position_y += 10;
	Options_InitSpin (&s_options_captions_box, position_y, M_RUS("captions", "CYVTUTP}"), CaptionsFunc, yesno_names);
	position_y += 20;
	Options_InitSlider (&s_options_fireworks_slider, position_y, M_RUS("fireworks", "FEZEPBEPK"), FireworksFunc, 0, 3);
	position_y += 10;
	Options_InitSpin (&s_options_dirlight_box, position_y, M_RUS("directional lighting", "HAIPABLEHH}Z CBET"), DirLightFunc, yesno_names);
	position_y += 10;
	Options_InitSlider (&s_options_bloodtime_slider, position_y, M_RUS("blood time", "BPEMR KPOBU"), BloodTimeFunc, 0, 10);
	position_y += 10;
	Options_InitSpin (&s_options_props_box, position_y, M_RUS("props", "CKP}T| IPEDMET}"), PropsFunc, yesno_names);
#endif

/*
#ifndef __psp__
	position_y += 10;

	s_options_noalttab_box.generic.type = MTYPE_SPINCONTROL;
	s_options_noalttab_box.generic.x	= 0;
	s_options_noalttab_box.generic.y	= position_y;
	s_options_noalttab_box.generic.name	= M_RUS("disable alt-tab", "OTKL@4UT| alt-tab");
	s_options_noalttab_box.generic.callback = NoAltTabFunc;
	s_options_noalttab_box.itemnames = yesno_names;
#endif
*/

	position_y += 10;

	s_options_joystick_box.generic.type = MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x	= 0;
	s_options_joystick_box.generic.y	= position_y;
	s_options_joystick_box.generic.name	= M_RUS("use joystick", "UCI. DJOZCTUK");
	s_options_joystick_box.generic.callback = JoystickFunc;
	s_options_joystick_box.itemnames = yesno_names;

	position_y += 20;

#ifdef __psp__
	s_options_joystick_setup_action.generic.type		= MTYPE_ACTION;
	s_options_joystick_setup_action.generic.x			= 0;
	s_options_joystick_setup_action.generic.y			= position_y;
	s_options_joystick_setup_action.generic.name		= M_RUS("joystick setup", "HACTPOZKA CTUKA");
	s_options_joystick_setup_action.generic.callback	= JoysticSetupFunc;

	position_y += 10;
#endif

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x		= 0;
	s_options_customize_options_action.generic.y		= position_y;
	s_options_customize_options_action.generic.name	= M_RUS("customize controls", "HACTPOUT| YIPABLEHUE");
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	position_y += 10;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x		= 0;
	s_options_defaults_action.generic.y		= position_y;
	s_options_defaults_action.generic.name	= M_RUS("reset defaults", "CVPOCUT| HACTPOZKU");
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	position_y += 10;

	s_options_console_action.generic.type	= MTYPE_ACTION;
	s_options_console_action.generic.x		= 0;
	s_options_console_action.generic.y		= position_y;
	s_options_console_action.generic.name	= M_RUS("go to console", "KOHCOL|");
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues();

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sfxvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_cdvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_cdenable_box );
#ifndef __psp__
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_quality_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_compatibility_list );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sensitivity_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_alwaysrun_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_invertmouse_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookspring_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookstrafe_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_freelook_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_crosshair_box );
#ifdef KINGPIN_ENGINE
	Menu_AddItem (&s_options_menu, &s_options_hudtalk_box);
	Menu_AddItem (&s_options_menu, &s_options_hudguys_box);
	Menu_AddItem (&s_options_menu, &s_options_captions_box);
	Menu_AddItem (&s_options_menu, &s_options_fireworks_slider);
	Menu_AddItem (&s_options_menu, &s_options_dirlight_box);
	Menu_AddItem (&s_options_menu, &s_options_bloodtime_slider);
	Menu_AddItem (&s_options_menu, &s_options_props_box);
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_joystick_box );
#ifdef __psp__
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_joystick_setup_action );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}

void Options_MenuDraw (void)
{
	M_Banner( "m_banner_options" );
	Menu_AdjustCursor( &s_options_menu, 1 );
	Menu_Draw( &s_options_menu );
}

const char *Options_MenuKey( int key )
{
	return Default_MenuKey( &s_options_menu, key );
}

void M_Menu_Options_f (void)
{
	Options_MenuInit();
	M_PushMenu ( Options_MenuDraw, Options_MenuKey );
}

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

void M_Menu_Video_f (void)
{
	VID_MenuInit();
	M_PushMenu( VID_MenuDraw, VID_MenuKey );
}

/*
=============================================================================

END GAME MENU

=============================================================================
*/
static int credits_start_time;
static const char **credits;
#ifdef KINGPIN_ENGINE
// kingpin.exe keeps its 453 line credits (@00459e9c) inside the executable;
// the port reads them from a "credits" file, which re/extract_credits.py
// writes from the original executable. Without it only the title shows.
#define MAX_CREDITS_LINES	1024
static char *creditsIndex[MAX_CREDITS_LINES];
static const char *kpcredits[] =
{
	"+KINGPIN",
	"+LIFE OF CRIME",
	"",
	"+BY",
	"",
	"+XATRIX ENTERTAINMENT, INC.",
	0
};
#else
static char *creditsIndex[256];
#endif
static char *creditsBuffer;
static const char *idcredits[] =
{
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

static const char *xatcredits[] =
{
	"+QUAKE II MISSION PACK: THE RECKONING",
	"+BY",
	"+XATRIX ENTERTAINMENT, INC.",
	"",
	"+DESIGN AND DIRECTION",
	"Drew Markham",
	"",
	"+PRODUCED BY",
	"Greg Goodrich",
	"",
	"+PROGRAMMING",
	"Rafael Paiz",
	"",
	"+LEVEL DESIGN / ADDITIONAL GAME DESIGN",
	"Alex Mayberry",
	"",
	"+LEVEL DESIGN",
	"Mal Blackwell",
	"Dan Koppel",
	"",
	"+ART DIRECTION",
	"Michael \"Maxx\" Kaufman",
	"",
	"+COMPUTER GRAPHICS SUPERVISOR AND",
	"+CHARACTER ANIMATION DIRECTION",
	"Barry Dempsey",
	"",
	"+SENIOR ANIMATOR AND MODELER",
	"Jason Hoover",
	"",
	"+CHARACTER ANIMATION AND",
	"+MOTION CAPTURE SPECIALIST",
	"Amit Doron",
	"",
	"+ART",
	"Claire Praderie-Markham",
	"Viktor Antonov",
	"Corky Lehmkuhl",
	"",
	"+INTRODUCTION ANIMATION",
	"Dominique Drozdz",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Aaron Barber",
	"Rhett Baldwin",
	"",
	"+3D CHARACTER ANIMATION TOOLS",
	"Gerry Tyra, SA Technology",
	"",
	"+ADDITIONAL EDITOR TOOL PROGRAMMING",
	"Robert Duffy",
	"",
	"+ADDITIONAL PROGRAMMING",
	"Ryan Feltrin",
	"",
	"+PRODUCTION COORDINATOR",
	"Victoria Sylvester",
	"",
	"+SOUND DESIGN",
	"Gary Bradfield",
	"",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Dave \"Zoid\" Kirsch",
	"Donna Jackson",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk \"The Original Ripper\" Hartong",
	"Kevin Kraff",
	"Jamey Gottlieb",
	"Chris Hepburn",
	"",
	"+AND THE GAME TESTERS",
	"",
	"Tim Vanlaw",
	"Doug Jacobs",
	"Steven Rosenthal",
	"David Baker",
	"Chris Campbell",
	"Aaron Casillas",
	"Steve Elwell",
	"Derek Johnstone",
	"Igor Krinitskiy",
	"Samantha Lee",
	"Michael Spann",
	"Chris Toft",
	"Juan Valdes",
	"",
	"+THANKS TO INTERGRAPH COMPUTER SYTEMS",
	"+IN PARTICULAR:",
	"",
	"Michael T. Nicolaou",
	"",
	"",
	"Quake II Mission Pack: The Reckoning",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Xatrix",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack: The",
	"Reckoning(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Xatrix(R) is a registered",
	"trademark of Xatrix Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static const char *roguecredits[] =
{
	"+QUAKE II MISSION PACK 2: GROUND ZERO",
	"+BY",
	"+ROGUE ENTERTAINMENT, INC.",
	"",
	"+PRODUCED BY",
	"Jim Molinets",
	"",
	"+PROGRAMMING",
	"Peter Mack",
	"Patrick Magruder",
	"",
	"+LEVEL DESIGN",
	"Jim Molinets",
	"Cameron Lamprecht",
	"Berenger Fish",
	"Robert Selitto",
	"Steve Tietze",
	"Steve Thoms",
	"",
	"+ART DIRECTION",
	"Rich Fleider",
	"",
	"+ART",
	"Rich Fleider",
	"Steve Maines",
	"Won Choi",
	"",
	"+ANIMATION SEQUENCES",
	"Creat Studios",
	"Steve Maines",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Rich Fleider",
	"Steve Maines",
	"Peter Mack",
	"",
	"+SOUND",
	"James Grunke",
	"",
	"+GROUND ZERO THEME",
	"+AND",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"+VWEP MODELS",
	"Brent \"Hentai\" Dill",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Katherine Anna Kang",
	"Donna Jackson",
	"Dave \"Zoid\" Kirsch",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk Hartong",
	"Mitch Lasky",
	"Steve Rosenthal",
	"Steve Elwell",
	"",
	"+AND THE GAME TESTERS",
	"",
	"The Ranger Clan",
	"Dave \"Zoid\" Kirsch",
	"Nihilistic Software",
	"Robert Duffy",
	"",
	"And Countless Others",
	"",
	"",
	"",
	"Quake II Mission Pack 2: Ground Zero",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Rogue",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack 2: Ground",
	"Zero(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Rogue(R) is a registered",
	"trademark of Rogue Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};


void M_Credits_MenuDraw( void )
{
	int i, y;

#ifdef KINGPIN_ENGINE
	// kingpin.exe @0042acf0
	re.DrawStretchPic (0, 0, viddef.width, viddef.height, "credits");
#endif

	/*
	** draw the credits
	*/
	for ( i = 0, y = viddef.height - ( ( cls.realtime - credits_start_time ) / 40.0F ); credits[i] && y < viddef.height; y += 10, i++ )
	{
		int j, stringoffset = 0;
		int bold = false;

		if ( y <= -8 )
			continue;

		if ( credits[i][0] == '+' )
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for ( j = 0; credits[i][j+stringoffset]; j++ )
		{
			int x;

			x = ( viddef.width - strlen( credits[i] ) * 8 - stringoffset * 8 ) / 2 + ( j + stringoffset ) * 8;

			if ( bold )
				re.DrawChar( x, y, credits[i][j+stringoffset] + 128 );
			else
				re.DrawChar( x, y, credits[i][j+stringoffset] );
		}
	}

	if ( y < 0 )
		credits_start_time = cls.realtime;
}

const char *M_Credits_Key( int key )
{
	switch (key)
	{
	case K_ESCAPE:
	case K_START_BUTTON:
	case K_B_BUTTON:
		if (creditsBuffer)
			FS_FreeFile (creditsBuffer);
		M_PopMenu ();
		break;
	}

	return menu_out_sound;

}

extern int Developer_searchpath (int who);

void M_Menu_Credits_f( void )
{
	int		n;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = FS_LoadFile ("credits", &count, FS_PATH_ALL);
	if (creditsBuffer)
	{
		p = creditsBuffer;
		for (n = 0; n < 255; n++)
		{
			creditsIndex[n] = p;
			while (*p != '\r' && *p != '\n')
			{
				p++;
				if (--count == 0)
					break;
			}
			if (*p == '\r')
			{
				*p++ = 0;
				if (--count == 0)
					break;
			}
			*p++ = 0;
			if (--count == 0)
				break;
		}
		creditsIndex[++n] = 0;
		credits = (const char **)creditsIndex;
	}
	else
	{
#ifdef KINGPIN_ENGINE
		credits = kpcredits;
#else
		isdeveloper = Developer_searchpath (1);

		if (isdeveloper == 1)			// xatrix
			credits = xatcredits;
		else if (isdeveloper == 2)		// ROGUE
			credits = roguecredits;
		else
		{
			credits = idcredits;
		}
#endif
	}

	credits_start_time = cls.realtime;
	M_PushMenu( M_Credits_MenuDraw, M_Credits_Key);
}

/*
=============================================================================

GAME MENU

=============================================================================
*/

static int		m_game_cursor;

static menuframework_s	s_game_menu;
#ifdef KINGPIN_ENGINE
static menuaction_s		s_novice_game_action;
static menuaction_s		s_real_game_action;
static menuaction_s		s_notebook_action;

/* Layout replies are sent even while the local server is paused. Keep the
 * game's overlay closed; this menu draws picnote without STAT_LAYOUTS. */
static void Notebook_Command (char *command)
{
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, command);
}

static qboolean Notebook_Available (void)
{
	return cls.state == ca_active && cl.refresh_prepped &&
		!cl.cinematictime && Com_ServerState () &&
		!Cvar_VariableValue ("deathmatch");
}

static void Notebook_Text (const char *text, int x, int y, int bottom)
{
	int column = 0;
	unsigned char c;

	text = CL_LocalizeNotebookText (text);

	while (*text && y + 8 <= bottom)
	{
		c = (unsigned char)*text++;
		if (c == '\r')
			continue;
		if (c == '\n' || column == 36)
		{
			y += 12;
			column = 0;
			if (c == '\n')
				continue;
			if (y + 8 > bottom)
				break;
		}
		/* Menu glyphs are printed capitals; the notebook atlas is handwritten. */
		re.DrawCharUpper (x + column * 8, y, c);
		column++;
	}
}

static void Notebook_Draw (void)
{
	char *layout, *token;
	int width = 320;
	int x = (viddef.width - width) / 2;
	int y = (viddef.height - 224) / 2;

	M_Main_Draw ();
	M_PrintWhite (128, 0, M_RUS("objectives", "3ADAHUR"));
	if (!Notebook_Available ())
	{
		M_PrintWhite (32, 104, M_RUS("Start a single player game", "HA4HUTE ODUHO4HY@ UGPY"));
		return;
	}

	re.DrawStretchPic (x, y, width, 224, "note");
	/* Opaque backing keeps the printed white menu font clear over gameplay. */
	re.DrawFillRGBA (x + 10, y + 10, width - 20, 204, 0.025f, 0.025f, 0.025f, 1.0f);
	layout = cl.layout;
	token = COM_Parse (&layout);
	if (!strcmp (token, "picnote"))
	{
		token = COM_Parse (&layout);
		Notebook_Text (token, x + 16, y + 16, y + 48);
		token = COM_Parse (&layout);
		Notebook_Text (token, x + 16, y + 52, y + 212);
	}
	M_PrintWhite (0, 232, M_RUS("LEFT/RIGHT: pages   CIRCLE: back", "VLEBO/BIPABO: LUCT   O: HA3AD"));
}

static const char *Notebook_Key (int key)
{
	if (Notebook_Available ())
	{
		switch (key)
		{
		case K_LEFTARROW:
		case K_KP_LEFTARROW:
		case K_L1_BUTTON:
			Notebook_Command ("invprev");
			Notebook_Command ("putaway");
			return menu_move_sound;
		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
		case K_R1_BUTTON:
			Notebook_Command ("invnext");
			Notebook_Command ("putaway");
			return menu_move_sound;
		}
	}
	return Default_MenuKey (NULL, key);
}

static void M_Menu_Notebook_f (void)
{
	if (Notebook_Available ())
	{
		cl.layout[0] = 0;
		Notebook_Command ("putaway");
		Notebook_Command ("help");
		Notebook_Command ("putaway");
	}
	M_PushMenu (Notebook_Draw, Notebook_Key);
}

static void NotebookFunc (void *unused)
{
	M_Menu_Notebook_f ();
}
#endif
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;

static void StartGame( void )
{
	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff ();
	Cvar_SetValue( "deathmatch", 0 );

#ifndef KINGPIN_ENGINE
	Cvar_SetValue( "coop", 0 );
	Cvar_SetValue( "gamerules", 0 );		//PGM
#endif

	Cbuf_AddText ("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

#ifdef KINGPIN_ENGINE
// kingpin.exe @0042b120..@0042b1e0: five skill levels, 0 to 4 (the
// reference executable's translation renamed the cvar to "HAB_K", which
// the game dll does not read; the original "skill" is used)
static void NoviceGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "0" );
	StartGame();
}

static void EasyGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "1" );
	StartGame();
}

static void MediumGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "2" );
	StartGame();
}

static void HardGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "3" );
	StartGame();
}

static void RealGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "4" );
	StartGame();
}
#else
static void EasyGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "0" );
	StartGame();
}

static void MediumGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "1" );
	StartGame();
}

static void HardGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "2" );
	StartGame();
}
#endif

static void LoadGameFunc( void *unused )
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc( void *unused )
{
	M_Menu_SaveGame_f();
}

static void CreditsFunc( void *unused )
{
	M_Menu_Credits_f();
}

#ifdef KINGPIN_ENGINE
void Game_MenuInit( void )
{
	static const char *difficulty_names[] =
	{
		M_RUS("novice", "HOBU4OK"),
		M_RUS("easy", "LEGKO"),
		M_RUS("medium", "CPEDHE"),
		M_RUS("hard", "CLOJHO"),
		M_RUS("real", "PEAL|HO"),
		0
	};

	s_game_menu.x = viddef.width * 0.50;
	s_game_menu.nitems = 0;
	s_game_menu.xoffset = -50;
	s_game_menu.yoffset = 0;

	s_novice_game_action.generic.type	= MTYPE_ACTION;
	s_novice_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_novice_game_action.generic.x		= 0;
	s_novice_game_action.generic.y		= 0;
	s_novice_game_action.generic.name	= M_RUS("novice", "HOBU4OK");
	s_novice_game_action.generic.callback = NoviceGameFunc;

	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= 0;
	s_easy_game_action.generic.y		= 10;
	s_easy_game_action.generic.name	= M_RUS("easy", "LEGKO");
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= 0;
	s_medium_game_action.generic.y		= 20;
	s_medium_game_action.generic.name	= M_RUS("medium", "CPEDHE");
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= 0;
	s_hard_game_action.generic.y		= 30;
	s_hard_game_action.generic.name	= M_RUS("hard", "CLOJHO");
	s_hard_game_action.generic.callback = HardGameFunc;

	s_real_game_action.generic.type	= MTYPE_ACTION;
	s_real_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_real_game_action.generic.x		= 0;
	s_real_game_action.generic.y		= 41;
	s_real_game_action.generic.name	= M_RUS("real", "PEAL|HO");
	s_real_game_action.generic.callback = RealGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= 0;
	s_load_game_action.generic.y		= 70;
	s_load_game_action.generic.name	= M_RUS("load game", "3AGPY3UT|");
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= 0;
	s_save_game_action.generic.y		= 80;
	s_save_game_action.generic.name	= M_RUS("save game", "COXPAHUT|");
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x		= 0;
	s_credits_action.generic.y		= 95;
	s_credits_action.generic.name	= M_RUS("credits", "TUTP}");
	s_credits_action.generic.callback = CreditsFunc;

	s_notebook_action.generic.type = MTYPE_ACTION;
	s_notebook_action.generic.flags = QMF_LEFT_JUSTIFY;
	s_notebook_action.generic.x = 0;
	s_notebook_action.generic.y = -20;
	s_notebook_action.generic.name = M_RUS("objectives", "3ADAHUR");
	s_notebook_action.generic.callback = NotebookFunc;

	if (Notebook_Available ())
		Menu_AddItem( &s_game_menu, &s_notebook_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_novice_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_easy_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_medium_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_hard_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_real_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_load_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_save_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_credits_action );

	Menu_Center( &s_game_menu );
	s_game_menu.cursor = Notebook_Available () ? 0 : 2;
}
#else
void Game_MenuInit( void )
{
	static const char *difficulty_names[] =
	{
		M_RUS("easy", "LEGKO"),
		M_RUS("medium", "CPEDHE"),
		M_RUS("hard", "CLOJHO"),
		0
	};

	s_game_menu.x = viddef.width * 0.50;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= 0;
	s_easy_game_action.generic.y		= 0;
	s_easy_game_action.generic.name	= M_RUS("easy", "LEGKO");
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= 0;
	s_medium_game_action.generic.y		= 10;
	s_medium_game_action.generic.name	= M_RUS("medium", "CPEDHE");
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= 0;
	s_hard_game_action.generic.y		= 20;
	s_hard_game_action.generic.name	= M_RUS("hard", "CLOJHO");
	s_hard_game_action.generic.callback = HardGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= 0;
	s_load_game_action.generic.y		= 40;
	s_load_game_action.generic.name	= M_RUS("load game", "3AGPY3UT|");
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= 0;
	s_save_game_action.generic.y		= 50;
	s_save_game_action.generic.name	= M_RUS("save game", "COXPAHUT|");
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x		= 0;
	s_credits_action.generic.y		= 60;
	s_credits_action.generic.name	= M_RUS("credits", "TUTP}");
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem( &s_game_menu, ( void * ) &s_easy_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_medium_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_hard_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_load_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_save_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_credits_action );

	Menu_Center( &s_game_menu );
}
#endif

void Game_MenuDraw( void )
{
	M_Banner( "m_banner_game" );
	Menu_AdjustCursor( &s_game_menu, 1 );
	Menu_Draw( &s_game_menu );
}

const char *Game_MenuKey( int key )
{
	return Default_MenuKey( &s_game_menu, key );
}

void M_Menu_Game_f (void)
{
	Game_MenuInit();
	M_PushMenu( Game_MenuDraw, Game_MenuKey );
	m_game_cursor = 1;
}

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

#define	MAX_SAVEGAMES	15

static menuframework_s	s_savegame_menu;

static menuframework_s	s_loadgame_menu;
static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];

char		m_savestrings[MAX_SAVEGAMES][32];
qboolean	m_savevalid[MAX_SAVEGAMES];

void Create_Savestrings (void)
{
	int		i;
	file_t	*file;
	char	name[MAX_OSPATH];

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		Com_sprintf (name, sizeof(name), "save/save%i/server.ssv", i);
		file = FS_FOpen (name, FS_MODE_READ | FS_PATH_GAMEDIR | FS_TYPE_RFS);
		if (!file)
		{
			strcpy (m_savestrings[i], M_RUS("<EMPTY>", "<IYCTO>"));
			m_savevalid[i] = false;
		}
		else
		{
			FS_FRead (file, m_savestrings[i], sizeof(m_savestrings[i]));
			FS_FClose (file);
			m_savevalid[i] = true;
		}
	}
}

void LoadGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	if ( m_savevalid[ a->generic.localdata[0] ] )
		Cbuf_AddText (va("load save%i\n",  a->generic.localdata[0] ) );
	M_ForceMenuOff ();
}

void LoadGame_MenuInit( void )
{
	int i;

	s_loadgame_menu.x = viddef.width / 2 - 120;
	s_loadgame_menu.y = viddef.height / 2 - 58;
	s_loadgame_menu.nitems = 0;
#ifdef KINGPIN_ENGINE
	s_loadgame_menu.xoffset = 90;	// kingpin.exe @0042b370
	s_loadgame_menu.yoffset = -10;
#endif

	Create_Savestrings();

	for ( i = 0; i < MAX_SAVEGAMES; i++ )
	{
		s_loadgame_actions[i].generic.name			= m_savestrings[i];
		s_loadgame_actions[i].generic.flags			= QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0]	= i;
		s_loadgame_actions[i].generic.callback		= LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = ( i ) * 10;
		if (i>0)	// separate from autosave
			s_loadgame_actions[i].generic.y += 10;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_loadgame_menu, &s_loadgame_actions[i] );
	}
#ifdef KINGPIN_ENGINE
	// kingpin.exe @0042b370 applies xoff/yoff after all slots are present.
	Menu_Center( &s_loadgame_menu );
#endif
}

void LoadGame_MenuDraw( void )
{
	M_Banner( "m_banner_load_game" );
//	Menu_AdjustCursor( &s_loadgame_menu, 1 );
	Menu_Draw( &s_loadgame_menu );
}

const char *LoadGame_MenuKey( int key )
{
	if ( key == K_ENTER || key == K_A_BUTTON || key == K_ESCAPE  || key == K_START_BUTTON || key == K_B_BUTTON )
	{
		s_savegame_menu.cursor = s_loadgame_menu.cursor - 1;
		if ( s_savegame_menu.cursor < 0 )
			s_savegame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_loadgame_menu, key );
}

void M_Menu_LoadGame_f (void)
{
	LoadGame_MenuInit();
	M_PushMenu( LoadGame_MenuDraw, LoadGame_MenuKey );
}


/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/
static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];

void SaveGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	Cbuf_AddText (va("save save%i\n", a->generic.localdata[0] ));
	M_ForceMenuOff ();
}

void SaveGame_MenuDraw( void )
{
	M_Banner( "m_banner_save_game" );
	Menu_AdjustCursor( &s_savegame_menu, 1 );
	Menu_Draw( &s_savegame_menu );
}

void SaveGame_MenuInit( void )
{
	int i;

	s_savegame_menu.x = viddef.width / 2 - 120;
	s_savegame_menu.y = viddef.height / 2 - 58;
	s_savegame_menu.nitems = 0;
#ifdef KINGPIN_ENGINE
	s_savegame_menu.xoffset = 80;	// kingpin.exe @0042b500
	s_savegame_menu.yoffset = -10;
#endif

	Create_Savestrings();

	// don't include the autosave slot
	for ( i = 0; i < MAX_SAVEGAMES-1; i++ )
	{
		s_savegame_actions[i].generic.name = m_savestrings[i+1];
		s_savegame_actions[i].generic.localdata[0] = i+1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = ( i ) * 10;

		s_savegame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_savegame_menu, &s_savegame_actions[i] );
	}
#ifdef KINGPIN_ENGINE
	// kingpin.exe @0042b500 applies xoff/yoff after all slots are present.
	Menu_Center( &s_savegame_menu );
#endif
}

const char *SaveGame_MenuKey( int key )
{
	if ( key == K_ENTER || key == K_A_BUTTON || key == K_ESCAPE  || key == K_START_BUTTON || key == K_B_BUTTON )
	{
		s_loadgame_menu.cursor = s_savegame_menu.cursor - 1;
		if ( s_loadgame_menu.cursor < 0 )
			s_loadgame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_savegame_menu, key );
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState())
		return;		// not playing a game

	SaveGame_MenuInit();
	M_PushMenu( SaveGame_MenuDraw, SaveGame_MenuKey );
	Create_Savestrings ();
}


/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
#define MAX_LOCAL_SERVERS 8

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int		m_num_servers;
#define	NO_SERVER_STRING	M_RUS("<no server>", "<HET CEPBEPA>")

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

void M_AddToServerList (netadr_t adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;
	while ( *info == ' ' )
		info++;

	// ignore if duplicated
	for (i=0 ; i<m_num_servers ; i++)
		if (!strcmp(info, local_server_names[i]))
			return;

	local_server_netadr[m_num_servers] = adr;
	strncpy (local_server_names[m_num_servers], info, sizeof(local_server_names[0])-1);
	m_num_servers++;
}


void JoinServerFunc( void *self )
{
	char	buffer[128];
	int		index;

	index = ( menuaction_s * ) self - s_joinserver_server_actions;

	if ( Q_stricmp( local_server_names[index], NO_SERVER_STRING ) == 0 )
		return;

	if (index >= m_num_servers)
		return;

	Com_sprintf (buffer, sizeof(buffer), "connect %s\n", NET_AdrToString (local_server_netadr[index]));
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

void AddressBookFunc( void *self )
{
	M_Menu_AddressBook_f();
}

void NullCursorDraw( void *self )
{
}

void SearchLocalGames( void )
{
	int		i;

	m_num_servers = 0;
	for (i=0 ; i<MAX_LOCAL_SERVERS ; i++)
		strcpy (local_server_names[i], NO_SERVER_STRING);

#ifdef KINGPIN_ENGINE
	// kingpin.exe @0042b770: no text box
	M_Print( 180, 192, M_RUS("Searching for local servers, this", "IOUCK LOKAL|H}X CEPBEPOB.") );
	M_Print( 180, 200, M_RUS("could take up to a minute, so", "{TO 3AZMET OKOLO MUHYT}.") );
	M_Print( 180, 208, M_RUS("please be patient.", "IOJALYZCTA, IODOJDUTE.") );
#else
	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  M_RUS("Searching for local servers, this", "IOUCK LOKAL|H}X CEPBEPOB.") );
	M_Print( 16 + 16, 120 - 48 + 16, M_RUS("could take up to a minute, so", "{TO 3AZMET OKOLO MUHYT}.") );
	M_Print( 16 + 16, 120 - 48 + 24, M_RUS("please be patient.", "IOJALYZCTA, IODOJDUTE.") );
#endif

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	// send out info packets
	CL_PingServers_f();
}

void SearchLocalGamesFunc( void *self )
{
	SearchLocalGames();
}

void JoinServer_MenuInit( void )
{
	int i;

	s_joinserver_menu.x = viddef.width * 0.50 - 120;
	s_joinserver_menu.nitems = 0;
#ifdef KINGPIN_ENGINE
	s_joinserver_menu.xoffset = 70;	// kingpin.exe @0042b810
	s_joinserver_menu.yoffset = 0;
#endif

	s_joinserver_address_book_action.generic.type	= MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name	= M_RUS("address book", "ADPECHAR KHUGA");
	s_joinserver_address_book_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x		= 0;
	s_joinserver_address_book_action.generic.y		= 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= M_RUS("refresh server list", "OVHOBUT| CIUCOK");
	s_joinserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x	= 0;
	s_joinserver_search_action.generic.y	= 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = M_RUS("search for servers", "IOUCK CEPBEPOB");

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = M_RUS("connect to...", "IODKL@4UT|CR K...");
	s_joinserver_server_title.generic.x    = 80;
	s_joinserver_server_title.generic.y	   = 30;

	for ( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		strcpy (local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x		= 0;
		s_joinserver_server_actions[i].generic.y		= 40 + i*10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = M_RUS("press ENTER to connect", "HAJMUTE KPECT DLR BXODA");
	}

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_address_book_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_title );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_action );

	for ( i = 0; i < 8; i++ )
		Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_actions[i] );

	Menu_Center( &s_joinserver_menu );

	SearchLocalGames();
}

void JoinServer_MenuDraw(void)
{
	M_Banner( "m_banner_join_server" );
	Menu_Draw( &s_joinserver_menu );
}


const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( JoinServer_MenuDraw, JoinServer_MenuKey );
}


/*
=============================================================================

START SERVER MENU

=============================================================================
*/
#ifdef KINGPIN_ENGINE
/*
kingpin.exe @0042ba20..@0042c360. Rules are deathmatch or bagman
(teamplay 1), each with its own map list (maps.lst, teammaps.lst), and a
cash limit field.
*/
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_cashlimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc( void *self )
{
	M_Menu_DMOptions_f();
}

static void StartServer_FreeMaps( void )
{
	int i;

	if ( mapnames )
	{
		for ( i = 0; i < nummaps; i++ )
			free( mapnames[i] );
		free( mapnames );
	}
	mapnames = 0;
	nummaps = 0;
}

// kingpin.exe @0042ba20: "long name" "short name" pairs, one per line
static void StartServer_LoadMaps( void )
{
	static char *listnames[2] = { "maps", "teammaps" };
	char *buffer;
	char  mapsname[MAX_QPATH];
	char *s;
	int length;
	int i;
	FILE *fp;

	StartServer_FreeMaps ();

#if 1
	Com_sprintf( mapsname, sizeof( mapsname ), "%s.lst", listnames[s_rules_box.curvalue] );

	if (!(buffer = FS_LoadFile(mapsname, &length, FS_PATH_GAMEDIR | FS_TYPE_RFS)))
	{
		if (!(buffer = FS_LoadFile(mapsname, &length, FS_PATH_ALL)))
			Com_Error( ERR_DROP, "couldn't find %s.lst\n", listnames[s_rules_box.curvalue] );
	}
#else
	Com_sprintf( mapsname, sizeof( mapsname ), "%s/%s.lst", FS_Gamedir(), listnames[s_rules_box.curvalue] );
	if ( ( fp = fopen( mapsname, "rb" ) ) == 0 )
	{
		if ( ( length = FS_LoadFile( va( "%s.lst", listnames[s_rules_box.curvalue] ), ( void ** ) &buffer ) ) == -1 )
			Com_Error( ERR_DROP, va( "couldn't find %s.lst\n", listnames[s_rules_box.curvalue] ) );
	}
	else
	{
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buffer = malloc( length );
		fread( buffer, length, 1, fp );
	}
#endif
	for ( i = 0; i < length; i++ )
		if ( buffer[i] == '\r' )
			nummaps++;

	if ( nummaps == 0 )
		Com_Error( ERR_DROP, va( "no maps in %s.lst\n", listnames[s_rules_box.curvalue] ) );

	mapnames = malloc( sizeof( char * ) * ( nummaps + 1 ) );
	memset( mapnames, 0, sizeof( char * ) * ( nummaps + 1 ) );

	s = buffer;

	for ( i = 0; i < nummaps; i++ )
	{
		char  shortname[MAX_TOKEN_CHARS];
		char  longname[MAX_TOKEN_CHARS];
		char  scratch[200];
		int		j, l;

		strcpy( shortname, COM_Parse( &s ) );
		l = strlen(shortname);
		for (j=0 ; j<l ; j++)
			shortname[j] = toupper(shortname[j]);
		strcpy( longname, COM_Parse( &s ) );
		Com_sprintf( scratch, sizeof( scratch ), "%s\n%s", longname, shortname );

		mapnames[i] = malloc( strlen( scratch ) + 1 );
		strcpy( mapnames[i], scratch );
	}
	mapnames[nummaps] = 0;

	if ( fp != 0 )
	{
		fclose( fp );
		free( buffer );
	}
	else
	{
		FS_FreeFile( buffer );
	}
}

// kingpin.exe @0042bd20: switch map lists with the rules
void RulesChangeFunc ( void *self )
{
	s_maxclients_field.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.statusbar = NULL;

	StartServer_LoadMaps ();
	s_startmap_list.curvalue = 0;
	s_startmap_list.itemnames = (const char **)mapnames;
}

// kingpin.exe @0042bd50
void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		cashlimit;
	int		maxclients;

	strcpy( startmap, strchr( mapnames[s_startmap_list.curvalue], '\n' ) + 1 );

	maxclients  = atoi( s_maxclients_field.buffer );
	timelimit	= atoi( s_timelimit_field.buffer );
	fraglimit	= atoi( s_fraglimit_field.buffer );
	cashlimit	= atoi( s_cashlimit_field.buffer );

	Cvar_SetValue( "maxclients", ClampCvar( 0, maxclients, maxclients ) );
	Cvar_SetValue ("timelimit", ClampCvar( 0, timelimit, timelimit ) );
	Cvar_SetValue ("fraglimit", ClampCvar( 0, fraglimit, fraglimit ) );
	Cvar_SetValue ("cashlimit", ClampCvar( 0, cashlimit, cashlimit ) );
	Cvar_Set("hostname", s_hostname_field.buffer );
	Cvar_SetValue ("deathmatch", 1 );
	Cvar_SetValue ("teamplay", s_rules_box.curvalue );
	Cvar_SetValue ("coop", 0 );

	Cbuf_AddText (va("map %s\n", startmap));

	M_ForceMenuOff ();
}

static void StartServer_InitField( menufield_s *f, int y, char *name, char *cvar, int length, int flags )
{
	f->generic.type = MTYPE_FIELD;
	f->generic.name = name;
	f->generic.flags = flags;
	f->generic.x	= 0;
	f->generic.y	= y;
	f->generic.statusbar = flags ? M_RUS("0 = no limit", "0 = VE3 OGPAHU4EHUZ") : NULL;
	f->length = length;
	f->visible_length = length;
	strcpy( f->buffer, Cvar_VariableString(cvar) );
}

// kingpin.exe @0042bed0
void StartServer_MenuInit( void )
{
	static const char *rules_names[] =
	{
		M_RUS("deathmatch", "VOZ HACMEPT|"),
		M_RUS("bagman", "MEWOK DEHEG"),
		0
	};

	s_startserver_menu.x = viddef.width * 0.50;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= 0;
	s_startmap_list.generic.name	= M_RUS("initial map", "HA4AL|HAR KAPTA");

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	= 0;
	s_rules_box.generic.y	= 20;
	s_rules_box.generic.name	= M_RUS("rules", "IPABULA");
	s_rules_box.itemnames = rules_names;
	s_rules_box.curvalue = Cvar_VariableValue("teamplay") != 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	StartServer_LoadMaps ();
	s_startmap_list.itemnames = (const char **)mapnames;

	StartServer_InitField( &s_timelimit_field, 36, M_RUS("time limit", "LUMUT BPEMEHU"), "timelimit", 3, QMF_NUMBERSONLY );
	StartServer_InitField( &s_fraglimit_field, 54, M_RUS("frag limit", "LUMUT YVUZCTB"), "fraglimit", 3, QMF_NUMBERSONLY );
	StartServer_InitField( &s_cashlimit_field, 72, M_RUS("cash limit", "LUMUT DEHEG"), "cashlimit", 5, QMF_NUMBERSONLY );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is.
	*/
	StartServer_InitField( &s_maxclients_field, 90, M_RUS("max players", "MAKC. UGPOKOB"), "maxclients", 3, QMF_NUMBERSONLY );
	s_maxclients_field.generic.statusbar = NULL;
	if ( Cvar_VariableValue( "maxclients" ) == 1 )
		strcpy( s_maxclients_field.buffer, "8" );

	StartServer_InitField( &s_hostname_field, 108, M_RUS("hostname", "UMR CEPBEPA"), "hostname", 12, 0 );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= M_RUS(" deathmatch flags", " IPABULA VOR");
	s_startserver_dmoptions_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	= 24;
	s_startserver_dmoptions_action.generic.y	= 135;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= M_RUS(" begin", " HA4AT|");
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= 24;
	s_startserver_start_action.generic.y	= 165;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_cashlimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );
}

void StartServer_MenuDraw(void)
{
	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key )
{
	if ( key == K_ESCAPE )
		StartServer_FreeMaps ();

	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit();
	M_PushMenu( StartServer_MenuDraw, StartServer_MenuKey );
}
#else
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc( void *self )
{
	if (s_rules_box.curvalue == 1)
		return;
	M_Menu_DMOptions_f();
}

void RulesChangeFunc ( void *self )
{
	// DM
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if(s_rules_box.curvalue == 1)		// coop				// PGM
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy( s_maxclients_field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
//=====
//PGM
	// ROGUE GAMES
	else if(Developer_searchpath(2) == 2)
	{
		if (s_rules_box.curvalue == 2)			// tag
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
/*
		else if(s_rules_box.curvalue == 3)		// deathball
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
*/
	}
//PGM
//=====
}

void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		maxclients;
	char	*spot;

	strcpy( startmap, strchr( mapnames[s_startmap_list.curvalue], '\n' ) + 1 );

	maxclients  = atoi( s_maxclients_field.buffer );
	timelimit	= atoi( s_timelimit_field.buffer );
	fraglimit	= atoi( s_fraglimit_field.buffer );

	Cvar_SetValue( "maxclients", ClampCvar( 0, maxclients, maxclients ) );
	Cvar_SetValue ("timelimit", ClampCvar( 0, timelimit, timelimit ) );
	Cvar_SetValue ("fraglimit", ClampCvar( 0, fraglimit, fraglimit ) );
	Cvar_Set("hostname", s_hostname_field.buffer );
//	Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
//	Cvar_SetValue ("coop", s_rules_box.curvalue );

//PGM
	if((s_rules_box.curvalue < 2) || (Developer_searchpath(2) != 2))
	{
		Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
		Cvar_SetValue ("coop", s_rules_box.curvalue );
		Cvar_SetValue ("gamerules", 0 );
	}
	else
	{
		Cvar_SetValue ("deathmatch", 1 );	// deathmatch is always true for rogue games, right?
		Cvar_SetValue ("coop", 0 );			// FIXME - this might need to depend on which game we're running
		Cvar_SetValue ("gamerules", s_rules_box.curvalue );
	}
//PGM

	spot = NULL;
	if (s_rules_box.curvalue == 1)		// PGM
	{
 		if(Q_stricmp(startmap, "bunk1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "mintro") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "fact1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "power1") == 0)
  			spot = "pstart";
 		else if(Q_stricmp(startmap, "biggun") == 0)
  			spot = "bstart";
 		else if(Q_stricmp(startmap, "hangar1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "city1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

void StartServer_MenuInit( void )
{
	char	shortname[MAX_TOKEN_CHARS];
	char	longname[MAX_TOKEN_CHARS];
	char	scratch[200];
	char	*buffer, *parseptr;
	int		length;
	int		i, j, l;

	static const char *dm_coop_names[] =
	{
		M_RUS("deathmatch", "VOZ HACMEPT|"),
		"cooperative",
		0
	};
//=======
//PGM
	static const char *dm_coop_names_rogue[] =
	{
		M_RUS("deathmatch", "VOZ HACMEPT|"),
		"cooperative",
		"tag",
//		"deathball",
		0
	};
//PGM
//=======

	/*
	** load the list of map names
	*/
	if (!(buffer = FS_LoadFile("maps.lst", &length, FS_PATH_GAMEDIR | FS_TYPE_RFS)))
	{
		if (!(buffer = FS_LoadFile("maps.lst", &length, FS_PATH_ALL)))
			Com_Error( ERR_DROP, "couldn't find maps.lst\n" );
	}

	parseptr = buffer;

	for(i = 0; i < length; i++)
	{
		if ( parseptr[i] == '\n' )
			nummaps++;
	}

	if ( nummaps == 0 )
		Com_Error( ERR_DROP, "no maps in maps.lst\n" );

	mapnames = malloc( sizeof( char * ) * ( nummaps + 1 ) );
	memset( mapnames, 0, sizeof( char * ) * ( nummaps + 1 ) );

	for ( i = 0; i < nummaps; i++ )
	{
		strcpy( shortname, COM_Parse( &parseptr ) );
		l = strlen(shortname);
		for (j=0 ; j<l ; j++)
			shortname[j] = toupper(shortname[j]);
		strcpy( longname, COM_Parse( &parseptr ) );
		Com_sprintf( scratch, sizeof( scratch ), "%s\n%s", longname, shortname );

		mapnames[i] = malloc( strlen( scratch ) + 1 );
		strcpy( mapnames[i], scratch );
	}
	mapnames[nummaps] = 0;

	FS_FreeFile( buffer );

	/*
	** initialize the menu stuff
	*/
	s_startserver_menu.x = viddef.width * 0.50;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= 0;
	s_startmap_list.generic.name	= M_RUS("initial map", "HA4AL|HAR KAPTA");
	s_startmap_list.itemnames = (const char **)mapnames;

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	= 0;
	s_rules_box.generic.y	= 20;
	s_rules_box.generic.name	= M_RUS("rules", "IPABULA");

//PGM - rogue games only available with rogue DLL.
	if(Developer_searchpath(2) == 2)
		s_rules_box.itemnames = dm_coop_names_rogue;
	else
		s_rules_box.itemnames = dm_coop_names;
//PGM

	if (Cvar_VariableValue("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = M_RUS("time limit", "LUMUT BPEMEHU");
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= 36;
	s_timelimit_field.generic.statusbar = M_RUS("0 = no limit", "0 = VE3 OGPAHU4EHUZ");
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy( s_timelimit_field.buffer, Cvar_VariableString("timelimit") );

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = M_RUS("frag limit", "LUMUT YVUZCTB");
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= 54;
	s_fraglimit_field.generic.statusbar = M_RUS("0 = no limit", "0 = VE3 OGPAHU4EHUZ");
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy( s_fraglimit_field.buffer, Cvar_VariableString("fraglimit") );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is.
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = M_RUS("max players", "MAKC. UGPOKOB");
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= 0;
	s_maxclients_field.generic.y	= 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if ( Cvar_VariableValue( "maxclients" ) == 1 )
		strcpy( s_maxclients_field.buffer, "8" );
	else
		strcpy( s_maxclients_field.buffer, Cvar_VariableString("maxclients") );

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = M_RUS("hostname", "UMR CEPBEPA");
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= 0;
	s_hostname_field.generic.y	= 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy( s_hostname_field.buffer, Cvar_VariableString("hostname") );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= M_RUS(" deathmatch flags", " IPABULA VOR");
	s_startserver_dmoptions_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	= 24;
	s_startserver_dmoptions_action.generic.y	= 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= M_RUS(" begin", " HA4AT|");
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= 24;
	s_startserver_start_action.generic.y	= 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );
}

void StartServer_MenuDraw(void)
{
	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key )
{
	if ( key == K_ESCAPE || key == K_START_BUTTON || key == K_B_BUTTON )
	{
		if ( mapnames )
		{
			int i;

			for ( i = 0; i < nummaps; i++ )
				free( mapnames[i] );
			free( mapnames );
		}
		mapnames = 0;
		nummaps = 0;
	}

	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit();
	M_PushMenu( StartServer_MenuDraw, StartServer_MenuKey );
}
#endif

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/
#ifdef KINGPIN_ENGINE
/*
kingpin.exe @0042c380: thirteen yes/no switches. Bit 2 (DF_NO_ITEMS in
Quake II) is "auto assign teams" in Kingpin, and the teamplay switch only
toggles DF_MODELTEAMS.
*/
static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_health_box;
static menulist_s	s_armor_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_friendlyfire_box;
static menulist_s	s_autoteams_box;

// kingpin.exe @0042c790
static void DMFlagCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue( "dmflags" );

	if ( f == &s_friendlyfire_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if ( f == &s_falls_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if ( f == &s_weapons_stay_box )
	{
		bit = DF_WEAPONS_STAY;
	}
	else if ( f == &s_allow_exit_box )
	{
		bit = DF_ALLOW_EXIT;
	}
	else if ( f == &s_health_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if ( f == &s_spawn_farthest_box )
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if ( f == &s_teamplay_box )
	{
		if ( f->curvalue )
			flags |= DF_MODELTEAMS;
		else
			flags &= ~DF_MODELTEAMS;
		goto setvalue;
	}
	else if ( f == &s_samelevel_box )
	{
		bit = DF_SAME_LEVEL;
	}
	else if ( f == &s_force_respawn_box )
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if ( f == &s_armor_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if ( f == &s_infinite_ammo_box )
	{
		bit = DF_INFINITE_AMMO;
	}
	else if ( f == &s_fixed_fov_box )
	{
		bit = DF_FIXED_FOV;
	}
	else if ( f == &s_autoteams_box )
	{
		bit = DF_AUTO_JOIN_TEAM;		// auto assign teams in Kingpin
	}

	if ( f )
	{
		if ( f->curvalue == 0 )
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", flags);

	Com_sprintf( dmoptions_statusbar, sizeof( dmoptions_statusbar ), M_RUS("dmflags = %d", "IPABULA = %d"), flags );
}

static void DMOptions_InitBox( menulist_s *box, int y, char *name, int curvalue )
{
	static const char *yes_no_names[] =
	{
		M_RUS("no", "HET"), M_RUS("yes", "DA"), 0
	};

	box->generic.type = MTYPE_SPINCONTROL;
	box->generic.x	= 0;
	box->generic.y	= y;
	box->generic.name	= name;
	box->generic.callback = DMFlagCallback;
	box->itemnames = yes_no_names;
	box->curvalue = curvalue;
	Menu_AddItem( &s_dmoptions_menu, box );
}

void DMOptions_MenuInit( void )
{
	int dmflags = Cvar_VariableValue( "dmflags" );

	s_dmoptions_menu.x = viddef.width * 0.50;
	s_dmoptions_menu.nitems = 0;

	DMOptions_InitBox( &s_falls_box, 0, M_RUS("falling damage", "YPOH OT IADEHUZ"), ( dmflags & DF_NO_FALLING ) == 0 );
	DMOptions_InitBox( &s_weapons_stay_box, 10, M_RUS("weapons stay", "OPYJUE OCTAETCR"), ( dmflags & DF_WEAPONS_STAY ) != 0 );
	DMOptions_InitBox( &s_health_box, 20, M_RUS("allow health", "AITE4KU"), ( dmflags & DF_NO_HEALTH ) == 0 );
	DMOptions_InitBox( &s_armor_box, 30, M_RUS("allow armor", "VPOHR"), ( dmflags & DF_NO_ARMOR ) == 0 );
	DMOptions_InitBox( &s_spawn_farthest_box, 40, M_RUS("spawn farthest", "BO3POJDAT| BDALU"), ( dmflags & DF_SPAWN_FARTHEST ) != 0 );
	DMOptions_InitBox( &s_samelevel_box, 50, M_RUS("same map", "TA JE KAPTA"), ( dmflags & DF_SAME_LEVEL ) != 0 );
	// sic: kingpin.exe reads the switch from DF_MODELTEAMS, it sets DF_FORCE_RESPAWN
	DMOptions_InitBox( &s_force_respawn_box, 60, M_RUS("force respawn", "ABTOBO3POJDEHUE"), ( dmflags & DF_MODELTEAMS ) != 0 );
	// sic: kingpin.exe leaves this switch at its last value
	DMOptions_InitBox( &s_teamplay_box, 70, M_RUS("teamplay by model & skin", "KOMAHD} IO OVLUKY"), s_teamplay_box.curvalue );
	DMOptions_InitBox( &s_allow_exit_box, 80, M_RUS("allow exit", "PA3PEWUT| B}XOD"), ( dmflags & DF_ALLOW_EXIT ) != 0 );
	DMOptions_InitBox( &s_infinite_ammo_box, 90, M_RUS("infinite ammo", "VECKOH. IATPOH}"), ( dmflags & DF_INFINITE_AMMO ) != 0 );
	DMOptions_InitBox( &s_fixed_fov_box, 100, M_RUS("fixed FOV", "FUKC. YGOL OV3OPA"), ( dmflags & DF_FIXED_FOV ) != 0 );
	DMOptions_InitBox( &s_friendlyfire_box, 110, M_RUS("Friendly Damage", "YPOH CBOUM"), ( dmflags & DF_NO_FRIENDLY_FIRE ) == 0 );
	DMOptions_InitBox( &s_autoteams_box, 120, M_RUS("Auto Assign Teams", "ABTOB}VOP KOMAHD}"), ( dmflags & DF_AUTO_JOIN_TEAM ) != 0 );

	Menu_Center( &s_dmoptions_menu );

	// set the original dmflags statusbar
	DMFlagCallback( 0 );
	Menu_SetStatusBar( &s_dmoptions_menu, dmoptions_statusbar );
}
#else
static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

//ROGUE
static menulist_s	s_no_mines_box;
static menulist_s	s_no_nukes_box;
static menulist_s	s_stack_double_box;
static menulist_s	s_no_spheres_box;
//ROGUE

static void DMFlagCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue( "dmflags" );

	if ( f == &s_friendlyfire_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if ( f == &s_falls_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if ( f == &s_weapons_stay_box )
	{
		bit = DF_WEAPONS_STAY;
	}
	else if ( f == &s_instant_powerups_box )
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if ( f == &s_allow_exit_box )
	{
		bit = DF_ALLOW_EXIT;
	}
	else if ( f == &s_powerups_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if ( f == &s_health_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if ( f == &s_spawn_farthest_box )
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if ( f == &s_teamplay_box )
	{
		if ( f->curvalue == 1 )
		{
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if ( f->curvalue == 2 )
		{
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~( DF_MODELTEAMS | DF_SKINTEAMS );
		}

		goto setvalue;
	}
	else if ( f == &s_samelevel_box )
	{
		bit = DF_SAME_LEVEL;
	}
	else if ( f == &s_force_respawn_box )
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if ( f == &s_armor_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if ( f == &s_infinite_ammo_box )
	{
		bit = DF_INFINITE_AMMO;
	}
	else if ( f == &s_fixed_fov_box )
	{
		bit = DF_FIXED_FOV;
	}
	else if ( f == &s_quad_drop_box )
	{
		bit = DF_QUAD_DROP;
	}

//=======
//ROGUE
	else if (Developer_searchpath(2) == 2)
	{
		if ( f == &s_no_mines_box)
		{
			bit = DF_NO_MINES;
		}
		else if ( f == &s_no_nukes_box)
		{
			bit = DF_NO_NUKES;
		}
		else if ( f == &s_stack_double_box)
		{
			bit = DF_NO_STACK_DOUBLE;
		}
		else if ( f == &s_no_spheres_box)
		{
			bit = DF_NO_SPHERES;
		}
	}
//ROGUE
//=======

	if ( f )
	{
		if ( f->curvalue == 0 )
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", flags);

	Com_sprintf( dmoptions_statusbar, sizeof( dmoptions_statusbar ), M_RUS("dmflags = %d", "IPABULA = %d"), flags );

}

void DMOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		M_RUS("no", "HET"), M_RUS("yes", "DA"), 0
	};
	static const char *teamplay_names[] =
	{
		M_RUS("disabled", "B}KL@4EHO"), "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue( "dmflags" );
	int y = 0;

	s_dmoptions_menu.x = viddef.width * 0.50;
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x	= 0;
	s_falls_box.generic.y	= y;
	s_falls_box.generic.name	= M_RUS("falling damage", "YPOH OT IADEHUZ");
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = ( dmflags & DF_NO_FALLING ) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x	= 0;
	s_weapons_stay_box.generic.y	= y += 10;
	s_weapons_stay_box.generic.name	= M_RUS("weapons stay", "OPYJUE OCTAETCR");
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = ( dmflags & DF_WEAPONS_STAY ) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x	= 0;
	s_instant_powerups_box.generic.y	= y += 10;
	s_instant_powerups_box.generic.name	= "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = ( dmflags & DF_INSTANT_ITEMS ) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x	= 0;
	s_powerups_box.generic.y	= y += 10;
	s_powerups_box.generic.name	= "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = ( dmflags & DF_NO_ITEMS ) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x	= 0;
	s_health_box.generic.y	= y += 10;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name	= M_RUS("allow health", "AITE4KU");
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = ( dmflags & DF_NO_HEALTH ) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x	= 0;
	s_armor_box.generic.y	= y += 10;
	s_armor_box.generic.name	= M_RUS("allow armor", "VPOHR");
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = ( dmflags & DF_NO_ARMOR ) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x	= 0;
	s_spawn_farthest_box.generic.y	= y += 10;
	s_spawn_farthest_box.generic.name	= M_RUS("spawn farthest", "BO3POJDAT| BDALU");
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = ( dmflags & DF_SPAWN_FARTHEST ) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x	= 0;
	s_samelevel_box.generic.y	= y += 10;
	s_samelevel_box.generic.name	= M_RUS("same map", "TA JE KAPTA");
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = ( dmflags & DF_SAME_LEVEL ) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x	= 0;
	s_force_respawn_box.generic.y	= y += 10;
	s_force_respawn_box.generic.name	= M_RUS("force respawn", "ABTOBO3POJDEHUE");
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = ( dmflags & DF_FORCE_RESPAWN ) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x	= 0;
	s_teamplay_box.generic.y	= y += 10;
	s_teamplay_box.generic.name	= M_RUS("teamplay", "KOMAHDHAR UGPA");
	s_teamplay_box.generic.callback = DMFlagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x	= 0;
	s_allow_exit_box.generic.y	= y += 10;
	s_allow_exit_box.generic.name	= M_RUS("allow exit", "PA3PEWUT| B}XOD");
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = ( dmflags & DF_ALLOW_EXIT ) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x	= 0;
	s_infinite_ammo_box.generic.y	= y += 10;
	s_infinite_ammo_box.generic.name	= M_RUS("infinite ammo", "VECKOH. IATPOH}");
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = ( dmflags & DF_INFINITE_AMMO ) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x	= 0;
	s_fixed_fov_box.generic.y	= y += 10;
	s_fixed_fov_box.generic.name	= M_RUS("fixed FOV", "FUKC. YGOL OV3OPA");
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = ( dmflags & DF_FIXED_FOV ) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x	= 0;
	s_quad_drop_box.generic.y	= y += 10;
	s_quad_drop_box.generic.name	= "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = ( dmflags & DF_QUAD_DROP ) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x	= 0;
	s_friendlyfire_box.generic.y	= y += 10;
	s_friendlyfire_box.generic.name	= "friendly fire";
	s_friendlyfire_box.generic.callback = DMFlagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = ( dmflags & DF_NO_FRIENDLY_FIRE ) == 0;

//============
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x	= 0;
		s_no_mines_box.generic.y	= y += 10;
		s_no_mines_box.generic.name	= "remove mines";
		s_no_mines_box.generic.callback = DMFlagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = ( dmflags & DF_NO_MINES ) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x	= 0;
		s_no_nukes_box.generic.y	= y += 10;
		s_no_nukes_box.generic.name	= "remove nukes";
		s_no_nukes_box.generic.callback = DMFlagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = ( dmflags & DF_NO_NUKES ) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x	= 0;
		s_stack_double_box.generic.y	= y += 10;
		s_stack_double_box.generic.name	= "2x/4x stacking off";
		s_stack_double_box.generic.callback = DMFlagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = ( dmflags & DF_NO_STACK_DOUBLE ) != 0;

		s_no_spheres_box.generic.type = MTYPE_SPINCONTROL;
		s_no_spheres_box.generic.x	= 0;
		s_no_spheres_box.generic.y	= y += 10;
		s_no_spheres_box.generic.name	= "remove spheres";
		s_no_spheres_box.generic.callback = DMFlagCallback;
		s_no_spheres_box.itemnames = yes_no_names;
		s_no_spheres_box.curvalue = ( dmflags & DF_NO_SPHERES ) != 0;

	}
//ROGUE
//============

	Menu_AddItem( &s_dmoptions_menu, &s_falls_box );
	Menu_AddItem( &s_dmoptions_menu, &s_weapons_stay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_instant_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_health_box );
	Menu_AddItem( &s_dmoptions_menu, &s_armor_box );
	Menu_AddItem( &s_dmoptions_menu, &s_spawn_farthest_box );
	Menu_AddItem( &s_dmoptions_menu, &s_samelevel_box );
	Menu_AddItem( &s_dmoptions_menu, &s_force_respawn_box );
	Menu_AddItem( &s_dmoptions_menu, &s_teamplay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_allow_exit_box );
	Menu_AddItem( &s_dmoptions_menu, &s_infinite_ammo_box );
	Menu_AddItem( &s_dmoptions_menu, &s_fixed_fov_box );
	Menu_AddItem( &s_dmoptions_menu, &s_quad_drop_box );
	Menu_AddItem( &s_dmoptions_menu, &s_friendlyfire_box );

//=======
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		Menu_AddItem( &s_dmoptions_menu, &s_no_mines_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_nukes_box );
		Menu_AddItem( &s_dmoptions_menu, &s_stack_double_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_spheres_box );
	}
//ROGUE
//=======

	Menu_Center( &s_dmoptions_menu );

	// set the original dmflags statusbar
	DMFlagCallback( 0 );
	Menu_SetStatusBar( &s_dmoptions_menu, dmoptions_statusbar );
}
#endif

void DMOptions_MenuDraw(void)
{
	Menu_Draw( &s_dmoptions_menu );
}

const char *DMOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_dmoptions_menu, key );
}

void M_Menu_DMOptions_f (void)
{
	DMOptions_MenuInit();
	M_PushMenu( DMOptions_MenuDraw, DMOptions_MenuKey );
}

/*
=============================================================================

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/
static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static void DownloadCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", f->curvalue);
	}

	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", f->curvalue);
	}

	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", f->curvalue);
	}

	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", f->curvalue);
	}

	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", f->curvalue);
	}
}

void DownloadOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		M_RUS("no", "HET"), M_RUS("yes", "DA"), 0
	};
	int y = 0;

	s_downloadoptions_menu.x = viddef.width * 0.50;
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = M_RUS("Download Options", "3AGPY3KA FAZLOB");
	s_download_title.generic.x    = 48;
	s_download_title.generic.y	 = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x	= 0;
	s_allow_download_box.generic.y	= y += 20;
	s_allow_download_box.generic.name	= M_RUS("allow downloading", "PA3PEWUT| 3AGPY3KY");
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x	= 0;
	s_allow_download_maps_box.generic.y	= y += 20;
	s_allow_download_maps_box.generic.name	= M_RUS("maps", "KAPT}");
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x	= 0;
	s_allow_download_players_box.generic.y	= y += 10;
	s_allow_download_players_box.generic.name	= M_RUS("player models/skins", "OVLUK UGPOKOB");
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x	= 0;
	s_allow_download_models_box.generic.y	= y += 10;
	s_allow_download_models_box.generic.name	= M_RUS("models", "MODELU");
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x	= 0;
	s_allow_download_sounds_box.generic.y	= y += 10;
	s_allow_download_sounds_box.generic.name	= M_RUS("sounds", "3BYKU");
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem( &s_downloadoptions_menu, &s_download_title );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_maps_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_players_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_models_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_sounds_box );

	Menu_Center( &s_downloadoptions_menu );

	// skip over title
	if (s_downloadoptions_menu.cursor == 0)
		s_downloadoptions_menu.cursor = 1;
}

void DownloadOptions_MenuDraw(void)
{
	Menu_Draw( &s_downloadoptions_menu );
}

const char *DownloadOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_downloadoptions_menu, key );
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit();
	M_PushMenu( DownloadOptions_MenuDraw, DownloadOptions_MenuKey );
}
/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/
#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

void AddressBook_MenuInit( void )
{
	int i;

	s_addressbook_menu.x = viddef.width / 2 - 142;
	s_addressbook_menu.y = viddef.height / 2 - 58;
	s_addressbook_menu.nitems = 0;

	for ( i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++ )
	{
		cvar_t *adr;
		char buffer[20];

		Com_sprintf( buffer, sizeof( buffer ), "adr%d", i );

		adr = Cvar_Get( buffer, "", CVAR_ARCHIVE );

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x		= 0;
		s_addressbook_fields[i].generic.y		= i * 18 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor			= 0;
		s_addressbook_fields[i].length			= 60;
		s_addressbook_fields[i].visible_length	= 30;

		strcpy( s_addressbook_fields[i].buffer, adr->string );

		Menu_AddItem( &s_addressbook_menu, &s_addressbook_fields[i] );
	}
}

const char *AddressBook_MenuKey( int key )
{
	if ( key == K_ESCAPE || key == K_START_BUTTON || key == K_B_BUTTON )
	{
		int index;
		char buffer[20];

		for ( index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++ )
		{
			Com_sprintf( buffer, sizeof( buffer ), "adr%d", index );
			Cvar_Set( buffer, s_addressbook_fields[index].buffer );
		}
	}
	return Default_MenuKey( &s_addressbook_menu, key );
}

void AddressBook_MenuDraw(void)
{
	M_Banner( "m_banner_addressbook" );
	Menu_Draw( &s_addressbook_menu );
}

void M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	M_PushMenu( AddressBook_MenuDraw, AddressBook_MenuKey );
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
#ifdef KINGPIN_ENGINE
/*
kingpin.exe @0042cb10..@0042f740. A Kingpin player is a directory under
players/ with head, body and legs .mdx parts, each with its own list of
skins (head_*.tga, body_*.tga, legs_*.tga). skinfolder.txt can point each
part's skins at another directory ("body:models/actors/thug"), extras.txt
lists up to four groups of optional extra models ("1 Hats" "+cap" ...).
The skin cvar is "model/HHH BBB LLL", the extras cvar one digit per group.
*/
char **FS_ListFiles( char *, int *, unsigned, unsigned );
extern cvar_t *skin;

static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menuseparator_s	s_player_model_title;
static menulist_s		s_player_model_box;
static menuseparator_s	s_player_head_title;
static menulist_s		s_player_head_box;
static menuseparator_s	s_player_body_title;
static menulist_s		s_player_body_box;
static menuseparator_s	s_player_legs_title;
static menulist_s		s_player_legs_box;
static menuseparator_s	s_player_extras_title[4];
static menulist_s		s_player_extras_box[4];
static menuseparator_s	s_player_rate_title;
static menulist_s		s_player_rate_box;

#define MAX_DISPLAYNAME		16
#define MAX_PLAYERMODELS	128
#define MAX_EXTRAS			4
#define MAX_EXTRA_MODELS	8

typedef struct
{
	int		nbodyskins, nlegsskins, nheadskins;
	char	**bodyskins, **legsskins, **headskins;	// "body_001", sorted
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
	char	bodydir[MAX_QPATH], headdir[MAX_QPATH], legsdir[MAX_QPATH];	// where the skins are
	char	extras[MAX_EXTRAS][MAX_EXTRA_MODELS][16];	// model names; element zero is the localized none/N/A label
	char	extranames[MAX_EXTRAS][16];					// group names
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

// the selection, kept between visits like kingpin.exe
static int s_pm_model;
static int s_pm_head = -1, s_pm_body = -1, s_pm_legs = -1;
static int s_pm_extras[MAX_EXTRAS];
static int s_pm_skinmemory[MAX_PLAYERMODELS][3];	// legs, head, body per model
static char *s_pm_extranames[MAX_EXTRAS][MAX_EXTRA_MODELS + 1];

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { M_RUS("28.8 Modem", "MODEM 28.8"), M_RUS("33.6 Modem", "MODEM 33.6"), M_RUS("Single ISDN", "ODUH isdn"),
	M_RUS("Dual ISDN/Cable", "DBA isdn/KAVEL|"), "T1/LAN", M_RUS("User defined", "CBOR"), 0 };

// kingpin.exe @0042caf0, from CL_InitLocal
void M_ClearPlayerSkinMemory (void)
{
	memset (s_pm_skinmemory, 0, sizeof(s_pm_skinmemory));
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

// kingpin.exe @0042e440: the extras choices of the current model
static void PlayerConfig_UpdateExtras( void )
{
	playermodelinfo_s *pmi = &s_pmi[s_player_model_box.curvalue];
	int i, j;

	for (i = 0; i < MAX_EXTRAS; i++)
	{
		memset (s_pm_extranames[i], 0, sizeof(s_pm_extranames[i]));
		for (j = 0; j < MAX_EXTRA_MODELS && pmi->extras[i][j][0]; j++)
			s_pm_extranames[i][j] = pmi->extras[i][j];
		s_player_extras_box[i].itemnames = (const char **)s_pm_extranames[i];
		if (!pmi->extras[i][s_player_extras_box[i].curvalue][0])
			s_player_extras_box[i].curvalue = 0;
		s_player_extras_title[i].generic.name = pmi->extranames[i];
	}
}

// kingpin.exe @0042e510: each model remembers its skin choice
static void ModelCallback( void *unused )
{
	int m = s_player_model_box.curvalue;

	s_player_body_box.curvalue = s_pm_skinmemory[m][2];
	s_player_body_box.itemnames = (const char **)s_pmi[m].bodyskins;
	s_player_legs_box.curvalue = s_pm_skinmemory[m][0];
	s_player_head_box.curvalue = s_pm_skinmemory[m][1];
	s_player_legs_box.itemnames = (const char **)s_pmi[m].legsskins;
	s_player_head_box.itemnames = (const char **)s_pmi[m].headskins;
	PlayerConfig_UpdateExtras ();
}

static void FreeFileList( char **list, int n )
{
	int i;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] )
		{
			free( list[i] );
			list[i] = 0;
		}
	}
	free( list );
}

/*
list the skins matching pattern ("body_*.tga") in the part's skin
directory, without the p1/p2 pain skins, as sorted names without extension
*/
static char **PlayerConfig_ListSkins( char *dir, char *folder, char *pattern, int *count )
{
	char	path[1024], name[1024];
	char	**files, **skins, *a, *b, *s;
	int		nfiles, i, j, n;

	strcpy (path, dir);
	if (folder[0])
		strcpy (strstr (path, "players/"), folder);
	strcat (path, pattern);

	files = FS_ListFiles (path, &nfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
	if (!files)
		return NULL;

	skins = malloc (sizeof(char *) * (nfiles + 1));
	memset (skins, 0, sizeof(char *) * (nfiles + 1));
	n = 0;
	for (i = 0; i < nfiles - 1; i++)
	{
		if (strstr (files[i], "p1.tga") || strstr (files[i], "p2.tga"))
			continue;

		a = strrchr (files[i], '/');
		b = strrchr (files[i], '\\');
		if (b < a)
			b = a;
		strcpy (name, b + 1);
		if (strrchr (name, '.'))
			*strrchr (name, '.') = 0;
		s = strdup (name);

		// insertion sort
		for (j = 0; j < n; j++)
			if (strcmp (skins[j], s) > 0)
				break;
		memmove (&skins[j + 1], &skins[j], sizeof(char *) * (n - j));
		skins[j] = s;
		n++;
	}

	FreeFileList (files, nfiles);
	*count = n;
	return skins;
}

// kingpin.exe @0042cee0
static qboolean PlayerConfig_ScanDirectories( void )
{
	char	findname[1024];
	char	scratch[1024];
	char	folders[3][64];
	char	*bodyfolder, *headfolder, *legsfolder;
	char	*path = NULL, *s, *a, *b;
	char	**dirnames;
	int		ndirs = 0, i, j, group;
	char	c;
	FILE	*f;
	playermodelinfo_s *pmi;

	s_numplayermodels = 0;

	/*
	** get a list of directories
	*/
	do
	{
		path = FS_NextPath( path );
		Com_sprintf( findname, sizeof(findname), "%s/players/*.*", path );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, SFF_SUBDIR, 0 ) ) != 0 )
			break;
	} while ( path );

	if ( !dirnames )
		return false;

	if ( ndirs > MAX_PLAYERMODELS )
		ndirs = MAX_PLAYERMODELS;

	for ( i = 0; i < ndirs; i++ )
	{
		if ( dirnames[i] == 0 )
			continue;

		// where the skins of each part are
		memset (folders, 0, sizeof(folders));
		bodyfolder = headfolder = legsfolder = folders[0];
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/skinfolder.txt" );
		if ( ( f = fopen( scratch, "r" ) ) != 0 )
		{
			fscanf( f, "%s\n%s\n%s", folders[0], folders[1], folders[2] );
			fclose( f );
			if ( folders[0][0] )
			{
				if ( !strstr( folders[0], "body:" ) && !strstr( folders[0], "legs:" ) && !strstr( folders[0], "head:" ) )
				{	// one folder for all parts
					bodyfolder = headfolder = legsfolder = folders[0];
				}
				else
				{
					for ( j = 0; j < 3; j++ )
					{
						if ( strstr( folders[j], "body:" ) )
							bodyfolder = folders[j] + 5;
						else if ( strstr( folders[j], "head:" ) )
							headfolder = folders[j] + 5;
						else if ( strstr( folders[j], "legs:" ) )
							legsfolder = folders[j] + 5;
					}
				}
			}
		}

		// the extras groups
		pmi = &s_pmi[s_numplayermodels];
		memset( pmi->extras, 0, sizeof(pmi->extras) );
		for ( j = 0; j < MAX_EXTRAS; j++ )
		{
			strcpy( pmi->extranames[j], M_RUS("(unused)", "(HE UCI.)") );
			strcpy( pmi->extras[j][0], M_RUS("N/A", "HET") );
		}
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/extras.txt" );
		if ( ( f = fopen( scratch, "r" ) ) != 0 )
		{
			char unused[16];

			group = 0;
			while ( !feof( f ) )
			{
				fscanf( f, "%c", &c );
				if ( c >= '1' && c <= '9' )
				{
					group = c - '1';
					fscanf( f, " %s\n", pmi->extranames[group] );
					strcpy( pmi->extras[group][0], M_RUS("none", "HET") );
				}
				else if ( c == '+' )
				{
					for ( j = 0; j < MAX_EXTRA_MODELS; j++ )
						if ( !pmi->extras[group][j][0] )
							break;
					fscanf( f, "%s\n", j == MAX_EXTRA_MODELS ? unused : pmi->extras[group][j] );
				}
			}
			fclose( f );
		}

		// a player model needs a body
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/body.mdx" );
		if ( !Sys_FindFirst( scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			Sys_FindClose();
			continue;
		}
		Sys_FindClose();

		pmi->bodyskins = PlayerConfig_ListSkins( dirnames[i], bodyfolder, "/body_*.tga", &pmi->nbodyskins );
		if ( pmi->bodyskins )
			pmi->headskins = PlayerConfig_ListSkins( dirnames[i], headfolder, "/head_*.tga", &pmi->nheadskins );
		if ( pmi->bodyskins && pmi->headskins )
			pmi->legsskins = PlayerConfig_ListSkins( dirnames[i], legsfolder, "/legs_*.tga", &pmi->nlegsskins );
		if ( !pmi->bodyskins || !pmi->headskins || !pmi->legsskins )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			continue;
		}

		a = strrchr( dirnames[i], '/' );
		b = strrchr( dirnames[i], '\\' );
		if ( b < a )
			b = a;
		strncpy( pmi->displayname, b + 1, MAX_DISPLAYNAME - 1 );
		strcpy( pmi->directory, b + 1 );

		if ( !bodyfolder[0] )
		{
			s = strstr( dirnames[i], "players" );
			strcpy( pmi->bodydir, s );
			strcpy( pmi->headdir, s );
			strcpy( pmi->legsdir, s );
		}
		else
		{
			strcpy( pmi->bodydir, bodyfolder );
			strcpy( pmi->headdir, headfolder );
			strcpy( pmi->legsdir, legsfolder );
		}

		s_numplayermodels++;
	}

	if ( dirnames )
		FreeFileList( dirnames, ndirs );

	return true;
}

// kingpin.exe @0042de40: "male" and "female" first, then by name
static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
}

static int PlayerConfig_FindSkin( char **skins, int n, char *number, int old )
{
	int i;

	for ( i = 0; i < n; i++ )
		if ( !Q_stricmp( skins[i] + 5, number ) )
			return i;
	return old;
}

// kingpin.exe @0042cb10: scan the models and find the current choice
static qboolean PlayerConfig_LoadModels( void )
{
	char currentdirectory[1024];
	char currentskin[1024];
	char *extras;
	int i;

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	Cvar_SetValue( "hand", 0 );

	strcpy( currentdirectory, skin->string );

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
		strcpy( currentdirectory, "male_thug" );
		strcpy( currentskin, "001 001 001" );
	}

	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			s_pm_model = i;
			if ( strlen( currentskin ) < 11 )
				break;

			// "HHH BBB LLL"
			currentskin[3] = currentskin[7] = 0;
			s_pm_head = PlayerConfig_FindSkin( s_pmi[i].headskins, s_pmi[i].nheadskins, currentskin, s_pm_head );
			s_pm_body = PlayerConfig_FindSkin( s_pmi[i].bodyskins, s_pmi[i].nbodyskins, currentskin + 4, s_pm_body );
			s_pm_legs = PlayerConfig_FindSkin( s_pmi[i].legsskins, s_pmi[i].nlegsskins, currentskin + 8, s_pm_legs );
			currentskin[3] = currentskin[7] = ' ';
		}
	}
	if ( s_pm_head < 0 )
		s_pm_head = 0;
	if ( s_pm_body < 0 )
		s_pm_body = 0;
	if ( s_pm_legs < 0 )
		s_pm_legs = 0;

	memset( s_pm_extras, 0, sizeof( s_pm_extras ) );
	extras = Cvar_VariableString( "extras" );
	for ( i = 0; i < MAX_EXTRAS && extras[i]; i++ )
		s_pm_extras[i] = extras[i] - '0';

	return true;
}

static void PlayerConfig_InitSpin( menuseparator_s *title, char *name, menulist_s *box, int y )
{
	title->generic.type = MTYPE_SEPARATOR;
	title->generic.name = name;
	title->generic.x = -16;
	title->generic.y = y - 10;

	box->generic.type = MTYPE_SPINCONTROL;
	box->generic.name = 0;
	box->generic.x = -86;
	box->generic.y = y;
	box->generic.callback = 0;
	box->generic.cursor_offset = -8;
}

// kingpin.exe @0042df90
qboolean PlayerConfig_MenuInit( void )
{
	extern cvar_t *name;
	int i;

	if ( !PlayerConfig_LoadModels() )
		return false;

	s_player_config_menu.x = viddef.width / 2;
	s_player_config_menu.y = viddef.height / 2;
	s_player_config_menu.nitems = 0;
	s_player_config_menu.xoffset = -70;
	s_player_config_menu.yoffset = -20;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = M_RUS("name", "UMR");
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy( s_player_name_field.buffer, name->string );
	s_player_name_field.cursor = strlen( name->string );

	PlayerConfig_InitSpin( &s_player_model_title, M_RUS("model", "MODEL|"), &s_player_model_box, 70 );
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.curvalue = s_pm_model;
	s_player_model_box.itemnames = (const char **)s_pmnames;

	PlayerConfig_InitSpin( &s_player_head_title, M_RUS("head skin", "LUQO"), &s_player_head_box, 100 );
	s_player_head_box.curvalue = s_pm_head;
	s_player_head_box.itemnames = (const char **)s_pmi[s_pm_model].headskins;

	PlayerConfig_InitSpin( &s_player_body_title, M_RUS("body skin", "ODEJDA"), &s_player_body_box, 120 );
	s_player_body_box.curvalue = s_pm_body;
	s_player_body_box.itemnames = (const char **)s_pmi[s_pm_model].bodyskins;

	PlayerConfig_InitSpin( &s_player_legs_title, M_RUS("legs skin", "VP@KU"), &s_player_legs_box, 140 );
	s_player_legs_box.curvalue = s_pm_legs;
	s_player_legs_box.itemnames = (const char **)s_pmi[s_pm_model].legsskins;

	for ( i = 0; i < MAX_EXTRAS; i++ )
	{
		PlayerConfig_InitSpin( &s_player_extras_title[i], M_RUS("extras", "DOIOLHEHUR"), &s_player_extras_box[i], 175 + i * 20 );
		s_player_extras_box[i].curvalue = s_pm_extras[i];
		s_player_extras_box[i].itemnames = rate_names;	// until PlayerConfig_UpdateExtras
	}

	for ( i = 0; i < sizeof( rate_tbl ) / sizeof( *rate_tbl ) - 1; i++ )
		if ( Cvar_VariableValue( "rate" ) == rate_tbl[i] )
			break;

	PlayerConfig_InitSpin( &s_player_rate_title, M_RUS("connect speed", "CKOPOCT| CBR3U"), &s_player_rate_box, 285 );
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	if ( s_player_head_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_head_title );
		Menu_AddItem( &s_player_config_menu, &s_player_head_box );
	}
	if ( s_player_body_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_body_title );
		Menu_AddItem( &s_player_config_menu, &s_player_body_box );
	}
	if ( s_player_legs_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_legs_title );
		Menu_AddItem( &s_player_config_menu, &s_player_legs_box );
	}
	for ( i = 0; i < MAX_EXTRAS; i++ )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_extras_title[i] );
		Menu_AddItem( &s_player_config_menu, &s_player_extras_box[i] );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );

	Menu_Center( &s_player_config_menu );
	PlayerConfig_UpdateExtras ();

	return true;
}

/*
kingpin.exe @0042e580: the model turns towards the viewer under three
directional lights, one of them wandering, and idles like the leader model
on the HUD (now and then one of three gestures).
*/
float CalcFov( float fov_x, float w, float h );

static int pc_anims[4][2] = {{0, 31}, {37, 46}, {47, 62}, {63, 77}};

void PlayerConfig_MenuDraw( void )
{
	static int	lasttime, frame, oldframe, anim_start, anim_end = 31;
	static int	yaw;
	static float zoom;
	playermodelinfo_s *pmi;
	refdef_t	refdef;
	entity_t	entities[4 + MAX_EXTRAS], *ent;
	char		scratch[MAX_QPATH];
	vec3_t		angles, dir;
	float		backlerp;
	int			m, i, n, r;

	m = s_player_model_box.curvalue;
	pmi = &s_pmi[m];
	s_pm_skinmemory[m][1] = s_player_head_box.curvalue;
	s_pm_skinmemory[m][2] = s_player_body_box.curvalue;
	s_pm_skinmemory[m][0] = s_player_legs_box.curvalue;

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.x = viddef.width / 8 + viddef.width / 2;
	refdef.y = viddef.height / 5;
	refdef.width = (int)(viddef.width * (1.0/640) * 220);
	refdef.height = (int)(viddef.height * (1.0/480) * 350);
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

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
				anim_start = pc_anims[r][0];
				anim_end = pc_anims[r][1];
			}
			else
			{
				anim_start = pc_anims[0][0];
				anim_end = pc_anims[0][1];
			}
			frame = anim_start;
		}
	}
	lasttime = cls.realtime;
	backlerp = 1.0 - (cls.realtime*0.005 - cls.realtime/200);

	memset( entities, 0, sizeof( entities ) );

	// legs, body, head, weapon and the chosen extras
	ent = &entities[0];
	Com_sprintf( scratch, sizeof( scratch ), "players/%s/legs.mdx", pmi->directory );
	ent->model = re.RegisterModel( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/%s.tga", pmi->legsdir, pmi->legsskins[s_player_legs_box.curvalue] );
	ent->skin = re.RegisterSkin( scratch );

	ent = &entities[1];
	Com_sprintf( scratch, sizeof( scratch ), "players/%s/body.mdx", pmi->directory );
	ent->model = re.RegisterModel( scratch );
	Com_sprintf( scratch, sizeof( scratch ), "%s/%s.tga", pmi->bodydir, pmi->bodyskins[s_player_body_box.curvalue] );
	ent->skin = re.RegisterSkin( scratch );

	ent = &entities[2];
	r = pmi->headskins[s_player_head_box.curvalue][5];
	if ( r > '4' )
	{	// skins 5xx and up belong to the alternative heads
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/head%i.mdx", pmi->directory, r - '4' );
		ent->model = re.RegisterModel( scratch );
	}
	if ( !ent->model )
	{
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/head.mdx", pmi->directory );
		ent->model = re.RegisterModel( scratch );
	}
	Com_sprintf( scratch, sizeof( scratch ), "%s/%s.tga", pmi->headdir, pmi->headskins[s_player_head_box.curvalue] );
	ent->skin = re.RegisterSkin( scratch );

	ent = &entities[3];
	Com_sprintf( scratch, sizeof( scratch ), "players/%s/w_shotgun.mdx", pmi->directory );
	ent->model = re.RegisterModel( scratch );

	n = 4;
	for ( i = 0; i < MAX_EXTRAS; i++ )
	{
		if ( s_player_extras_box[i].curvalue <= 0 )
			continue;
		ent = &entities[n++];
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.mdx", pmi->directory,
			s_pm_extranames[i][s_player_extras_box[i].curvalue] );
		ent->model = re.RegisterModel( scratch );
	}

	for ( i = 0; i < n; i++ )
	{
		ent = &entities[i];
		ent->flags = RF_DIMLIGHT;
		ent->angles[1] = yaw;
		VectorSet( ent->origin, 120, 0, -15 );
		ent->origin[0] -= fabs( zoom );
		ent->origin[2] -= zoom * 1.5;
		ent->origin[0] -= 10;
		ent->origin[2] += 10;
		VectorCopy( ent->origin, ent->oldorigin );
		ent->flags2 |= RF2_NOSHADOW | RF2_DIR_LIGHTS;
		ent->model_lighting.num_dir_lights = 3;

		// light 0 wanders around the model and changes colour
		VectorSet( angles, (cls.realtime/50)%360 - 180, (cls.realtime/100)%360 - 180, 0 );
		AngleVectors( angles, dir, NULL, NULL );
		dir[0] = -(fabs( dir[0] )*fabs( dir[0] )) - 0.3;
		dir[1] = -(fabs( dir[1] )*fabs( dir[1] )) - 0.3;
		dir[2] = -(fabs( dir[2] )*fabs( dir[2] )) - 0.3;
		ent->model_lighting.light_colors[0][0] = 1.0 - dir[0]*0.8;
		ent->model_lighting.light_colors[0][1] = 1.0 - dir[1]*0.8;
		ent->model_lighting.light_colors[0][2] = 1.0 - dir[2]*0.8;
		VectorSet( angles, 0, (cls.realtime/20)%360 - 180, 0 );
		AngleVectors( angles, dir, NULL, NULL );
		dir[0] = -(fabs( dir[0] )*fabs( dir[0] )) - 0.3;
		VectorMA( ent->origin, 80, dir, dir );
		VectorSet( ent->model_lighting.light_orgs[0], dir[0], dir[1], -10 );
		ent->model_lighting.light_intensities[0] = 300;

		VectorSet( ent->model_lighting.light_colors[1], 1, 1, 1 );
		VectorSet( ent->model_lighting.light_orgs[1], 60, 60, 0 );
		ent->model_lighting.light_intensities[1] = 200;

		VectorSet( ent->model_lighting.light_colors[2], 1, 1, 1 );
		VectorSet( ent->model_lighting.light_orgs[2], -60, 60, 0 );
		ent->model_lighting.light_intensities[2] = 200;

		ent->frame = frame;
		ent->oldframe = oldframe;
		ent->backlerp = backlerp;
	}
	// sic: kingpin.exe starts facing away and pulled in for the first frame
	yaw = 180;
	zoom = 20;

	refdef.viewangles[PITCH] = 20;
	refdef.rdflags = RDF_NOWORLDMODELALPHA;
	refdef.num_entities = n;
	refdef.entities = entities;

	Menu_Draw( &s_player_config_menu );

	refdef.height += 4;
	re.RenderFrame( &refdef );
}

// kingpin.exe @0042f550: the choice is applied when leaving the menu
const char *PlayerConfig_MenuKey (int key)
{
	char	scratch[1024];
	int		i;

	if ( key == K_ESCAPE )
	{
		playermodelinfo_s *pmi = &s_pmi[s_player_model_box.curvalue];

		Cvar_Set( "name", s_player_name_field.buffer );

		Com_sprintf( scratch, sizeof( scratch ), "%s/%s %s %s", pmi->directory,
			pmi->headskins[s_player_head_box.curvalue] + 5,
			pmi->bodyskins[s_player_body_box.curvalue] + 5,
			pmi->legsskins[s_player_legs_box.curvalue] + 5 );
		Cvar_Set( "skin", scratch );

		for ( i = 0; i < MAX_EXTRAS; i++ )
			scratch[i] = s_player_extras_box[i].curvalue + '0';
		scratch[i] = 0;
		Cvar_Set( "extras", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			FreeFileList( s_pmi[i].headskins, s_pmi[i].nheadskins );
			FreeFileList( s_pmi[i].bodyskins, s_pmi[i].nbodyskins );
			FreeFileList( s_pmi[i].legsskins, s_pmi[i].nlegsskins );
			s_pmi[i].headskins = s_pmi[i].bodyskins = s_pmi[i].legsskins = 0;
			s_pmi[i].nheadskins = s_pmi[i].nbodyskins = s_pmi[i].nlegsskins = 0;
		}
	}
	return Default_MenuKey( &s_player_config_menu, key );
}

/*
kingpin.exe @0042ef90, drawn by the inventory screen in single player: a
thug slowly turning on the left. The player models are rescanned at most
once a second (which also forces hand 0, as PlayerConfig_LoadModels does).
*/
void M_DrawInventoryModel (void)
{
	static int	lastscan;
	static int	yaw;
	refdef_t	refdef;
	entity_t	entities[3];
	float		sx, sy;
	int			i;

	sx = viddef.width * (1.0/640);
	sy = viddef.height * (1.0/480);

	if ( !Com_ServerState() || Cvar_VariableValue( "deathmatch" ) )
		return;

	if ( !lastscan || cls.realtime < lastscan || lastscan < cls.realtime - 1000 )
	{
		if ( !PlayerConfig_LoadModels() )
		{
			Com_Printf( "Unable to load player models\n" );
			return;
		}
	}
	lastscan = cls.realtime;

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.x = 0;
	refdef.y = 0;
	refdef.width = (int)(sx * 320);
	refdef.height = (int)(sy * 470);
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

	memset( entities, 0, sizeof( entities ) );

	// kingpin.exe has a players/ branch here that single player never takes
	entities[2].model = re.RegisterModel( "models/actors/thug/head.mdx" );
	entities[2].skin = re.RegisterSkin( "models/actors/thug/head_009.tga" );
	entities[1].model = re.RegisterModel( "models/actors/thug/body.mdx" );
	entities[1].skin = re.RegisterSkin( "models/actors/thug/body_019.tga" );
	entities[0].model = re.RegisterModel( "models/actors/thug/legs.mdx" );
	entities[0].skin = re.RegisterSkin( "models/actors/thug/legs_017.tga" );

	for ( i = 0; i < 3; i++ )
	{
		entity_t *ent = &entities[i];

		ent->flags = RF_DIMLIGHT;
		ent->angles[1] = yaw;
		VectorSet( ent->origin, 100, 0, -10 );
		VectorCopy( ent->origin, ent->oldorigin );
		ent->flags2 |= RF2_NOSHADOW | RF2_DIR_LIGHTS;
		ent->model_lighting.num_dir_lights = 2;
		VectorSet( ent->model_lighting.light_colors[0], 1, 1, 1 );
		VectorSet( ent->model_lighting.light_orgs[0], 110, 80, -10 );
		ent->model_lighting.light_intensities[0] = 512;
		VectorSet( ent->model_lighting.light_colors[1], 1, 1, 1 );
		VectorSet( ent->model_lighting.light_orgs[1], 0, 80, 0 );
		ent->model_lighting.light_intensities[1] = 256;
	}

	// a degree per frame
	if ( yaw + 1 > 360 )
		yaw = yaw - 359;
	else
		yaw = yaw + 1;

	refdef.height += 4;
	refdef.rdflags = RDF_NOWORLDMODELALPHA;
	refdef.num_entities = 3;
	refdef.entities = entities;

	re.RenderFrame( &refdef );
}

void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( &s_multiplayer_menu, M_RUS("No valid player models found", "HE HAZDEH} MODELU UGPOKOB") );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	M_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}
#else
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#ifdef __psp__
#define MAX_PLAYERMODELS 64
#else
#define MAX_PLAYERMODELS 1024
#endif

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { M_RUS("28.8 Modem", "MODEM 28.8"), M_RUS("33.6 Modem", "MODEM 33.6"), M_RUS("Single ISDN", "ODUH isdn"),
	M_RUS("Dual ISDN/Cable", "DBA isdn/KAVEL|"), "T1/LAN", M_RUS("User defined", "CBOR"), 0 };

void DownloadOptionsFunc( void *self )
{
	M_Menu_DownloadOptions_f();
}

static void HandednessCallback( void *unused )
{
	Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
	s_player_skin_box.itemnames = (const char **)s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

static void FreeFileList( char **list, int n )
{
	int i;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] )
		{
			free( list[i] );
			list[i] = 0;
		}
	}
	free( list );
}

static qboolean IconOfSkinExists( char *skin, char **pcxfiles, int npcxfiles )
{
	int i;
	char scratch[1024];

	strcpy( scratch, skin );
	*strrchr( scratch, '.' ) = 0;
	strcat( scratch, "_i.pcx" );

	for ( i = 0; i < npcxfiles; i++ )
	{
		if ( strcmp( pcxfiles[i], scratch ) == 0 )
			return true;
	}

	return false;
}

static qboolean PlayerConfig_ScanDirectories( void )
{
	char findname[1024];
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames;
	char *path = NULL;
	int i;

	s_numplayermodels = 0;

	/*
	** get a list of directories
	*/
	while (( path = FS_NextPath ( path )) != NULL)
	{
		Com_sprintf ( findname, sizeof(findname), "%s/players/*.*", path );
		if (( dirnames = FS_ListFiles ( findname, &ndirs, SFF_SUBDIR, 0 )) != 0 )
			break;
	}

	if ( !dirnames )
		return false;

	/*
	** go through the subdirectories
	*/
	npms = ndirs;
	if ( npms > MAX_PLAYERMODELS )
		npms = MAX_PLAYERMODELS;

	for ( i = 0; i < npms; i++ )
	{
		int k, s;
		char *a, *b, *c;
		char **pcxnames;
		char **skinnames;
		int npcxfiles;
		int nskins = 0;

		if ( dirnames[i] == 0 )
			continue;

		// verify the existence of tris.md2
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/tris.md2" );

		if ( !Sys_FindFirst( scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			Sys_FindClose();
			continue;
		}
		Sys_FindClose();

		// verify the existence of at least one pcx skin
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/*.pcx" );
		pcxnames = FS_ListFiles( scratch, &npcxfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );

		if ( !pcxnames )
		{
			free( dirnames[i] );
			dirnames[i] = 0;
			continue;
		}

		// count valid skins, which consist of a skin with a matching "_i" icon
		for ( k = 0; k < npcxfiles-1; k++ )
		{
			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					nskins++;
				}
			}
		}
		if ( !nskins )
			continue;

		skinnames = malloc( sizeof( char * ) * ( nskins + 1 ) );
		memset( skinnames, 0, sizeof( char * ) * ( nskins + 1 ) );

		// copy the valid skins
		for ( s = 0, k = 0; k < npcxfiles-1; k++ )
		{
			char *a, *b, *c;

			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					a = strrchr( pcxnames[k], '/' );
					b = strrchr( pcxnames[k], '\\' );

					if ( a > b )
						c = a;
					else
						c = b;

					strcpy( scratch, c + 1 );

					if ( strrchr( scratch, '.' ) )
						*strrchr( scratch, '.' ) = 0;

					skinnames[s] = strdup( scratch );
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		a = strrchr( dirnames[i], '/' );
		b = strrchr( dirnames[i], '\\' );

		if ( a > b )
			c = a;
		else
			c = b;

		strncpy( s_pmi[s_numplayermodels].displayname, c + 1, MAX_DISPLAYNAME-1 );
		strcpy( s_pmi[s_numplayermodels].directory, c + 1 );

		FreeFileList( pcxnames, npcxfiles );

		s_numplayermodels++;
	}
	if ( dirnames )
		FreeFileList( dirnames, ndirs );
}

static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
}


qboolean PlayerConfig_MenuInit( void )
{
	extern cvar_t *name;
	extern cvar_t *team;
	extern cvar_t *skin;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	if ( hand->value < 0 || hand->value > 2 )
		Cvar_SetValue( "hand", 0 );

	strcpy( currentdirectory, skin->string );

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
		strcpy( currentdirectory, "male" );
		strcpy( currentskin, "grunt" );
	}

	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 )
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	s_player_config_menu.x = viddef.width / 2 - 95;
	s_player_config_menu.y = viddef.height / 2 - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = M_RUS("name", "UMR");
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy( s_player_name_field.buffer, name->string );
	s_player_name_field.cursor = strlen( name->string );

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = M_RUS("model", "MODEL|");
	s_player_model_title.generic.x    = -8;
	s_player_model_title.generic.y	 = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = (const char **)s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = M_RUS("skin", "OVLUK");
	s_player_skin_title.generic.x    = -16;
	s_player_skin_title.generic.y	 = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -56;
	s_player_skin_box.generic.y	= 94;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = (const char **)s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = M_RUS("handedness", "BEDYNAR PYKA");
	s_player_hand_title.generic.x    = 32;
	s_player_hand_title.generic.y	 = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -56;
	s_player_handedness_box.generic.y	= 118;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (Cvar_VariableValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = M_RUS("connect speed", "CKOPOCT| CBR3U");
	s_player_rate_title.generic.x    = 56;
	s_player_rate_title.generic.y	 = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -56;
	s_player_rate_box.generic.y	= 166;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= M_RUS("download options", "3AGPY3KA FAZLOB");
	s_player_download_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x	= -24;
	s_player_download_action.generic.y	= 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_download_action );

	return true;
}

void PlayerConfig_MenuDraw( void )
{
	extern float CalcFov( float fov_x, float w, float h );
	refdef_t refdef;
	char scratch[MAX_QPATH];

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = viddef.width / 2;
	refdef.y = viddef.height / 2 - 72;
	refdef.width = 144;
	refdef.height = 168;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
	{
		static int yaw;
		int maxframe = 29;
		entity_t entity;

		memset( &entity, 0, sizeof( entity ) );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
		entity.model = re.RegisterModel( scratch );
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		entity.skin = re.RegisterSkin( scratch );
		entity.flags = RF_FULLBRIGHT;
		entity.origin[0] = 80;
#ifdef __psp__
		entity.origin[1] = -26;
#else
		entity.origin[1] = 0;
#endif
		entity.origin[2] = 0;
		VectorCopy( entity.origin, entity.oldorigin );
		entity.frame = 0;
		entity.oldframe = 0;
		entity.backlerp = 0.0;
		entity.angles[1] = yaw++;
		if ( ++yaw > 360 )
			yaw -= 360;

		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( &s_player_config_menu );

		M_DrawTextBox( ( refdef.x ) * ( 320.0F / viddef.width ) - 8, ( viddef.height / 2 ) * ( 240.0F / viddef.height) - 77, refdef.width / 8, refdef.height / 8 );
		refdef.height += 4;

		re.RenderFrame( &refdef );

		Com_sprintf( scratch, sizeof( scratch ), "/players/%s/%s_i.pcx",
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		re.DrawPic( s_player_config_menu.x - 40, refdef.y, scratch );
	}
}

const char *PlayerConfig_MenuKey (int key)
{
	int i;

	if ( key == K_ESCAPE || key == K_START_BUTTON || key == K_B_BUTTON )
	{
		char scratch[1024];

		Cvar_Set( "name", s_player_name_field.buffer );

		Com_sprintf( scratch, sizeof( scratch ), "%s/%s",
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		Cvar_Set( "skin", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			int j;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( s_pmi[i].skindisplaynames[j] )
					free( s_pmi[i].skindisplaynames[j] );
				s_pmi[i].skindisplaynames[j] = 0;
			}
			free( s_pmi[i].skindisplaynames );
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
	}
	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( &s_multiplayer_menu, M_RUS("No valid player models found", "HE HAZDEH} MODELU UGPOKOB") );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	M_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}
#endif


/*
=======================================================================

GALLERY MENU

=======================================================================
*/
#if 0
void M_Menu_Gallery_f( void )
{
	extern void Gallery_MenuDraw( void );
	extern const char *Gallery_MenuKey( int key );

	M_PushMenu( Gallery_MenuDraw, Gallery_MenuKey );
}
#endif

/*
=======================================================================

QUIT MENU

=======================================================================
*/

const char *M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_START_BUTTON:
	case K_B_BUTTON:
	case 'n':
	case 'N':
		M_PopMenu ();
		break;

	case K_A_BUTTON:
	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		break;
	}

	return NULL;

}


void M_Quit_Draw (void)
{
	int		w, h;

#ifdef KINGPIN_ENGINE
	// kingpin.exe @0042f830: on black
	re.DrawFillRGBA (0, 0, viddef.width, viddef.height, 0, 0, 0, 1);
#endif
	re.DrawGetPicSize (&w, &h, "quit");
	re.DrawPic ( (viddef.width-w)/2, (viddef.height-h)/2, "quit");
}


void M_Menu_Quit_f (void)
{
	M_PushMenu (M_Quit_Draw, M_Quit_Key);
}



//=============================================================================
/* Menu Subsystem */


/*
=================
M_Init
=================
*/
void M_Init (void)
{
	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_game", M_Menu_Game_f);
#ifdef KINGPIN_ENGINE
	Cmd_AddCommand ("menu_notebook", M_Menu_Notebook_f);
#endif
		Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
		Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
		Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
			Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f);
		Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
			Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
		Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
#ifndef KINGPIN_ENGINE
			Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
#endif
		Cmd_AddCommand ("menu_credits", M_Menu_Credits_f );
	Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f );
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
		Cmd_AddCommand ("menu_joy", M_Menu_Joy_f);
		Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	if (cls.key_dest != key_menu)
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cinematictime > 0)
		re.DrawFill (0,0,viddef.width, viddef.height, 0);
	else
		re.DrawFadeScreen ();

	m_drawfunc ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		S_StartLocalSound( menu_in_sound );
		m_entersound = false;
	}
}


/*
=================
M_Keydown
=================
*/
void M_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
		if ( ( s = m_keyfunc( key ) ) != 0 )
			S_StartLocalSound( ( char * ) s );
}


