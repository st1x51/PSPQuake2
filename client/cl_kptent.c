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
// cl_kptent.c -- Kingpin temporary entities (replaces cl_tent.c)
//
// Reconstructed from kingpin.exe; addresses in the comments.
// Kingpin draws its effects with lights, sprites (re.AddSFX), sparks and
// scorch marks: the Quake 2 effect models are never registered, so the
// model pointers below stay NULL as they do in kingpin.exe.

#include "client.h"


typedef enum
{
	ex_free, ex_explosion, ex_misc, ex_flash, ex_mflash, ex_poly, ex_poly2,
	ex_gib		// bouncing, bleeding gib model
} exptype_t;

typedef struct
{
	exptype_t	type;
	entity_t	ent;

	int			frames;
	float		light;
	vec3_t		lightcolor;
	float		start;
	int			baseframe;

	// ex_gib
	vec3_t		velocity;
	vec3_t		avelocity;

	// scorch mark left on the surroundings when frame burnframe is reached
	int			burnframe;
	float		burnradius;
	float		burnintensity;

	int			lasttime;
} explosion_t;

#define	MAX_EXPLOSIONS	48
explosion_t	cl_explosions[MAX_EXPLOSIONS];


typedef struct
{
	int		entity;
	struct model_s	*model;
	int		endtime;
	vec3_t	offset;
	vec3_t	start, end;
} beam_t;

#define	MAX_BEAMS	32
beam_t		cl_beams[MAX_BEAMS];


typedef struct
{
	entity_t	ent;
	int			endtime;
} laser_t;

#define	MAX_LASERS	32
laser_t		cl_lasers[MAX_LASERS];


// one frame scorch marks and art burns, time is the frame they were made in
#define	MAX_SURFMARKS	32
surflight_t	cl_surfmarks[MAX_SURFMARKS];


struct sfx_s	*cl_sfx_ric1;
struct sfx_s	*cl_sfx_ric2;
struct sfx_s	*cl_sfx_ric3;
struct sfx_s	*cl_sfx_arc;
struct sfx_s	*cl_sfx_shock;
struct sfx_s	*cl_sfx_explosion1;
struct sfx_s	*cl_sfx_watrexp;
struct sfx_s	*cl_sfx_explosion2;
struct sfx_s	*cl_sfx_fall1;
struct sfx_s	*cl_sfx_fall2;
struct sfx_s	*cl_sfx_gibs;

// per surface material (see SURF_*): landing and four footsteps
#define	NUM_STEP_SURFACES	9
struct sfx_s	*cl_sfx_land[NUM_STEP_SURFACES];
struct sfx_s	*cl_sfx_footsteps[NUM_STEP_SURFACES][4];

// never registered by Kingpin
struct model_s	*cl_mod_explode;
struct model_s	*cl_mod_smoke;
struct model_s	*cl_mod_flash;
struct model_s	*cl_mod_parasite_segment;
struct model_s	*cl_mod_grapple_cable;
struct model_s	*cl_mod_parasite_tip;
struct model_s	*cl_mod_explo4;
struct model_s	*cl_mod_bfg_explo;
struct model_s	*cl_mod_powerscreen;

#define	NUM_GIB_MODELS	5
struct model_s	*cl_mod_gibs[NUM_GIB_MODELS];

/*
=================
CL_RegisterTEntSounds (@004163c0)
=================
*/
void CL_RegisterTEntSounds (void)
{
	int		i, j;
	char	name[MAX_QPATH];
	static char *surfaces[NUM_STEP_SURFACES] = {
		"step pavement", "step rug", "step gravel", "step metal heavy",
		"step metal light", "step tin", "step marble", "step wood", "step water"
	};

	cl_sfx_ric1 = S_RegisterSound ("weapons/ric1.wav");
	cl_sfx_ric2 = S_RegisterSound ("weapons/ric2.wav");
	cl_sfx_ric3 = S_RegisterSound ("weapons/ric3.wav");
	cl_sfx_arc = S_RegisterSound ("world/arc.wav");
	cl_sfx_shock = S_RegisterSound ("world/shock.wav");
	cl_sfx_explosion1 = S_RegisterSound ("world/explosion1.wav");
	cl_sfx_watrexp = S_RegisterSound ("world/explosion1.wav");
	cl_sfx_explosion2 = S_RegisterSound ("world/explosion2.wav");
	cl_sfx_fall2 = S_RegisterSound ("actors/player/male/fall2.wav");
	cl_sfx_fall1 = S_RegisterSound ("actors/player/male/fall1.wav");
	cl_sfx_gibs = S_RegisterSound ("actors/player/bodyfalls/jibs.wav");

	for (i=0 ; i<NUM_STEP_SURFACES ; i++)
	{
		Com_sprintf (name, sizeof(name), "actors/player/%s/land1.wav", surfaces[i]);
		cl_sfx_land[i] = S_RegisterSound (name);
		for (j=0 ; j<4 ; j++)
		{
			Com_sprintf (name, sizeof(name), "actors/player/%s/step%i.wav", surfaces[i], j+1);
			cl_sfx_footsteps[i][j] = S_RegisterSound (name);
		}
	}
}

/*
=================
CL_RegisterTEntModels (@00416530)
=================
*/
void CL_RegisterTEntModels (void)
{
	cl_mod_gibs[0] = re.RegisterModel ("models/props/gibs/gib1.mdx");
	cl_mod_gibs[1] = re.RegisterModel ("models/props/gibs/gib2.mdx");
	cl_mod_gibs[2] = re.RegisterModel ("models/props/gibs/gib3.mdx");
	cl_mod_gibs[3] = re.RegisterModel ("models/props/gibs/gib4.mdx");
	cl_mod_gibs[4] = re.RegisterModel ("models/props/gibs/gib5.mdx");
}

