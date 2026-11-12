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
// cl_view.c -- player rendering positioning

#include "client.h"

//=============
//
// development tools for weapons
//
int			gun_frame;
struct model_s	*gun_model;

//=============

cvar_t		*crosshair;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;
#ifdef KINGPIN_ENGINE
cvar_t		*cl_rearviewmirror;
#endif


int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

#ifdef KINGPIN_ENGINE
int			r_numparticles2;
particle_t	r_particles2[MAX_PARTICLES2];

int			r_numflares;
dlight_t	r_flares[MAX_VISIBLE_FLARES];

int			r_numsurflights;
surflight_t	r_surflights[MAX_SURFLIGHTS];
#endif

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
#ifdef KINGPIN_ENGINE
	r_numparticles2 = 0;
	r_numsurflights = 0;
#endif
}


/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (entity_t *ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;
	r_entities[r_numentities++] = *ent;
}


/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (vec3_t org, int color, float alpha)
{
	particle_t	*p;

	if (r_numparticles >= MAX_PARTICLES)
		return;
	p = &r_particles[r_numparticles++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
#ifdef KINGPIN_ENGINE
	p->type = 0;
	p->frame = 0;
#endif
}

#ifdef KINGPIN_ENGINE
/*
=====================
V_AddParticleEx
=====================
*/
void V_AddParticleEx (vec3_t org, int color, float alpha, int type, int frame)
{
	particle_t	*p;

	if (r_numparticles >= MAX_PARTICLES)
		return;
	p = &r_particles[r_numparticles++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
	p->type = type;
	p->frame = frame;
}

/*
=====================
V_AddParticle2

Particles drawn by the renderer's second particle pass
=====================
*/
void V_AddParticle2 (vec3_t org, int color, float alpha, int type, int frame)
{
	particle_t	*p;

	if (r_numparticles2 >= MAX_PARTICLES2)
		return;
	p = &r_particles2[r_numparticles2++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
	p->type = type;
	p->frame = frame;
}

/*
=====================
V_AddSurfLight

A light that only lights (or with a negative intensity, darkens)
world surfaces within radius
=====================
*/
void V_AddSurfLight (vec3_t org, float radius, float intensity, float r, float g, float b)
{
	surflight_t	*sl;

	if (r_numsurflights >= MAX_SURFLIGHTS)
		return;
	sl = &r_surflights[r_numsurflights++];
	VectorCopy (org, sl->origin);
	sl->radius = radius;
	sl->intensity = intensity;
	sl->color[0] = r;
	sl->color[1] = g;
	sl->color[2] = b;
}

/*
=====================
V_InPVS

Same test as the server's PF_inPVS, which kingpin.exe calls from the client
=====================
*/
static qboolean V_InPVS (vec3_t p1, vec3_t p2)
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
=====================
V_AddFlares

Flares from CS_LIGHTFLARES. Kingpin's refdef light list is PVS/distance
filtered independently of the traced, fading AddSFX sprites (0041a8b0).
=====================
*/
void V_ClearFlares (void)
{
	r_numflares = 0;
}

static qboolean V_FlareVisible (vec3_t target, vec3_t dir)
{
	vec3_t start;
	trace_t tr;
	int passes;

	VectorCopy (cl.refdef.vieworg, start);
	tr = CL_PMTrace (start, vec3_origin, vec3_origin, target);
	for (passes=0 ; tr.contents & (CONTENTS_ALPHA | CONTENTS_PLAYERCLIP | CONTENTS_LADDER) ; passes++)
	{
		if (passes == 63)
			return false;
		VectorMA (tr.endpos, 6, dir, start);
		tr = CL_PMTrace (start, vec3_origin, vec3_origin, target);
		if (tr.contents & CONTENTS_SOLID)
			break;
	}

	return tr.fraction >= 1 && !tr.startsolid;
}

static float V_SunVisibility (vec3_t target)
{
	vec3_t dir, start;
	vec3_t forward, right, up;
	vec3_t sample_start, sample_end;
	trace_t tr;
	float x, y;
	int hits, samples, passes;
	static vec3_t mins = {-8, -8, -8};
	static vec3_t maxs = {8, 8, 8};

	VectorSubtract (target, cl.refdef.vieworg, dir);
	VectorNormalize (dir);
	VectorCopy (cl.refdef.vieworg, start);

	/* Alpha-tested fences are stepped through in 12-unit increments. */
	for (passes=0 ; ; passes++)
	{
		tr = CL_PMTrace (start, mins, maxs, target);
		if (!(tr.contents & CONTENTS_ALPHA))
			break;
		if (passes == 63)
			return 0;
		VectorMA (tr.endpos, 12, dir, start);
	}

	if (tr.fraction >= 1 || (tr.surface && (tr.surface->flags & SURF_SKY)))
		return 1;

	/*
	 * A blocked sun ray is sampled on the original 5 by 5 grid.  Only
	 * samples which actually terminate on a sky surface contribute.
	 */
	AngleVectors (cl.refdef.viewangles, forward, right, up);
	hits = 0;
	samples = 0;
	for (x=-8 ; x<=8 ; x+=4)
	{
		for (y=-8 ; y<=8 ; y+=4)
		{
			VectorMA (cl.refdef.vieworg, x, right, sample_start);
			VectorMA (target, y, up, sample_end);
			tr = CL_PMTrace (sample_start, vec3_origin, vec3_origin, sample_end);
			if (tr.fraction != 1 && tr.surface && (tr.surface->flags & SURF_SKY))
				hits++;
			samples++;
		}
	}

	return samples ? (float)hits / samples : 1;
}

static int V_FlareSprite (int type)
{
	switch (type)
	{
	case FLARE_AMBER: return 0x11;
	case FLARE_RED: return 0x12;
	case FLARE_BLUE: return 0x14;
	case FLARE_GREEN: return 0x13;
	default: return 0x0a;
	}
}

void V_AddFlares (void)
{
	int			i;
	dlight_t	*fl;
	lightflare_t *flare;
	vec3_t target, dir, viewforward;
	float distance, dot, size, alpha, end_alpha, lifetime, coverage;
	qboolean visible, in_pvs, fading;
	static const float sun_offset[4] = {0.5f, 0.25f, -0.25f, -0.75f};
	static const int sun_sprite[4] = {0x0f, 0x0e, 0x0d, 0x0b};
	int j;

	r_numflares = 0;
	if (!Cvar_VariableValue ("r_lightflares") ||
		Cvar_VariableValue ("r_sfx_pause") || cl_paused->value)
		return;
	AngleVectors (cl.refdef.viewangles, viewforward, NULL, NULL);

	for (i=0 ; i<cl_numlightflares; i++)
	{
		flare = &cl_lightflares[i];
		if (flare->type == FLARE_SUN)
		{
			/* The retail data already contains a direction; it is not normalized. */
			VectorCopy (flare->pos, dir);
			VectorMA (cl.refdef.vieworg, 9999, dir, target);
			dot = DotProduct (dir, viewforward);
			if (dot < -0.2f)
				continue;
			coverage = V_SunVisibility (target);
			distance = 9999;
		}
		else
		{
			VectorCopy (flare->pos, target);
			VectorSubtract (target, cl.refdef.vieworg, dir);
			distance = VectorNormalize (dir);
			if (distance < 0.1f)
				distance = 0.1f;
			if (distance > 2000)
			{
				flare->lightstyle = 1;
				continue;
			}

			in_pvs = V_InPVS (cl.refdef.vieworg, target);
			if (r_numflares < MAX_VISIBLE_FLARES && in_pvs)
			{
				fl = &r_flares[r_numflares++];
				memset (fl, 0, sizeof(*fl));
				VectorCopy (target, fl->origin);
				VectorSet (fl->color, 1, 1, 1);
				fl->intensity = 64;
			}
			dot = DotProduct (dir, viewforward);
			if (dot < 0.1f || !in_pvs)
			{
				flare->lightstyle = 1;
				continue;
			}
			visible = V_FlareVisible (target, dir);
		}

		if (flare->type == FLARE_SUN)
		{
			vec3_t near, lens;
			static const float sun_alpha[4] = {0.3f, 0.2f, 0.1f, 2.0f};
			static const float sun_size[4] = {0.2f, 0.2f, 0.4f, 1.0f};
			static const vec3_t sun_color[4] = {
				{0.3f, 1, 0.6f}, {0.6f, 0.8f, 1},
				{0.8f, 1, 0.5f}, {1, 1, 1}
			};

			alpha = 1.2f - dot;
			if (alpha > 1) alpha = 1;
			alpha *= coverage;
			size = (2 - dot) * 0.5f * flare->size * 10;
			VectorMA (cl.refdef.vieworg, 640, dir, target);
			VectorMA (cl.refdef.vieworg, 320, viewforward, near);
			VectorSubtract (target, near, lens);
			for (j = 0; j < 4; j++)
			{
				vec3_t pos, color;
				VectorMA (near, sun_offset[j], lens, pos);
				VectorCopy (sun_color[j], color);
				re.AddSFX (sun_sprite[j], SFX_FLAG_LIGHTFLARE | SFX_FLAG_NO_FLIP,
					pos, color, 0, 0.01f, alpha * sun_alpha[j],
					alpha * sun_alpha[j], 0, size * sun_size[j],
					size * sun_size[j], size * sun_size[j], size * sun_size[j]);
			}

			if (dot < 0.3f)
			{
				float edge = (0.3f - dot) * (10.0f/3.0f);
				size *= edge * edge * alpha * 5 + 1;
			}
			lifetime = 0.01f;
			end_alpha = alpha;
		}
		else
		{
			dot = DotProduct (dir, viewforward);
			alpha = distance >= 256 ? 1 : distance * (1.0f/256.0f);
			if (alpha > 0)
				alpha *= alpha;
			if (alpha > 0.8f)
				alpha = 0.8f;

			if (flare->lightstyle & 2)
				size = flare->size;
			else
			{
				size = (2 - dot) * 0.5f * flare->size;
				if (distance > 64)
					size *= distance * (1.0f/384.0f);
			}

			fading = !visible;
			if (fading)
			{
				if (flare->lightstyle & 1)
					continue;
				flare->lightstyle |= 1;
				lifetime = 0.1f;
				end_alpha = 0;
			}
			else
			{
				flare->lightstyle &= ~1;
				lifetime = 0.01f;
				end_alpha = alpha;
			}
		}
		re.AddSFX (V_FlareSprite (flare->type),
			SFX_FLAG_LIGHTFLARE | SFX_FLAG_NO_FLIP,
			target, vec3_origin, 0, lifetime, alpha, end_alpha, 0,
			size, size, size, size);
	}
}
#endif

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->type = 0;
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle (int style, float r, float g, float b)
{
	lightstyle_t	*ls;

	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, "Bad light style %i", style);
	ls = &r_lightstyles[style];

	ls->white = r+g+b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25;
		r = 4*((i&7)-3.5);
		u = 4*(((i>>3)&7)-3.5);
		p = &r_particles[i];

		for (j=0 ; j<3 ; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*d +
			cl.v_right[j]*r + cl.v_up[j]*u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof(r_entities));

	for (i=0 ; i<r_numentities ; i++)
	{
		ent = &r_entities[i];

		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for (j=0 ; j<3 ; j++)
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;

#ifdef KINGPIN_ENGINE
		ent->model = cl.baseclientinfo.model[0];
		ent->skin = cl.baseclientinfo.skin[0];
#else
		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
#endif

	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof(r_dlights));

	for (i=0 ; i<r_numdlights ; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for (j=0 ; j<3 ; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		dl->color[0] = ((i%6)+1) & 1;
		dl->color[1] = (((i%6)+1) & 2)>>1;
		dl->color[2] = (((i%6)+1) & 4)>>2;
		dl->intensity = 200;
	}
}

//===================================================================

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	char		mapname[32];
	int			i;
	char		name[MAX_QPATH];
#ifdef KINGPIN_ENGINE
	vec3_t		fogcolor, fogcolor2;
	char		*skins;
	char		skin[32];
	int			j, count;
#else
	float		rotate;
	vec3_t		axis;
#endif

	if (!cl.configstrings[CS_MODELS+1][0])
		return;		// no map loaded

	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);

	// let the render dll load the map
	strcpy (mapname, cl.configstrings[CS_MODELS+1] + 5);	// skip "maps/"
	mapname[strlen(mapname)-4] = 0;		// cut off ".bsp"

	// register models, pics, and skins
	Com_Printf ("Map: %s\r", mapname); 
	SCR_UpdateScreen ();
	re.BeginRegistration (mapname);
	Com_Printf ("                                     \r");

	// precache status bar pics
	Com_Printf ("pics\r"); 
	SCR_UpdateScreen ();
	SCR_TouchPics ();
	Com_Printf ("                                     \r");

	CL_RegisterTEntModels ();

	num_cl_weaponmodels = 1;
#ifdef KINGPIN_ENGINE
	strcpy(cl_weaponmodels[0], "w_tommygun.mdx");
#else
	strcpy(cl_weaponmodels[0], "weapon.md2");
#endif

	for (i=1 ; i<MAX_MODELS && cl.configstrings[CS_MODELS+i][0] ; i++)
	{
		strcpy (name, cl.configstrings[CS_MODELS+i]);
		name[37] = 0;	// never go beyond one line
		if (name[0] != '*')
			Com_Printf ("%s\r", name); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		if (name[0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+i]+1,
					sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		} 
		else
		{
			cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS+i]);
			if (name[0] == '*')
				cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
			else
				cl.model_clip[i] = NULL;
		}
		if (name[0] != '*')
			Com_Printf ("                                     \r");
	}

	Com_Printf ("images\r", i); 
	SCR_UpdateScreen ();
	for (i=1 ; i<MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0] ; i++)
	{
		cl.image_precache[i] = re.RegisterPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	
	Com_Printf ("                                     \r");
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf ("client %i\r", i); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
		Com_Printf ("                                     \r");
	}

