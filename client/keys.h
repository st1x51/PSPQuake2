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

typedef enum KeyDefs_e
{
	K_UNKNOWN,
	K_LEFTARROW,
	K_RIGHTARROW,
	K_DOWNARROW,
	K_UPARROW,

	K_SQUARE,
	K_CIRCLE,
	K_CROSS,
	K_TRIANGLE,

	K_LTRIGGER,
	K_RTRIGGER,

	K_START,
	K_SELECT,
	K_NOTE,
	K_HOLD,
	
	K_EXT1,
	K_EXT2,
	K_EXT3,
	K_EXT4,

	K_EP_PLAYPAUSE,
	K_EP_FORWARD,
	K_EP_BACK,
	K_EP_VOLUP,
	K_EP_VOLDOWN,
	K_EP_HOLD,

	K_ANUB_LEFT,
	K_ANUB_RIGHT,
	K_ANUB_DOWN,
	K_ANUB_UP,

	K_MAX
} KeyDefs;

extern char		*keybindings[256];
extern	int		key_repeats[256];

extern int anykeydown;
extern char chat_buffer[];
extern int chat_bufferlen;
extern qboolean	chat_team;

void Key_Event(int key, qboolean down, unsigned time);
void Key_Init(void);
#ifdef IOCTRL
void Key_WriteBindings(int   f);
#else
void Key_WriteBindings(FILE *f);
#endif
void Key_SetBinding(int keynum, char *binding);
void Key_ClearStates(void);
int  Key_GetKey(void);