/*
=================
CL_ClearTEnts (@00416590)
=================
*/
void CL_ClearTEnts (void)
{
	memset (cl_beams, 0, sizeof(cl_beams));
	memset (cl_explosions, 0, sizeof(cl_explosions));
	memset (cl_lasers, 0, sizeof(cl_lasers));
}

/*
=================
CL_AllocSurfMark (@004165c0)
=================
*/
surflight_t *CL_AllocSurfMark (void)
{
	int		i;

	for (i=0 ; i<MAX_SURFMARKS ; i++)
	{
		if (cl_surfmarks[i].reserved != cl.time)
		{
			cl_surfmarks[i].reserved = cl.time;
			return &cl_surfmarks[i];
		}
	}
	return NULL;
}

/*
=================
CL_AllocExplosion (@004165f0)
=================
*/
explosion_t *CL_AllocExplosion (void)
{
	int		i;
	int		time;
	int		index;

	for (i=0 ; i<MAX_EXPLOSIONS ; i++)
	{
		if (cl_explosions[i].type == ex_free)
		{
			memset (&cl_explosions[i], 0, sizeof (cl_explosions[i]));
			return &cl_explosions[i];
		}
	}
// find the oldest explosion
	time = cl.time;
	index = 0;

	for (i=0 ; i<MAX_EXPLOSIONS ; i++)
		if (cl_explosions[i].start < time)
		{
			time = cl_explosions[i].start;
			index = i;
		}
	memset (&cl_explosions[index], 0, sizeof (cl_explosions[index]));
	cl_explosions[index].burnframe = -1;
	return &cl_explosions[index];
}

/*
=================
CL_SmokeAndFlash

Quake 2 muzzle flash effect, kept for cl_fx.c.
Only used by the Quake 2 muzzle flash code paths.
=================
*/
void CL_SmokeAndFlash (vec3_t origin)
{
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_misc;
	ex->frames = 4;
	ex->ent.flags = RF_TRANSLUCENT;
	ex->start = cl.frame.servertime - 100;
	ex->ent.model = cl_mod_smoke;

	ex = CL_AllocExplosion ();
	VectorCopy (origin, ex->ent.origin);
	ex->type = ex_flash;
	ex->ent.flags = RF_FULLBRIGHT;
	ex->frames = 2;
	ex->start = cl.frame.servertime - 100;
	ex->ent.model = cl_mod_flash;
}

/*
=================
CL_ParseBeam (@004166a0)
=================
*/
int CL_ParseBeam (struct model_s *model)
{
	int		ent;
	vec3_t	start, end;
	beam_t	*b;
	int		i;

	ent = MSG_ReadShort (&net_message);

	MSG_ReadPos (&net_message, start);
	MSG_ReadPos (&net_message, end);

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorClear (b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");
	return ent;
}

/*
=================
CL_ParseBeam2 (@00416790)
=================
*/
int CL_ParseBeam2 (struct model_s *model)
{
	int		ent;
	vec3_t	start, end, offset;
	beam_t	*b;
	int		i;

	ent = MSG_ReadShort (&net_message);

	MSG_ReadPos (&net_message, start);
	MSG_ReadPos (&net_message, end);
	MSG_ReadPos (&net_message, offset);

// override any beam with the same entity
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
		if (b->entity == ent)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}

// find a free beam
	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
		{
			b->entity = ent;
			b->model = model;
			b->endtime = cl.time + 200;
			VectorCopy (start, b->start);
			VectorCopy (end, b->end);
			VectorCopy (offset, b->offset);
			return ent;
		}
	}
	Com_Printf ("beam list overflow!\n");
	return ent;
}

/*
=================
CL_ParseLaser (@004168e0)
=================
*/
void CL_ParseLaser (int colors)
{
	vec3_t	start;
	vec3_t	end;
	laser_t	*l;
	int		i;

	MSG_ReadPos (&net_message, start);
	MSG_ReadPos (&net_message, end);

	for (i=0, l=cl_lasers ; i< MAX_LASERS ; i++, l++)
	{
		if (l->endtime < cl.time)
		{
			l->ent.flags = RF_TRANSLUCENT | RF_BEAM;
			VectorCopy (start, l->ent.origin);
			VectorCopy (end, l->ent.oldorigin);
			l->ent.alpha = 0.30;
			l->ent.skinnum = (colors >> ((rand() % 4)*8)) & 0xff;
			l->ent.model = NULL;
			l->ent.frame = 4;
			l->endtime = cl.time + 100;
			return;
		}
	}
}

/*
=================
CL_AddSurfMark
=================
*/
static void CL_AddSurfMark (vec3_t pos, float radius, float intensity, float r, float g, float b)
{
	surflight_t	*m;

	m = CL_AllocSurfMark ();
	if (!m)
		return;
	VectorCopy (pos, m->origin);
	m->radius = radius;
	m->intensity = intensity;
	m->color[0] = r;
	m->color[1] = g;
	m->color[2] = b;
}

