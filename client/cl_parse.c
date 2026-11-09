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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

#ifdef KINGPIN_ENGINE
char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_muzzlflash3",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",
	"svc_hud",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame",
	"svc_configstring_pointer"
};
#else
char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame"
};
#endif

//=============================================================================

int CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	/* FS_FOpen/FS_FileRename add the selected write directory themselves. */
	Com_sprintf (dest, destlen, "%s", fn);

	if (strncmp(fn, "players", 7) == 0)
		return FS_PATH_BASEDIR;
	return FS_PATH_GAMEDIR;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (char *filename)
{
	file_t *fp;
	char	name[MAX_OSPATH];
	int		len;
	int		pathflag;

#ifdef KINGPIN_ENGINE
	// kingpin.exe CL_CheckOrDownloadFile @0040f690 does not request
	// downloads when they are globally disabled or when we are connected to
	// the local server.  In particular, the latter keeps a single-player game
	// from walking the whole precache list and asking itself for loose files
	// which are already available through pak0.pak.
	if (!Cvar_VariableValue ("cl_allow_downloads") || Com_ServerState ())
		return true;
#endif

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..\n");
		return true;
	}

	if (FS_FileExists (filename, 0))
	{	// it exists, no need to download
		return true;
	}

	strcpy (cls.downloadname, filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	pathflag = CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

	fp = FS_FOpen (name, FS_MODE_APPEND | pathflag);
	if (fp) { // it exists
		len = FS_FLength(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s %i", cls.downloadname, len));
	} else {
		Com_Printf ("Downloading %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
			va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with ..\n");
		return;
	}

	if (FS_FileExists (filename, 0))
	{	// it exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	strcpy (cls.downloadname, filename);
	Com_Printf ("Downloading %s\n", cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
		va("download %s", cls.downloadname));

	cls.downloadnumber++;
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	S_EndRegistration ();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	char	name[MAX_OSPATH];
	int		r;
	int		pathflag;

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);
	if (size == -1)
	{
		Com_Printf ("Server does not have this file.\n");
		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			FS_FClose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		pathflag = CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		cls.download = FS_FOpen (name, FS_MODE_WRITE | pathflag);
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	FS_FWrite (cls.download, net_message.data + net_message.readcount, size);
	net_message.readcount += size;

	if (percent != 100)
	{
		// request next block
// change display routines by zoid
#if 0
		Com_Printf (".");
		if (10*(percent/10) != cls.downloadpercent)
		{
			cls.downloadpercent = 10*(percent/10);
			Com_Printf ("%i%%", cls.downloadpercent);
		}
#endif
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		char	oldn[MAX_OSPATH];
		char	newn[MAX_OSPATH];

//		Com_Printf ("100%%\n");

		FS_FClose (cls.download);

		// rename the temp file to it's final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		pathflag = CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = FS_FileRename (oldn, newn, pathflag);
		if (!r)
			Com_Printf ("failed to rename.\n");

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;
	
	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	// BIG HACK to let demos from release work with the 3.0x patch!!!
#ifdef KINGPIN_ENGINE
	if (!Com_ServerState() && i != PROTOCOL_VERSION)
#else
	if (Com_ServerState() && PROTOCOL_VERSION == 34)
	{
	}
	else if (i != PROTOCOL_VERSION)
#endif
		Com_Error (ERR_DROP,"Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	int				bits;
	int				newnum;
	entity_state_t	nullstate;

	memset (&nullstate, 0, sizeof(nullstate));

	newnum = CL_ParseEntityBits (&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&nullstate, es, newnum, bits);
}

#ifdef KINGPIN_ENGINE
/*
================
CL_OpenPlayerFile

Kingpin reads extras.txt and skinfolder.txt straight from the main
directory under basedir, falling back to cddir, not through the pak files.
================
*/
static file_t *CL_OpenPlayerFile (char *modeldir, char *file)
{
	char	path[MAX_OSPATH];
#if 1
	Com_sprintf (path, sizeof(path), "%s%s", modeldir, file);
	return FS_FOpen (path, FS_MODE_READ | FS_PATH_BASEDIR | FS_TYPE_RFS);
#else
	FILE	*f;

	Com_sprintf (path, sizeof(path), "%s/%s/%s%s", Cvar_Get ("basedir", "", 0)->string, BASEDIRNAME, modeldir, file);
	f = fopen (path, "r");
	if (f)
		return f;

	Com_sprintf (path, sizeof(path), "%s/%s/%s%s", Cvar_Get ("cddir", "", 0)->string, BASEDIRNAME, modeldir, file);
	return fopen (path, "r");
#endif
}

/*
================
CL_LoadModelParts

head, legs and body .mdx for a player model, plus optional extras.

extras has one digit per extras.txt group ('0' = none); a digit n picks
the n'th item of the group. extras.txt lists groups as a digit line
followed by "+ name" lines (kingpin.exe @0040fd50).
================
*/
/* Portable equivalent of the reference's bounded fscanf("%s") reads.
 * Keep EOF as an int and accept all whitespace, including blank lines. */
static qboolean CL_ReadPlayerToken (file_t *file, char *token, size_t capacity)
{
	int c;
	size_t length = 0;
	do {
		c = FS_Getc (file);
	} while (c != FS_EOF && isspace((unsigned char)c));
	while (c != FS_EOF && !isspace((unsigned char)c))
	{
		if (length + 1 < capacity)
			token[length++] = (char)c;
		c = FS_Getc (file);
	}
	token[length] = '\0';
	return length != 0;
}

static void CL_LoadModelParts (clientinfo_t *ci, char *modeldir, char *extras)
{
	char	name[MAX_QPATH];
	char	items[4][8][16];
	char	title[64];
	int		c;
	int		i, group;
	file_t	*f;

	memset (ci->model, 0, sizeof(ci->model));

	Com_sprintf (name, sizeof(name), "%shead.mdx", modeldir);
	ci->model[0] = re.RegisterModel (name);
	Com_sprintf (name, sizeof(name), "%slegs.mdx", modeldir);
	ci->model[1] = re.RegisterModel (name);
	Com_sprintf (name, sizeof(name), "%sbody.mdx", modeldir);
	ci->model[2] = re.RegisterModel (name);

	// any extras?
	for (i=0 ; ; i++)
	{
		if (!extras[i] || i > 3)
			return;
		if (extras[i] > '0')
			break;
	}

	f = CL_OpenPlayerFile (modeldir, "extras.txt");
	memset (items, 0, sizeof(items));
	if (!f)
		return;
	group = 0;
	while ((c = FS_Getc(f)) != FS_EOF)
	{
		if (c >= '1' && c <= '9')
		{	// group heading
			group = c - '1';

			CL_ReadPlayerToken (f, title, sizeof(title));
		}
		else if (c == '+')
		{	// item of the current group
			for (i=0 ; i<8 ; i++)
				if (!items[group & 3][i][0])
					break;

			if (i == 8)
				CL_ReadPlayerToken (f, title, sizeof(title));
			else
				CL_ReadPlayerToken (f, items[group & 3][i], sizeof(items[0][0]));
		}
	}
	FS_FClose (f);
	for (i=0 ; extras[i] && i<4 && 3+i<MAX_CLIENTMODELPARTS ; i++)
	{
		if (extras[i] <= '0' || extras[i] > '8')
			continue;
		Com_sprintf (name, sizeof(name), "%s%s.mdx", modeldir, items[i][extras[i] - '1']);
		ci->model[3+i] = re.RegisterModel (name);
	}
}

/*
================
CL_LoadPartSkins

skins is "HHH BBB LLL": head_HHH.tga, body_BBB.tga and legs_LLL.tga,
looked up in the folders named by skinfolder.txt (either one folder for
all parts, or "head:", "body:", "legs:" lines). Heads 5xx and up use
head1.mdx, head2.mdx... (kingpin.exe @00410190)
================
*/
static void CL_LoadPartSkins (clientinfo_t *ci, char *skins, char *modeldir)
{
	char	lines[3][64];
	char	headdir[64], bodydir[64], legsdir[64];
	char	name[MAX_QPATH];
	char	*dir;
	int		i;
	file_t	*f;

	Com_sprintf (headdir, sizeof(headdir), "%s", modeldir);
	Com_sprintf (bodydir, sizeof(bodydir), "%s", modeldir);
	Com_sprintf (legsdir, sizeof(legsdir), "%s", modeldir);

	f = CL_OpenPlayerFile (modeldir, "skinfolder.txt");
	if (f)
	{
		memset (lines, 0, sizeof(lines));
		for (i = 0; i < 3; i++)
		{
			if (!CL_ReadPlayerToken (f, lines[i], sizeof(lines[i])))
				break;
		}
		
		FS_FClose(f);

		if (lines[0][0])
		{
			if (!strstr (lines[0], "body:") && !strstr (lines[0], "legs:") && !strstr (lines[0], "head:"))
			{	// one folder for everything
				Com_sprintf (headdir, sizeof(headdir), "%s", lines[0]);
				Com_sprintf (bodydir, sizeof(bodydir), "%s", lines[0]);
				Com_sprintf (legsdir, sizeof(legsdir), "%s", lines[0]);
			}
			else
			{
				for (i=0 ; i<3 ; i++)
				{
					if (strstr (lines[i], "body:"))
						Com_sprintf (bodydir, sizeof(bodydir), "%s", lines[i]+5);
					else if (strstr (lines[i], "head:"))
						Com_sprintf (headdir, sizeof(headdir), "%s", lines[i]+5);
					else if (strstr (lines[i], "legs:"))
						Com_sprintf (legsdir, sizeof(legsdir), "%s", lines[i]+5);
				}
			}
		}

		for (dir = headdir ; dir ; dir = (dir == headdir) ? bodydir : (dir == bodydir) ? legsdir : NULL)
			if (dir[0] && dir[strlen(dir)-1] != '/')
				strcat (dir, "/");
	}

	// split "HHH BBB LLL"
	skins[3] = 0;
	skins[7] = 0;

	Com_sprintf (name, sizeof(name), "%shead_%s.tga", headdir, skins);
	ci->skin[0] = re.RegisterSkin (name);
	if (ci->skin[0] && skins[0] > '4')
	{	// alternate head model
		Com_sprintf (name, sizeof(name), "%shead%i.mdx", modeldir, skins[0] - '4');
		if (re.RegisterModel (name))
			ci->model[0] = re.RegisterModel (name);
	}

	Com_sprintf (name, sizeof(name), "%slegs_%s.tga", legsdir, skins+8);
	ci->skin[1] = re.RegisterSkin (name);

	Com_sprintf (name, sizeof(name), "%sbody_%s.tga", bodydir, skins+4);
	ci->skin[2] = re.RegisterSkin (name);

	skins[3] = ' ';
	skins[7] = ' ';
}

/*
================
CL_LoadDefaultClientinfo

male_thug with the 001 skins
================
*/
static void CL_LoadDefaultClientinfo (clientinfo_t *ci, char *iconname)
{
	char	skins[16];

	strcpy (skins, "001 001 001");
	Com_sprintf (ci->iconname, sizeof(ci->iconname), "%s", iconname);
	CL_LoadModelParts (ci, "players/male_thug/", "0");
	memset (ci->weaponmodel, 0, sizeof(ci->weaponmodel));
	ci->weaponmodel[0] = re.RegisterModel ("players/male_thug/gun.mdx");
	CL_LoadPartSkins (ci, skins, "players/male_thug/");
}

/*
================
CL_LoadClientinfo

s is "name\model/HHH BBB LLL[ extras]" (kingpin.exe @00410830)
================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int		i;
	char	*t;
	char	model_name[MAX_QPATH];
	char	skin_name[MAX_QPATH];
	char	modeldir[MAX_QPATH];
	char	skins[MAX_QPATH];
	char	weapon_filename[MAX_QPATH];
	char	*extras;

	Com_sprintf (ci->name, sizeof(ci->name), "%s", s);
	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if (cl_noskins->value || *s == 0)
	{
		CL_LoadDefaultClientinfo (ci, "/players/male_thug/head_001.tga");
		ci->icon = re.RegisterPic (ci->iconname);
		return;
	}

	// isolate the model name
	Com_sprintf (model_name, sizeof(model_name), "%s", s);
	t = strstr (model_name, "/");
	if (!t)
		t = strstr (model_name, "\\");
	if (!t)
		t = model_name;
	*t = 0;

	// isolate the skin name
	Com_sprintf (skin_name, sizeof(skin_name), "%s", s + strlen(model_name) + 1);

	// "HHH BBB LLL" is 11 characters, extras follow after a blank
	if (strlen (skin_name) < 12)
		extras = "0";
	else
	{
		extras = skin_name + 12;
		skin_name[11] = 0;
	}

	Com_sprintf (modeldir, sizeof(modeldir), "players/%s/", model_name);
	CL_LoadModelParts (ci, modeldir, extras);
	if (!ci->model[0] || !ci->model[1] || !ci->model[2])
	{
		strcpy (model_name, "male_thug");
		Com_sprintf (modeldir, sizeof(modeldir), "players/male_thug/");
		CL_LoadModelParts (ci, modeldir, "0");
	}

	Com_sprintf (skins, sizeof(skins), "%s", skin_name);
	CL_LoadPartSkins (ci, skins, modeldir);

	// if we don't have the skins, use male_thug
	if (!ci->skin[0] || !ci->skin[1] || (!ci->skin[2] && strcmp (model_name, "male_thug")))
		CL_LoadDefaultClientinfo (ci, "players/male_thug/head_001.tga");

	// weapon file
	for (i=0 ; i<num_cl_weaponmodels ; i++)
	{
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
		ci->weaponmodel[i] = re.RegisterModel (weapon_filename);
		if (!cl_vwep->value)
			break; // only one when vwep is off
	}
}
#else
/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo)-1] = 0;

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name)-1] = 0;
	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if (cl_noskins->value || *s == 0)
	{
		Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/weapon.md2");
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/male/grunt.pcx");
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/male/grunt_i.pcx");
		ci->model = re.RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = re.RegisterModel (weapon_filename);
		ci->skin = re.RegisterSkin (skin_filename);
		ci->icon = re.RegisterPic (ci->iconname);
	}
	else
	{
		// isolate the model name
		strcpy (model_name, s);
		t = strstr(model_name, "/");
		if (!t)
			t = strstr(model_name, "\\");
		if (!t)
			t = model_name;
		*t = 0;

		// isolate the skin name
		strcpy (skin_name, s + strlen(model_name) + 1);

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = re.RegisterModel (model_filename);
		if (!ci->model)
		{
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = re.RegisterSkin (skin_filename);

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name, skin_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
				// try male
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
			}
			if (!cl_vwep->value)
				break; // only one when vwep is off
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = re.RegisterPic (ci->iconname);
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}
#endif

/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}

#ifdef KINGPIN_ENGINE
/*
================
CL_ConfigStringChanged

Kingpin's handling of a new configstring value (kingpin.exe @00410cb0).
Called for svc_configstring and for every string of an
svc_configstring_pointer.
================
*/
void CL_ConfigStringChanged (int i)
{
	char	*s;
	char	skin[32];
	int		len, n;
	int		style, type;
	float	f;
	lightflare_t	*fl;
	cmlight_t		*ml;

	s = cl.configstrings[i];

	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - CS_LIGHTS);
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (s);
			if (s[0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (s);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_SOUNDS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (s);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_IMAGES)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic (s);
	}
	else if (i >= CS_MODELSKINS && i < CS_MODELSKINS+MAX_MODELS)
	{
		// the string holds all skins used with a model, each n chars long;
		// the server appends n after the string for new skins (see
		// SV_SkinIndex), which is only read once the refresh is prepped
		if (!cl.refresh_prepped)
			return;
		n = MSG_ReadByte (&net_message);
		if (n <= 0 || n >= sizeof(skin) - 2)
			return;
		len = strlen (s);
		strncpy (skin, s + (len/n - 1)*n, n);
		skin[n] = 0;

		// the skin and its two pain skins
		re.RegisterModelSkin (cl.model_draw[i-CS_MODELSKINS], skin);
		strcat (skin, "p1");
		re.RegisterModelSkin (cl.model_draw[i-CS_MODELSKINS], skin);
		skin[n] = 0;
		strcat (skin, "p2");
		re.RegisterModelSkin (cl.model_draw[i-CS_MODELSKINS], skin);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped)
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}
	else if (i >= CS_LIGHTFLARES && i < CS_LIGHTFLARES+MAX_LIGHTFLARES)
	{
		// flares are appended in the order they arrive
		fl = &cl_lightflares[i - CS_LIGHTFLARES];
		sscanf (s, "%i %i %f %f %f %f", &style, &type,
			&fl->pos[0], &fl->pos[1], &fl->pos[2], &fl->size);
		fl->lightstyle = style;
		fl->type = type;
		cl_numlightflares = i - CS_LIGHTFLARES + 1;
	}
	else if (i >= CS_JUNIORS && i < CS_JUNIORS+MAX_JUNIOR_STRINGS)
	{
		// model light sources, referenced by entity_state_t.model_lighting
		if (cl_nummodellights >= MAX_MODELLIGHTS)
			return;
		ml = &cl_modellights[cl_nummodellights++];
		sscanf (s, "%f %f %f %f %f %f %i %i",
			&ml->origin[0], &ml->origin[1], &ml->origin[2],
			&ml->color[0], &ml->color[1], &ml->color[2], &n, &style);
		f = n;
		ml->intensity = f;
		ml->style = style;
	}
}

/*
================
CL_ParseConfigStringPointer

svc_configstring_pointer: a single player server in this process sends
the address of its configstrings instead of the strings themselves
================
*/
void CL_ParseConfigStringPointer (void)
{
	int		i;
	char	*serverstrings;

	serverstrings = (char *)MSG_ReadLong (&net_message);
	if (!Com_ServerState())
		Com_Error (ERR_DROP, "svc_configstring_pointer from a remote server");

	memcpy (cl.configstrings, serverstrings, sizeof(cl.configstrings));

	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
		if (cl.configstrings[i][0])
			CL_ConfigStringChanged (i);
}
#endif

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	int		i;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	s = MSG_ReadString(&net_message);

	strncpy (olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	strcpy (cl.configstrings[i], s);

#ifdef KINGPIN_ENGINE
	CL_ConfigStringChanged (i);
#else
	// do something apropriate 

	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - CS_LIGHTS);
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic (cl.configstrings[i]);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped && strcmp(olds, s))
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}
#endif
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;
	float	ofs;

	flags = MSG_ReadByte (&net_message);
	sound_num = MSG_ReadByte (&net_message);

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
#ifdef KINGPIN_ENGINE
	else if (flags & SND_PITCH)
		ofs = MSG_ReadByte (&net_message) / -64.0;	// negative = pitch
#endif
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       


void SHOWNET(char *s)
{
	if (cl_shownet->value>=2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;

//
// if recording demos, copy the message out
//
	if (cl_shownet->value == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->value >= 2)
		Com_Printf ("------------------\n");


//
// parse the message
//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value>=2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message\n");
			break;
			
		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;
			
		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected\n");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				FS_FClose (cls.download);
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			if (i == PRINT_CHAT)
			{
				S_StartLocalSound ("misc/talk.wav");
				con.ormask = 128;
			}
			Com_Printf ("%s", MSG_ReadString (&net_message));
			con.ormask = 0;
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			break;
			
		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_muzzleflash:
			CL_ParseMuzzleFlash ();
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			break;

#ifdef KINGPIN_ENGINE
		case svc_muzzleflash3:
			CL_ParseMuzzleFlash3 ();
			break;

		case svc_hud:
			Cvar_SetValue ("scr_hud", scr_hud->value + 1 > 2 ? 0 : scr_hud->value + 1);
			break;

		case svc_configstring_pointer:
			CL_ParseConfigStringPointer ();
			break;
#endif

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_frame:
			CL_ParseFrame ();
			break;

		case svc_inventory:
			CL_ParseInventory ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;
		}
	}

	CL_AddNetgraph ();

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if (cls.demorecording && !cls.demowaiting)
		CL_WriteDemoMessage ();

}