#ifdef KINGPIN_ENGINE
	/*
	 * kingpin.exe @0041a220 performs this as a distinct pass after all
	 * models, images and clientinfo have been registered.  Keeping that
	 * order matters to the retail refresh module's registration tables.
	 */
	for (i=1 ; i<MAX_MODELS && cl.configstrings[CS_MODELS+i][0] ; i++)
	{
		skins = cl.configstrings[CS_MODELSKINS+i];
		if (!skins[0])
			continue;

		if (!cl.model_draw[i])
			cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS+i]);
		if (!cl.model_draw[i])
			continue;

		count = strlen (skins) / 3;
		for (j=0 ; j<count ; j++, skins+=3)
		{
			strncpy (skin, skins, 3);
			skin[3] = 0;
			re.RegisterModelSkin (cl.model_draw[i], skin);
			strcat (skin, "p1");
			re.RegisterModelSkin (cl.model_draw[i], skin);
			skin[3] = 0;
			strcat (skin, "p2");
			re.RegisterModelSkin (cl.model_draw[i], skin);
		}
	}

	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male_thug/001 001 001");
#else
	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male/grunt");
#endif

	// set sky textures and speed
	Com_Printf ("sky\r", i); 
	SCR_UpdateScreen ();

#ifdef KINGPIN_ENGINE
	re.SetSky (cl.configstrings[CS_SKY]);

	// fog
	sscanf (cl.configstrings[CS_FOGVAL], "%f %f %f", &fogcolor[0], &fogcolor[1], &fogcolor[2]);
	sscanf (cl.configstrings[CS_FOGVAL2], "%f %f %f", &fogcolor2[0], &fogcolor2[1], &fogcolor2[2]);
	re.SetFog (atof (cl.configstrings[CS_DENSITY]), fogcolor,
		atof (cl.configstrings[CS_DENSITY2]), fogcolor2);