/*
=================
CL_Explosion1Sprites

Fire and smoke of TE_EXPLOSION1 style explosions (count scales the size)
=================
*/
static void CL_ExplosionSprites (vec3_t pos, vec3_t dir, int count, float velscale,
	float firew, float fireh, float smokeflags, qboolean concussion)
{
	vec3_t	vel;
	float	m, f;

	VectorClear (vel);
	m = count/30 + 1.0;
	VectorMA (vel, velscale * m, dir, vel);

	if (concussion)
		f = frand()*0.5 + 1.5;
	else
		f = frand() + 1.5;

	re.AddSFX (SFX_SPRITE_EXPLOSION2, 0, pos, vel, 0,
		frand()*0.25 + 0.7, 1, 1, 0,
		f*firew, f*60*m, f*fireh, f*72*m);
	re.AddSFX (SFX_SPRITE_EXPLOSION1, (int)smokeflags, pos, vel, 0,
		frand()*0.25 + 1.0, 1, 1, 0,
		f*40, f*60*m, f*48, f*72*m);

	if (concussion)
	{
		VectorMA (pos, 48, dir, pos);
		re.AddSFX (SFX_SPRITE_SMOKE_ANIM1, 0x400, pos, vec3_origin, 0,
			frand()*0.25 + 2.0, 1, 1, -2.9,
			f*40, f*60*m, f*48, f*72*m);
	}
}

/*
=================
CL_ExplosionScorch

The explosion_t shared by the TE_EXPLOSION1 family: light and a scorch
mark on frame 4, no model
=================
*/
static void CL_ExplosionScorch (vec3_t pos)
{
	explosion_t	*ex;

	ex = CL_AllocExplosion ();
	VectorCopy (pos, ex->ent.origin);
	ex->type = ex_poly;
	ex->light = 300;
	ex->lightcolor[0] = 1.0;
	ex->lightcolor[1] = 0.8;
	ex->lightcolor[2] = 0.5;
	ex->ent.model = NULL;
	ex->frames = 6;
	ex->burnframe = 4;
	ex->start = cl.frame.servertime - 100;
	ex->burnradius = 64;
	ex->burnintensity = 0.4;
}

/*
=================
CL_ParseTEnt (@004169b0)
=================
*/
static byte splash_color[] = {0x00, 0xe0, 0xb0, 0x50, 0xd0, 0xe0, 0xe8};

