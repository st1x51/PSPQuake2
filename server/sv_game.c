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
// sv_game.c -- interface to the game dll

#include "server.h"

game_export_t	*ge;


/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client
===============
*/
void PF_Unicast (edict_t *ent, qboolean reliable)
{
	int		p;
	client_t	*client;

	if (!ent)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > maxclients->value)
		return;

	client = svs.clients + (p-1);

	if (reliable)
		SZ_Write (&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
	else
		SZ_Write (&client->datagram, sv.multicast.data, sv.multicast.cursize);

	SZ_Clear (&sv.multicast);
}


/*
===============
PF_dprintf

Debug print to server console
===============
*/
void PF_dprintf (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Com_Printf ("%s", msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
void PF_cprintf (edict_t *ent, int level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;

	if (ent)
	{
		n = NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->value)
			Com_Error (ERR_DROP, "cprintf to a non-client");
	}

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	if (ent)
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else
		Com_Printf ("%s", msg);
}


/*
===============
PF_centerprintf

centerprint to a single client
===============
*/
void PF_centerprintf (edict_t *ent, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	
	n = NUM_FOR_EDICT(ent);
	if (n < 1 || n > maxclients->value)
		return;	// Com_Error (ERR_DROP, "centerprintf to a non-client");

	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	MSG_WriteByte (&sv.multicast,svc_centerprint);
	MSG_WriteString (&sv.multicast,msg);
	PF_Unicast (ent, true);
}


/*
===============
PF_error

Abort the server with a game error
===============
*/
void PF_error (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Com_Error (ERR_DROP, "Game Error: %s", msg);
}


/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
void PF_setmodel (edict_t *ent, char *name)
{
	int		i;
	cmodel_t	*mod;

	if (!name)
		Com_Error (ERR_DROP, "PF_setmodel: NULL");

	i = SV_ModelIndex (name);
		
//	ent->model = name;
	ent->s.modelindex = i;

// if it is an inline model, get the size information for it
	if (name[0] == '*')
	{
		mod = CM_InlineModel (name);
		VectorCopy (mod->mins, ent->mins);
		VectorCopy (mod->maxs, ent->maxs);
		SV_LinkEdict (ent);
	}

}

/*
===============
PF_Configstring

===============
*/
void PF_Configstring (int index, char *val)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i\n", index);

	if (!val)
		val = "";

	// change the string in sv
	strcpy (sv.configstrings[index], val);

	
	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_WriteChar (&sv.multicast, svc_configstring);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteString (&sv.multicast, val);

		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
	}
}



void PF_WriteChar (int c) {MSG_WriteChar (&sv.multicast, c);}
void PF_WriteByte (int c) {MSG_WriteByte (&sv.multicast, c);}
void PF_WriteShort (int c) {MSG_WriteShort (&sv.multicast, c);}
void PF_WriteLong (int c) {MSG_WriteLong (&sv.multicast, c);}
void PF_WriteFloat (float f) {MSG_WriteFloat (&sv.multicast, f);}
void PF_WriteString (char *s) {MSG_WriteString (&sv.multicast, s);}
void PF_WritePos (vec3_t pos) {MSG_WritePos (&sv.multicast, pos);}
void PF_WriteDir (vec3_t dir) {MSG_WriteDir (&sv.multicast, dir);}
void PF_WriteAngle (float f) {MSG_WriteAngle (&sv.multicast, f);}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean PF_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks sight
	return true;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
qboolean PF_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks hearing

	return true;
}

void PF_StartSound (edict_t *entity, int channel, int sound_num, float volume,
    float attenuation, float timeofs)
{
	if (!entity)
		return;
	SV_StartSound (NULL, entity, channel, sound_num, volume, attenuation, timeofs);
}

//==============================================

#ifdef KINGPIN_ENGINE
/*
===============================================================================

.MDX OBJECT BOUNDS

Each object of an .mdx model part has a bounding box per frame. The game
dll owns the array of bounds pointers; the server loads the boxes out of
the .mdx file and stores 1-based indexes into that array in
model_part->object_bounds[] (kingpin.exe @0043c270).

===============================================================================
*/

#define	MAX_OBJECTBOUNDS_FILES	256
#define	MAX_OBJECTBOUNDS		2048	// size of the game dll's array

static char	objectbounds_files[MAX_OBJECTBOUNDS_FILES][MAX_QPATH];
static int	objectbounds_indexes[MAX_OBJECTBOUNDS_FILES][MAX_MODELPART_OBJECTS];
static int	num_objectbounds_files;

/*
===============
PF_ClearObjectBoundsCached

Called by the game when it frees its object bounds (new level)
===============
*/
void PF_ClearObjectBoundsCached (void)
{
	num_objectbounds_files = 0;
}

static void MDX_SetBoundsFilename (model_part_t *part, char *name)
{
	// the game's TAG_LEVEL, freed on level change
	part->objectbounds_filename = Z_TagMalloc (strlen(name)+1, TAG_LEVEL);
	strcpy (part->objectbounds_filename, name);
}