#else
	rotate = atof (cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", 
		&axis[0], &axis[1], &axis[2]);
	re.SetSky (cl.configstrings[CS_SKY], rotate, axis);
#endif
	Com_Printf ("                                     \r");

	// the renderer can now free unneeded stuff
	re.EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();

	SCR_UpdateScreen ();
	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	// start the cd track
	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
}

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	static float last_fov_x = -1.0f;
	static float last_width = -1.0f;
	static float last_height = -1.0f;
	static float last_result = 0.0f;

	if (fov_x != last_fov_x || width != last_width || height != last_height)
	{
		if (fov_x < 1.0f || fov_x > 179.0f)
			Com_Error (ERR_DROP, "Bad fov: %f", fov_x);

		x = width/tan(fov_x / 360.0f * M_PI);

		last_result = atan (height / x);
		last_result *= 360.0f / M_PI;

		last_fov_x = fov_x;
		last_width = width;
		last_height = height;
	}
	return last_result;
}

//============================================================================

// gun frame debugging functions
void V_Gun_Next_f (void)
{
	gun_frame++;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Prev_f (void)
{
	gun_frame--;
	if (gun_frame < 0)
		gun_frame = 0;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Model_f (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		gun_model = NULL;
		return;
	}
	Com_sprintf (name, sizeof(name), "models/%s/tris.md2", Cmd_Argv(1));
	gun_model = re.RegisterModel (name);
}

//============================================================================


/*
=================
SCR_DrawCrosshair
=================
*/
void SCR_DrawCrosshair (void)
{
	if (!crosshair->value)
		return;

	if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics ();
	}

	if (!crosshair_pic[0])
		return;

	re.DrawPic (scr_vrect.x + ((scr_vrect.width - crosshair_width)>>1)
	, scr_vrect.y + ((scr_vrect.height - crosshair_height)>>1), crosshair_pic);
}