void CL_ParseTEnt (void)
{
	int		type;
	vec3_t	pos, pos2, dir;
	vec3_t	vel, angles, v;
	explosion_t	*ex;
	cspark_t	*s;
	int		cnt;
	int		color;
	int		r;
	int		ent;
	int		i, j, k;
	int		size, xsize, ysize, height, range;
	float	scale, alpha, f, len, width, lt, g, m, t;
	trace_t	tr;
	vec3_t	start, end;

	type = MSG_ReadByte (&net_message);

	switch (type)
	{
	case TE_GUNSHOT:			// bullet hitting wall
	case TE_GUNSHOT_VISIBLE:
	case TE_SPARKS:
	case TE_BULLET_SPARKS:
	case TE_IMPACT:
	case TE_IMPACT_CONCUSSION:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);

		if (type == TE_GUNSHOT || type == TE_GUNSHOT_VISIBLE
			|| type == TE_IMPACT || type == TE_IMPACT_CONCUSSION)
		{
			alpha = (type == TE_IMPACT) ? 0.05 : 0.2;

			VectorMA (pos, 4, dir, start);		// unused, as in kingpin.exe
			VectorScale (dir, 12, vel);
			if (vel[2] <= 1)
				vel[2] += 4.0f * frand();

			if (type == TE_GUNSHOT_VISIBLE)
			{
				cnt = rand()%6 + 3;
				for (i=0 ; i<cnt ; i++)
				{
					for (j=0 ; j<3 ; j++)
						v[j] = (frand()*300 + 200) * dir[j] * 0.5;
					CL_AllocSpark (pos, v, (frand() + 1) * 0.2, 0.05);
				}
			}

			re.AddSFX (SFX_SPRITE_SMOKE_SMALL, 0, pos, vel, 0,
				0.4, alpha, 0, 0.2,
				1, 96, 1, 96);

			VectorScale (dir, 8, vel);
			if (vel[2] <= 1)
				vel[2] += 12.0f * frand();
			re.AddSFX (SFX_SPRITE_SMOKE_SMALL, 0, pos, vel, 0,
				0.5, alpha*2, 0, 0.2,
				1, 80, 1, 80);
		}

		if (type == TE_SPARKS || type == TE_IMPACT_CONCUSSION)
			break;

		// impact sound
		r = rand() & 15;
		if (r == 1)
			S_StartSound (pos, 0, 0, cl_sfx_ric1, 1, ATTN_NORM, 0);
		else if (r == 2)
			S_StartSound (pos, 0, 0, cl_sfx_ric2, 1, ATTN_NORM, 0);
		else if (r == 3)
			S_StartSound (pos, 0, 0, cl_sfx_ric3, 1, ATTN_NORM, 0);
		break;

	case TE_BLOOD:
		MSG_ReadPos (&net_message, pos);
		cnt = MSG_ReadByte (&net_message);
		VectorClear (dir);			// never initialised in kingpin.exe, unused for type 7
		CL_BloodEffect (pos, dir, 7, cnt);
		break;

	case TE_BLASTER:			// blaster hitting wall
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_BlasterParticles (pos, dir);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->ent.angles[0] = acos(dir[2])/M_PI*180;
		// kingpin.exe stores the yaw in angles[2]
		ex->ent.angles[2] = atan2(dir[1], dir[0])/M_PI*180;

		ex->type = ex_misc;
		ex->ent.flags = RF_FULLBRIGHT|RF_TRANSLUCENT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 150;
		ex->lightcolor[0] = 1;
		ex->lightcolor[1] = 1;
		ex->ent.model = cl_mod_explode;
		ex->frames = 4;
		break;

	case TE_RAILTRAIL:			// railgun effect
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);
		MSG_ReadDir (&net_message, dir);

		VectorSubtract (pos2, pos, v);
		VectorMA (pos, 0.5, v, start);
		len = VectorNormalize (v);
		width = 4;
		if (len < 512)
		{
			width = len * (1.0/128);
			if (len < 32)
				break;
		}
		vectoangles (v, angles);
		for (i=0 ; i<8 ; i++)
		{
			angles[2] += 22.5;
			re.AddSFX (SFX_SPRITE_SNIPER1C, 0xc6, start, angles, 0,
				0.1, 0.3, 0, 0,
				len, len, width, width*2);
		}
		CL_RailSparks (pos, pos2, dir);
		break;

	case TE_SHOTGUN:			// nothing but the message
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		break;

	case TE_EXPLOSION1:
	case TE_EXPLOSION1B:
	case TE_EXPLOSION1C:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		cnt = MSG_ReadByte (&net_message);
		r = MSG_ReadByte (&net_message);
		if (cnt > 30)
			cnt = 30;
		if (r && r <= 100)
			scale = r * 0.1;
		else
			scale = 1.0;

		VectorMA (pos, 5, dir, pos);

		if (type == TE_EXPLOSION1 || type == TE_EXPLOSION1C)
		{
			f = scale * 0.2;
			g = dir[2] > 0 ? dir[2] : 0;
			CL_ExplosionDebris (pos, dir, (int)cl_fireworks->value * cnt,
				(g + 1) * 0.5 * f * 400, 1.5, 1.0,
				((dir[2] + 1) * 0.35 + 0.3) * f * 800, 0);
			if (type == TE_EXPLOSION1C)
				break;
		}

		CL_ExplosionScorch (pos);

		VectorMA (pos, 40, dir, pos);
		CL_ExplosionSprites (pos, dir, cnt, (1.5 - dir[2]) * 12.8, 40, 48, 0, false);
		S_StartSound (pos, 0, 0, cl_sfx_explosion1, 1, ATTN_NORM, 0);
		break;

	case TE_CONCUSSION_EXPLOSION:
	case TE_CONCUSSION_EXPLOSION_WATER:
	case TE_CONCUSSION_EXPLOSION_BIG:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		cnt = MSG_ReadByte (&net_message);
		if (cnt > 30)
			cnt = 30;

		// fewer, slower debris close to the view
		f = VectorDistance (pos, cl.refdef.vieworg);
		if (f < 1024)
			f = f * (1.0/1024) * 0.8 + 0.2;
		else
			f = 1;
		if (type == TE_CONCUSSION_EXPLOSION_BIG)
			f += f;
		g = dir[2] > 0 ? dir[2] : 0;
		CL_ExplosionDebris (pos, dir, (int)((int)cl_fireworks->value * (cnt/3) * f),
			(g + 1) * 0.5 * (f*400), 1.5, 1.0,
			((dir[2] + 1) * 0.35 + 0.3) * (f*800), 0);

		CL_ExplosionScorch (pos);

		VectorMA (pos, 16, dir, pos);
		if (VectorDistance (cl.refdef.vieworg, pos) > 128)
		{	// flash if the explosion is in sight
			tr = CL_PMTrace (cl.refdef.vieworg, vec3_origin, vec3_origin, pos);
			if (tr.fraction == 1 && !tr.startsolid)
				re.AddSFX (SFX_SPRITE_CORONA_BLUE, 0x40c, pos, vec3_origin, 0,
					0.3, 1, 0, 0.05,
					200, 500, 200, 500);
		}

		CL_ExplosionSprites (pos, dir, cnt, (fabs(dir[2]) + 1.5) * 19.2, 20, 24, 0x400, true);

		if (type == TE_CONCUSSION_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else if (type == TE_CONCUSSION_EXPLOSION_BIG)
			S_StartSound (pos, 0, 0, cl_sfx_explosion2, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl_sfx_explosion1, 1, ATTN_NORM, 0);
		break;

	case TE_EXPLOSION2:
	case TE_GRENADE_EXPLOSION:
	case TE_GRENADE_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.5;
		ex->lightcolor[2] = 0.5;
		ex->ent.model = cl_mod_explo4;
		ex->frames = 19;
		ex->baseframe = 30;
		ex->ent.angles[1] = rand() % 360;
		CL_ExplosionParticles (pos);
		if (type == TE_GRENADE_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		else
			S_StartSound (pos, 0, 0, cl_sfx_explosion1, 1, ATTN_NORM, 0);
		break;

	case TE_ROCKET_EXPLOSION:
	case TE_ROCKET_EXPLOSION_WATER:
		MSG_ReadPos (&net_message, pos);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 100;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.6;
		ex->frames = 4;
		ex->burnframe = 50;
		ex->burnradius = 96;
		ex->burnintensity = 0.5;
		ex->ent.angles[1] = rand() % 360;
		if (type == TE_ROCKET_EXPLOSION_WATER)
			S_StartSound (pos, 0, 0, cl_sfx_watrexp, 1, ATTN_NORM, 0);
		break;

	case TE_METAL_SPARKS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_MetalSparks (pos, dir, rand()%6 + 1);
		if (!(rand() & 8))
			S_StartSound (pos, 0, 0, cl_sfx_ric1, 1, ATTN_NORM, 0);
		break;

	case TE_SPLASH:			// bullet hitting water
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		r = MSG_ReadByte (&net_message);
		if (r == SPLASH_SPARKS || r == SPLASH_SPARKS_P)
		{	// electric sparks
			for (i=0 ; i<cnt ; i++)
			{
				v[0] = (rand()&0xff) - 0x80;
				v[1] = (rand()&0xff) - 0x80;
				v[2] = dir[2];
				s = CL_AllocSpark (pos, v, frand()*0.64, 0.1);
				if (s)
					s->light = 32;
				v[0] = (rand()&0xff) - 0x80;
				v[1] = (rand()&0xff) - 0x80;
				CL_AllocSpark (pos, v, 0.2, 0.05);
			}
			if (r != SPLASH_SPARKS_P)
				S_StartSound (pos, 0, 0, cl_sfx_arc, 1, ATTN_STATIC, 0);
		}
		else if (r == SPLASH_BLOOD)
			CL_BloodEffect (pos, dir, 7, cnt);
		else
		{
			if (r < 7)
				color = splash_color[r];
			else
				color = 0;
			CL_ParticleEffect (pos, dir, color, cnt);
		}
		break;

	case TE_BUBBLETRAIL:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);
		CL_BubbleTrail (pos, pos2);
		break;

	case TE_SCREEN_SPARKS:
	case TE_SHIELD_SPARKS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		if (type == TE_SCREEN_SPARKS)
			CL_ParticleEffect (pos, dir, 0xd0, 40);
		else
			CL_ParticleEffect (pos, dir, 0xb0, 40);
		break;

	case TE_LASER_SPARKS:
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		color = MSG_ReadByte (&net_message);
		CL_ParticleEffect2 (pos, dir, color, cnt);
		break;

	case TE_PARASITE_ATTACK:
	case TE_MEDIC_CABLE_ATTACK:
		ent = CL_ParseBeam (cl_mod_parasite_segment);
		break;

	case TE_BFG_EXPLOSION:
		MSG_ReadPos (&net_message, pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT|RF_TRANSLUCENT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 0.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.0;
		ex->ent.model = cl_mod_bfg_explo;
		ex->ent.alpha = 0.30;
		ex->frames = 4;
		break;

	case TE_BFG_BIGEXPLOSION:
		MSG_ReadPos (&net_message, pos);
		CL_BFGExplosionParticles (pos);
		break;

	case TE_BOSSTPORT:			// boss teleporting to station
		MSG_ReadPos (&net_message, pos);
		CL_BigTeleportParticles (pos);
		S_StartSound (pos, 0, 0, S_RegisterSound ("misc/bigtele.wav"), 1, ATTN_NONE, 0);
		break;

	case TE_BFG_LASER:
		CL_ParseLaser (0xd0d1d2d3);
		break;

	case TE_GRAPPLE_CABLE:
		ent = CL_ParseBeam2 (cl_mod_grapple_cable);
		break;

	case TE_WELDING_SPARKS:
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		color = MSG_ReadByte (&net_message);
		CL_ParticleEffect2 (pos, dir, color, cnt);

		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_flash;
		// note that this is now a flare, not a cross
		ex->ent.flags = RF_BEAM;
		ex->start = cl.frame.servertime - 0.1;
		ex->light = 100 + (rand()%75);
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 1.0;
		ex->lightcolor[2] = 0.3;
		ex->ent.model = cl_mod_flash;
		ex->frames = 2;
		break;

	case TE_GREENBLOOD:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_ParticleEffect2 (pos, dir, 0xdf, 30);
		break;

	case TE_BLUEHYPERBLASTER:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_BlasterParticles (pos, dir);
		break;

	case TE_PLASMA_EXPLOSION:
		MSG_ReadPos (&net_message, pos);
		ex = CL_AllocExplosion ();
		VectorCopy (pos, ex->ent.origin);
		ex->type = ex_poly;
		ex->ent.flags = RF_FULLBRIGHT;
		ex->start = cl.frame.servertime - 100;
		ex->light = 350;
		ex->lightcolor[0] = 1.0;
		ex->lightcolor[1] = 0.5;
		ex->lightcolor[2] = 0.5;
		ex->ent.angles[1] = rand() % 360;
		ex->ent.model = cl_mod_explo4;
		if (frand() < 0.5)
			ex->baseframe = 15;
		ex->frames = 15;
		CL_ExplosionParticles (pos);
		break;

	case TE_TUNNEL_SPARKS:
		cnt = MSG_ReadByte (&net_message);
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		color = MSG_ReadByte (&net_message);
		CL_ParticleEffect3 (pos, dir, color, cnt);
		break;

	case TE_SFXFIRE:
	case TE_SFXFIRET:
		MSG_ReadPos (&net_message, pos);
		size = MSG_ReadByte (&net_message);
		r = MSG_ReadByte (&net_message);
		alpha = r ? r * 0.1 : 0.5;

		/* PSP rand() is 31-bit. Use the engine's normalized RNG so fire
		 * sizes stay bounded and integer multiplication cannot overflow. */
		vel[0] = vel[1] = 0;
		vel[2] = 10 + (2.0f * frand() - 1) * 4;
		f = (size>>2) + size;
		g = (2.0f * frand() - 0.5) * 15 + f;		// end height
		t = (2.0f * frand() - 0.5) * 10 + f;		// end width
		re.AddSFX (type == TE_SFXFIRET ? SFX_SPRITE_FIREWALL : SFX_SPRITE_FIRE, 0x20, pos, vel, 0,
			0.7, alpha, 0, 0.3,
			size, t, (size>>1) + size, g);
		break;

	case TE_SFXSMOKE:
		MSG_ReadPos (&net_message, pos);
		size = MSG_ReadByte (&net_message);
		r = MSG_ReadByte (&net_message);
		alpha = r ? r * 0.1 : 0.4;

		f = size >> 3;
		vel[0] = (2.0f * frand() - 1) * f;
		vel[1] = (2.0f * frand() - 1) * f;
		vel[2] = (size >> 2) * (2.0f * frand() + 1);
		k = size - size/5;
		g = k*6 + (2.0f * frand() - 1) * 16;
		t = size*4 + (2.0f * frand() - 1) * 16;
		re.AddSFX (SFX_SPRITE_SMOKE_SMALL, 0, pos, vel, 0,
			4.5, alpha, 0, 1.8,
			size, t, k, g);
		break;

	case TE_SFXFIREGO:
		MSG_ReadPos (&net_message, pos);
		lt = MSG_ReadByte (&net_message) * 0.1;
		VectorClear (vel);
		i = rand()%10 + 20;
		g = (float)(rand()%25) + (float)(i*2);
		t = (float)(rand()%20) + (float)((i>>2) + i);
		re.AddSFX (SFX_SPRITE_EXPLOSION1 + (rand() & 1), 0, pos, vel, 0,
			lt, 1.0, 0, lt*0.25,
			(float)i, t, (float)((i>>1) + i), g);
		break;

	case TE_SNOW:
	case TE_RAIN:
		MSG_ReadPos (&net_message, pos);
		cnt = MSG_ReadShort (&net_message);
		xsize = MSG_ReadShort (&net_message);
		ysize = MSG_ReadShort (&net_message);
		height = MSG_ReadShort (&net_message);
		if (type == TE_RAIN)
			MSG_ReadShort (&net_message);		// unused

		for (i=0 ; i<cnt ; i++)
		{
			VectorCopy (pos, start);
			start[0] += rand()%xsize - (xsize>>1);
			start[1] += rand()%ysize - (ysize>>1);
			start[2] += (height>>1) + 1;
			VectorCopy (start, end);
			end[2] -= 8192;
			tr = CL_PMTrace (start, vec3_origin, vec3_origin, end);
			if (tr.fraction >= 1.0)
				continue;

			VectorCopy (start, v);
			range = abs ((int)(start[2] - tr.endpos[2]));
			if (type == TE_SNOW)
			{
				if (!range)
					range = 1;
				v[2] -= rand()%range;
				CL_SnowParticle (v, start[2], tr.endpos[2]);
			}
			else
			{
				if (!range)
					v[2] -= 1.0;
				else
					v[2] -= rand()%range + 1;
				CL_RainParticle (v, start[2], tr.endpos[2]);
			}
		}
		break;

	case TE_BLOOD_DRIP:
	case TE_BLOOD_SPLAT:
		break;		// no data, no effect

	case TE_BLOOD_POOL:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		CL_BloodEffect (pos, dir, 5, 1);
		break;

	case TE_SURFACE_SPRITE_ENTITY:
		r = MSG_ReadByte (&net_message);
		f = MSG_ReadByte (&net_message);		// width
		g = MSG_ReadByte (&net_message);		// height
		MSG_ReadPos (&net_message, pos);
		angles[0] = MSG_ReadAngle (&net_message);
		angles[1] = MSG_ReadAngle (&net_message);
		angles[2] = 0;
		AngleVectors (angles, dir, NULL, NULL);

		if (r == SFX_SPRITE_SURF_BULLET1)
			re.AddSFX (r, SFX_FLAG_SURFACE_EFFECT, pos, dir, 0,
				1000000, 1.0, 0, 9,
				f, f, g, g);
		else if (r == SFX_SPRITE_SURF_BLOOD_POOL || r == SFX_SPRITE_SURF_BLOOD1)
		{
			if (cl_parental_lock->value && !cl_parental_override->value)
				break;
			re.AddSFX (r, SFX_FLAG_SURFACE_EFFECT, pos, dir, 0,
				cl_bloodtime->value * 40, 1.0, 0, 9,
				f, f, g, g);
		}
		else if (r == SFX_SPRITE_SURF_RIPPLE)
		{
			t = 2.0f * frand() - 1;
			re.AddSFX (r, SFX_FLAG_SURFACE_EFFECT|SFX_FLAG_NO_FLIP, pos, dir, 0,
				0.6, 1.0, 0, 0.2,
				0, t*f*0.5 + f, 0, t*g*0.5 + g);
		}
		else
			re.AddSFX (r, SFX_FLAG_SURFACE_EFFECT, pos, dir, 0,
				10, 1.0, 0, 9,
				f, f, g, g);
		break;

	case TE_BURN_TO_A_CRISP:
		MSG_ReadPos (&net_message, pos);
		CL_AddSurfMark (pos, 96, -1, 1, 1, 1);
		break;

	case TE_SFXSTEAM:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		cnt = MSG_ReadByte (&net_message);
		k = MSG_ReadByte (&net_message);
		f = MSG_ReadByte (&net_message);		// growth per step
		g = MSG_ReadByte (&net_message);		// extra height
		r = MSG_ReadByte (&net_message);
		alpha = r ? r * 0.1 : 0.2;

		for (i=0 ; i<cnt ; i++)
		{
			for (j=0 ; j<k ; j++)
			{
				m = i * f;
				v[0] = dir[0] * 25 + ((rand()&3) - 2);
				v[1] = dir[1] * 25 + ((rand()&3) - 2);
				v[2] = dir[2] * 25 + ((rand()&3) - 2) + i;
				t = ((rand()&8) - 4) + g + m;		// end height
				len = ((rand()&8) - 4) + g + m;		// start height
				width = ((rand()&8) - 4) + m;		// end width
				scale = ((rand()&8) - 4) + m;		// start width
				re.AddSFX (SFX_SPRITE_CORONA, 0, pos, v, 0,
					(rand()&7) * 0.25 + 1.0, alpha, alpha, 0.2,
					scale, width, len, t);
			}
			VectorMA (pos, 8 * (i * 0.1), dir, pos);
		}
		break;

	case TE_SFXSMOKE2:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, vel);
		size = MSG_ReadByte (&net_message);
		lt = MSG_ReadByte (&net_message);
		height = MSG_ReadByte (&net_message);
		r = MSG_ReadByte (&net_message);
		alpha = r ? r * 0.1 : 0.4;

		if (!vel[0] && !vel[1] && !vel[2])
		{
			f = size >> 3;
			vel[0] = (2.0f * frand() - 1) * f;
			vel[1] = (2.0f * frand() - 1) * f;
			vel[2] = 2.0f * frand() * height;
		}
		k = size - size/5;
		g = k*6 + (2.0f * frand() - 1) * 16;
		t = size*4 + (2.0f * frand() - 1) * 16;
		re.AddSFX (SFX_SPRITE_SMOKE_SMALL, 0, pos, vel, 0,
			lt, alpha, 0, 1.8,
			size, t, k, g);
		break;

	case TE_ART_BURN:
		MSG_ReadPos (&net_message, pos);
		f = MSG_ReadShort (&net_message);
		v[0] = MSG_ReadByte (&net_message) * (1.0/255);
		v[1] = MSG_ReadByte (&net_message) * (1.0/255);
		v[2] = MSG_ReadByte (&net_message) * (1.0/255);
		g = (MSG_ReadByte (&net_message) - 127) * (1.0/127);
		CL_AddSurfMark (pos, f, g, v[0], v[1], v[2]);
		break;

	case TE_FOG_BRUSH:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadPos (&net_message, pos2);
		// kingpin.exe calls refexport slot 32 here, which its ref_gl never
		// fills in; calling it would jump to stack garbage
		if (0)
			re.Unknown32 (pos, (int)pos2[0]);
		break;

	case TE_GIBS:
		MSG_ReadPos (&net_message, pos);
		MSG_ReadDir (&net_message, dir);
		cnt = MSG_ReadByte (&net_message);
		f = MSG_ReadByte (&net_message);		// speed along dir
		r = MSG_ReadByte (&net_message);		// spread
		g = MSG_ReadByte (&net_message);		// random speed
		if (cl_parental_lock->value && !cl_parental_override->value)
			break;

		S_StartSound (pos, 0, 0, cl_sfx_gibs, 1, ATTN_NORM, 0);
		for (i=0 ; i<cnt ; i++)
		{
			ex = CL_AllocExplosion ();
			VectorCopy (pos, ex->ent.origin);
			if (r)
			{
				k = r*2;
				ex->ent.origin[0] += rand()%k - r;
				ex->ent.origin[1] += rand()%k - r;
				t = (r == 16) * frand() * 16;
				ex->ent.origin[2] += rand()%k - r + t;
			}
			ex->velocity[0] = crand() * g;
			ex->velocity[1] = crand() * g;
			ex->velocity[2] = frand() * g * 3 + 120;
			VectorMA (ex->velocity, f, dir, ex->velocity);
			if (VectorLength (ex->velocity) > 500)
			{
				VectorNormalize (ex->velocity);
				VectorScale (ex->velocity, 500, ex->velocity);
			}
			VectorCopy (ex->ent.origin, ex->ent.oldorigin);
			ex->avelocity[0] = frand() * 600;
			ex->avelocity[1] = frand() * 600;
			ex->avelocity[2] = frand() * 600;
			ex->type = ex_gib;
			ex->baseframe = -1;
			ex->ent.flags2 = RF2_NOSHADOW;
			ex->ent.flags = RF_MINLIGHT;
			ex->start = cl.time;
			ex->ent.model = cl_mod_gibs[rand() % NUM_GIB_MODELS];
			ex->frames = rand()%50 + 150;
			ex->lasttime = cl.time;
		}
		break;

	default:	// TE_SUN_FLARE, TE_GUN_FLARE, TE_LIGHT_FLARE, TE_BARMACHINGUN too
		Com_Error (ERR_DROP, "CL_ParseTEnt: bad type");
	}
}