/*
===============
MDX_GetObjectBounds
===============
*/
void MDX_GetObjectBounds (char *name, model_part_t *part)
{
	int				i, j;
	byte			*buf;
	dmdx_t			header;
	int				*numbounds;
	object_bounds_t	**bounds;
	object_bounds_t	*b;

	// already loaded for another entity
	for (i=0 ; i<num_objectbounds_files ; i++)
	{
		if (strcmp (name, objectbounds_files[i]))
			continue;
		memcpy (part->object_bounds, objectbounds_indexes[i], sizeof(part->object_bounds));
		MDX_SetBoundsFilename (part, name);
		return;
	}

	if (num_objectbounds_files >= MAX_OBJECTBOUNDS_FILES)
	{
		Com_Printf ("MDX_GetObjectBounds: Reached MAX_MODELS limit, unable to load object-bounds\n");
		return;
	}

	// temp
	buf = FS_LoadFile (name, NULL, FS_PATH_ALL | FS_FLAG_MTEMP);
	if (!buf)
	{
		Com_Printf ("MDX_GetObjectBounds: Unable to find MDX file \"%s\"\nUsing simple collision detection.\n", name);
		return;
	}

	numbounds = ge->GetNumObjectBounds ();
	bounds = (object_bounds_t **)ge->GetObjectBoundsPointer ();

	for (i=0 ; i<sizeof(header)/4 ; i++)
		((int *)&header)[i] = LittleLong (((int *)buf)[i]);

	memset (part->object_bounds, 0, sizeof(part->object_bounds));

	if (*numbounds > MAX_OBJECTBOUNDS)
	{
		Com_Printf ("MDX_GetObjectBounds: Out of ObjectBounds items\n");
		//FS_FreeFile (buf); // FS_FLAG_MTEMP !
		return;
	}

	strcpy (objectbounds_files[num_objectbounds_files], name);
	memset (objectbounds_indexes[num_objectbounds_files], 0, sizeof(objectbounds_indexes[0]));

	for (i=0 ; i<header.num_subobjects && i<MAX_MODELPART_OBJECTS ; i++)
	{
		// the game's TAG_LEVEL, freed on level change
		b = Z_TagMalloc (header.num_frames * sizeof(object_bounds_t), TAG_LEVEL);
		memcpy (b, buf + header.ofs_bbox_frames + header.num_frames * i * sizeof(object_bounds_t),
			header.num_frames * sizeof(object_bounds_t));
		for (j=0 ; j<header.num_frames*6 ; j++)
			((float *)b)[j] = LittleFloat (((float *)b)[j]);

		bounds[*numbounds] = b;
		(*numbounds)++;
		part->object_bounds[i] = *numbounds;
		objectbounds_indexes[num_objectbounds_files][i] = *numbounds;
	}

	MDX_SetBoundsFilename (part, name);
	num_objectbounds_files++;

	//FS_FreeFile (buf); // FS_FLAG_MTEMP !
}

/*
===============
PF_StopRender

Keeps the loading plaque up until the next SCR_EndLoadingPlaque
===============
*/
void PF_StopRender (void)
{
	com_stoprender = true;
}
#endif

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs (void)
{
	if (!ge)
		return;
	ge->Shutdown ();
	Sys_UnloadGame ();
	ge = NULL;
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SCR_DebugGraph (float value, int color);
void SV_InitGameProgs (void)
{
	game_import_t	import;

	// unload anything we have now
	if (ge)
		SV_ShutdownGameProgs ();


	// load a new game dll
	import.multicast = SV_Multicast;
	import.unicast = PF_Unicast;
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = Pmove;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;
#ifdef KINGPIN_ENGINE
	import.skinindex = SV_SkinIndex;
#endif

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteFloat = PF_WriteFloat;
	import.WriteString = PF_WriteString;
	import.WritePosition = PF_WritePos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;

	import.TagMalloc = Z_TagMallocGame;
	import.TagFree = Z_FreeGame;
	import.FreeTags = Z_FreeTagsGame;
#ifdef KINGPIN_ENGINE
	import.ClearObjectBoundsCached = PF_ClearObjectBoundsCached;
	import.StopRender = PF_StopRender;
#endif

	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.DebugGraph = SCR_DebugGraph;
#ifdef KINGPIN_ENGINE
	import.GetObjectBounds = MDX_GetObjectBounds;
	import.SaveCurrentGame = SV_SaveCurrentGame;
#endif
	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	import.FileOpen = FS_FOpen;
	import.FileLength = FS_FLength;
	import.FileRead = FS_FRead;
	import.FileWrite = FS_FWrite;
	import.FilePrintf = FS_FPrintf;
	import.FileSeek = FS_FSeek;
	import.FileClose = FS_FClose;
	import.GetWriteDir  = FS_GetWriteDir;
#ifndef KINGPIN_ENGINE
	import.monster_flash_offset = monster_flash_offset;
#endif
	// v0.1 API END

	import.FileGetc = FS_Getc;
	import.FileUnGetc = FS_UnGetc;
	import.FileGets = FS_Gets;

	ge = (game_export_t *)Sys_GetGameAPI (&import);

	if (!ge)
		Com_Error (ERR_DROP, "failed to load game DLL");
	if (ge->apiversion > GAME_API_VERSION)
		Com_Error (ERR_DROP, "game is version %i, not <= %i", ge->apiversion,
		GAME_API_VERSION);

	ge->Init ();
}