/*
==================
V_RenderView

==================
*/
void V_RenderView( float stereo_separation )
{
#ifdef KINGPIN_ENGINE
	qboolean rearview;
	vec3_t rearview_angles;
#endif
	extern int entitycmpfnc( const entity_t *, const entity_t * );

	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

#ifdef KINGPIN_ENGINE
	// kingpin.exe @0041a8b0: the main menu covers the whole screen, the
	// world is not rendered behind it
	if (m_mainmenu_active)
		return;
#endif

	if (cl_timedemo->value)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();
		cl.timedemo_frames++;
	}

#ifdef KINGPIN_ENGINE
	/*
	 * kingpin.exe @0041a8b0 clears and builds the visible flare list here,
	 * before the valid-frame branch calls CL_AddEntities and applies the
	 * stereo/node-line view offsets.  V_ClearScene deliberately does not
	 * clear this list.
	 */
	V_AddFlares ();

	/*
	 * The retail executable saves the previous refdef angles before
	 * CL_AddEntities rebuilds the main view.  Its rear-view pass below uses
	 * those saved angles verbatim, including the resulting one-frame lag.
	 */
	rearview = cl_rearviewmirror->value != 0;
	if (rearview)
		VectorCopy (cl.refdef.viewangles, rearview_angles);
#endif

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && (cl.force_refdef || !cl_paused->value) )
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities ();

		if (cl_testparticles->value)
			V_TestParticles ();
		if (cl_testentities->value)
			V_TestEntities ();
		if (cl_testlights->value)
			V_TestLights ();
		if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}

		// offset vieworg appropriately if we're doing stereo separation
		if ( stereo_separation != 0 )
		{
			vec3_t tmp;

			VectorScale( cl.v_right, stereo_separation, tmp );
			VectorAdd( cl.refdef.vieworg, tmp, cl.refdef.vieworg );
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cl.refdef.vieworg[0] += 1.0/16;
		cl.refdef.vieworg[1] += 1.0/16;
		cl.refdef.vieworg[2] += 1.0/16;

		cl.refdef.x = scr_vrect.x;
		cl.refdef.y = scr_vrect.y;
		cl.refdef.width = scr_vrect.width;
		cl.refdef.height = scr_vrect.height;
		cl.refdef.fov_y = CalcFov (cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		cl.refdef.time = cl.time*0.001;

		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->value)
			r_numentities = 0;
		if (!cl_add_particles->value)
			r_numparticles = 0;
		if (!cl_add_lights->value)
			r_numdlights = 0;
		if (!cl_add_blend->value)
		{
			VectorClear (cl.refdef.blend);
		}

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;
#ifdef KINGPIN_ENGINE
		cl.refdef.num_particles2 = r_numparticles2;
		cl.refdef.particles2 = r_particles2;
		cl.refdef.num_flares = r_numflares;
		cl.refdef.flares = r_flares;
		cl.refdef.num_surflights = r_numsurflights;
		cl.refdef.surflights = r_surflights;
#endif

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;
#ifdef KINGPIN_ENGINE
		if (cl_paused->value)
			cl.refdef.rdflags |= RDF_PAUSED;
#endif

		// sort entities for better cache locality
        qsort( cl.refdef.entities, cl.refdef.num_entities, sizeof( cl.refdef.entities[0] ), (int (*)(const void *, const void *))entitycmpfnc );
	}

	re.RenderFrame (&cl.refdef);