/*
=================
CL_AddBeams (@004195b0)
=================
*/
void CL_AddBeams (void)
{
	int			i,j;
	beam_t		*b;
	vec3_t		dist, org;
	float		d;
	entity_t	ent;
	float		yaw, pitch;
	float		forward;

	for (i=0, b=cl_beams ; i< MAX_BEAMS ; i++, b++)
	{
		if (!b->model || b->endtime < cl.time)
			continue;

		// if coming from the player, update the start position
		if (b->entity == cl.playernum+1)	// entity 0 is the world
		{
			VectorCopy (cl.refdef.vieworg, b->start);
			b->start[2] -= 22;	// adjust for view height
		}
		VectorAdd (b->start, b->offset, org);

		// calculate pitch and yaw
		VectorSubtract (b->end, org, dist);

		if (dist[1] == 0 && dist[0] == 0)
		{
			yaw = 0;
			if (dist[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(dist[1], dist[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (dist[0]*dist[0] + dist[1]*dist[1]);
			pitch = (int) (atan2(dist[2], forward) * -180.0 / M_PI);
			if (pitch < 0)
				pitch += 360.0;
		}

		// add new entities for the beams
		d = VectorNormalize(dist);

		memset (&ent, 0, sizeof(ent));
		while (d > 0)
		{
			VectorCopy (org, ent.origin);
			ent.model = b->model;
			ent.angles[0] = pitch;
			ent.angles[1] = yaw;
			ent.angles[2] = rand()%360;
			V_AddEntity (&ent);

			for (j=0 ; j<3 ; j++)
				org[j] += dist[j]*30;
			d -= 30;
		}
	}
}

/*
=================
CL_AddExplosions (@00419800)
=================
*/
void CL_AddExplosions (void)
{
	entity_t	*ent;
	int			i, j;
	explosion_t	*ex;
	float		frac, dt, backoff, vol;
	int			f;
	trace_t		tr;
	static float	gib_soundtime;
	static vec3_t	gib_mins = {-4, -4, -4};
	static vec3_t	gib_maxs = {4, 4, 4};

	for (i=0, ex=cl_explosions ; i< MAX_EXPLOSIONS ; i++, ex++)
	{
		if (ex->type == ex_free)
			continue;
		frac = (cl.time - ex->start)*0.01;
		f = (int)floor(frac);

		ent = &ex->ent;

		switch (ex->type)
		{
		case ex_mflash:
			if (f >= ex->frames-1)
				ex->type = ex_free;
			break;
		case ex_misc:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0 - frac/(ex->frames-1);
			break;
		case ex_flash:
			if (f >= 1)
			{
				ex->type = ex_free;
				break;
			}
			ent->alpha = 1.0;
			break;
		case ex_poly:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (16.0 - (float)f)/16.0;

			if (f < 10)
			{
				ent->skinnum = (f>>1);
				if (ent->skinnum < 0)
					ent->skinnum = 0;
			}
			else
			{
				ent->flags |= RF_TRANSLUCENT;
				if (f < 13)
					ent->skinnum = 5;
				else
					ent->skinnum = 6;
			}
			break;
		case ex_poly2:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}

			ent->alpha = (5.0 - (float)f)/5.0;
			ent->skinnum = 0;
			ent->flags |= RF_TRANSLUCENT;
			break;
		case ex_gib:
			if (f >= ex->frames-1)
			{
				ex->type = ex_free;
				break;
			}
			f = 0;
			dt = (cl.time - ex->lasttime) * 0.001;
			if (!ex->velocity[0] && !ex->velocity[1] && !ex->velocity[2])
				break;		// at rest

			VectorMA (ent->origin, dt, ex->velocity, ent->origin);
			ex->velocity[2] -= dt * 400;
			tr = CL_PMTrace (ent->oldorigin, gib_mins, gib_maxs, ent->origin);
			if (tr.fraction >= 1)
			{
				if (tr.startsolid || tr.allsolid)
				{
					ex->type = ex_free;
					break;
				}
			}
			else
			{	// bounce
				backoff = DotProduct (tr.plane.normal, ex->velocity) * 1.4;

				// kingpin.exe compares the last impact sound time with
				// cls.frametime, not a clock
				if (gib_soundtime < cls.frametime - 0.5 || cls.frametime < gib_soundtime)
				{
					vol = VectorLength (ex->velocity) * 0.0125;
					if (vol > 1)
						vol = 1;
					if (vol < 0.2)
						vol = 0;
					if (vol > 0.2)
					{
						S_StartSound (tr.endpos, 0, 0, cl_sfx_gibs, vol, ATTN_NORM, 0.85);
						gib_soundtime = cls.frametime;
					}
				}

				for (j=0 ; j<3 ; j++)
				{
					ex->velocity[j] -= backoff * tr.plane.normal[j];
					if (ex->velocity[j] > -0.1 && ex->velocity[j] < 0.1)
						ex->velocity[j] = 0;
				}
				VectorAdd (tr.plane.normal, tr.endpos, ent->origin);

				if (tr.plane.normal[2] > 0.7 && fabs(ex->velocity[2]) < 120)
				{	// landed
					VectorClear (ex->velocity);
					VectorClear (ex->avelocity);
					VectorClear (ent->angles);
				}
				CL_BloodEffect (ent->oldorigin, vec3_origin, 7, 10);
			}
			CL_BloodEffect (ent->oldorigin, vec3_origin, 7, 1);
			VectorMA (ent->angles, dt, ex->avelocity, ent->angles);
			break;
		default:
			break;
		}

		if (ex->type == ex_free)
			continue;

		if (ex->light)
		{
			frac = 1.0 - ((cl.time - ex->start)*0.01) / ex->frames;
			if (frac > 0)
				V_AddLight (ent->origin, ex->light*frac,
					ex->lightcolor[0], ex->lightcolor[1], ex->lightcolor[2]);

			if (ex->burnframe > 0 && ex->burnframe == f)
			{
				V_AddSurfLight (ent->origin, ex->burnradius, -ex->burnintensity, 1, 1, 1);
				ex->burnframe = -1;
			}
		}

		if (!ent->model)
			continue;

		VectorCopy (ent->origin, ent->oldorigin);

		if (f < 0)
			f = 0;
		ent->frame = ex->baseframe + f + 1;
		ent->oldframe = ex->baseframe + f;
		if (ent->oldframe < 0)
			ent->oldframe = ent->frame;
		ex->lasttime = cl.time;
		ent->backlerp = 1.0 - cl.lerpfrac;

		V_AddEntity (ent);
	}
}

/*
=================
CL_AddLasers (@00419d90)
=================
*/
void CL_AddLasers (void)
{
	laser_t		*l;
	int			i;

	for (i=0, l=cl_lasers ; i< MAX_LASERS ; i++, l++)
	{
		if (l->endtime >= cl.time)
			V_AddEntity (&l->ent);
	}
}

/*
=================
CL_AddSurfMarks (@00419dd0)
=================
*/
void CL_AddSurfMarks (void)
{
	int			i;
	surflight_t	*m;

	for (i=0, m=cl_surfmarks ; i<MAX_SURFMARKS ; i++, m++)
	{
		if (m->reserved > 0)
		{
			V_AddSurfLight (m->origin, m->radius, m->intensity, m->color[0], m->color[1], m->color[2]);
			m->reserved = 0;
		}
	}
}

/*
=================
CL_AddTEnts (@00419e10)
=================
*/
void CL_AddTEnts (void)
{
	CL_AddBeams ();
	CL_AddExplosions ();
	CL_AddLasers ();
	CL_AddSurfMarks ();
}