#ifdef KINGPIN_ENGINE
	if (rearview)
	{
		/*
		 * kingpin.exe @0041b5be: draw the mirror over the completed main
		 * view.  The scene lists are reused; only the depth buffer, viewport
		 * and camera angles are changed for this second render.
		 */
		if (cl.frame.valid && (cl.force_refdef || !cl_paused->value))
		{
			re.ClearDepth ();
			cl.force_refdef = false;

			cl.refdef.x = viddef.width / 4;
			cl.refdef.y = 0;
			cl.refdef.width = viddef.width - viddef.width / 2;
			cl.refdef.height = viddef.height / 6;
			cl.refdef.viewangles[PITCH] = -rearview_angles[PITCH];
			cl.refdef.viewangles[YAW] = rearview_angles[YAW] + 180;
			cl.refdef.viewangles[ROLL] = -rearview_angles[ROLL];
			cl.refdef.fov_y = CalcFov (cl.refdef.fov_x,
				cl.refdef.width, cl.refdef.height);
			cl.refdef.time = cl.time * 0.001;
			cl.refdef.areabits = cl.frame.areabits;

			if (!cl_add_entities->value)
				r_numentities = 0;
			if (!cl_add_particles->value)
				r_numparticles = 0;
			if (!cl_add_lights->value)
				r_numdlights = 0;
			if (!cl_add_blend->value)
				VectorClear (cl.refdef.blend);

			cl.refdef.num_entities = r_numentities;
			cl.refdef.entities = r_entities;
			cl.refdef.num_particles = r_numparticles;
			cl.refdef.particles = r_particles;
			cl.refdef.num_dlights = r_numdlights;
			cl.refdef.dlights = r_dlights;
			cl.refdef.lightstyles = r_lightstyles;
			cl.refdef.num_particles2 = r_numparticles2;
			cl.refdef.particles2 = r_particles2;
			cl.refdef.num_flares = r_numflares;
			cl.refdef.flares = r_flares;
			cl.refdef.rdflags = cl.frame.playerstate.rdflags;
			if (cl_paused->value)
				cl.refdef.rdflags |= RDF_PAUSED;
		}

		re.RenderFrame (&cl.refdef);
	}
#endif

	if (cl_stats->value)
		Com_Printf ("ent:%i  lt:%i  part:%i\n", r_numentities, r_numdlights, r_numparticles);
	if ( log_stats->value && ( log_stats_file != 0 ) )
		FS_FPrintf( log_stats_file, "%i,%i,%i,",r_numentities, r_numdlights, r_numparticles);


	SCR_AddDirtyPoint (scr_vrect.x, scr_vrect.y);
	SCR_AddDirtyPoint (scr_vrect.x+scr_vrect.width-1,
		scr_vrect.y+scr_vrect.height-1);

	SCR_DrawCrosshair ();
}


/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1], (int)cl.refdef.vieworg[2], 
		(int)cl.refdef.viewangles[YAW]);
}

#ifdef KINGPIN_ENGINE
/*
=============
V_Viewpos_f (@0041bce0)
=============
*/
void V_CamPos_f (void)
{
	Com_Printf ("pos : (%i %i %i) : ang : (%i %i %i)\n",
		(int)cl.refdef.vieworg[0], (int)cl.refdef.vieworg[1],
		(int)cl.refdef.vieworg[2], (int)cl.refdef.viewangles[0],
		(int)cl.refdef.viewangles[1], (int)cl.refdef.viewangles[2]);
}
#endif

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("gun_next", V_Gun_Next_f);
	Cmd_AddCommand ("gun_prev", V_Gun_Prev_f);
	Cmd_AddCommand ("gun_model", V_Gun_Model_f);

	Cmd_AddCommand ("viewpos", V_Viewpos_f);
#ifdef KINGPIN_ENGINE
	Cmd_AddCommand ("cam_pos", V_CamPos_f);
#endif

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);
#ifdef KINGPIN_ENGINE
	cl_rearviewmirror = Cvar_Get ("cl_rearviewmirror", "0", 0);
#endif
}
