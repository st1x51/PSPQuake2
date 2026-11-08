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
// cl_kpfx.c -- Kingpin entity effects parsing and management
//
// Reconstructed from kingpin.exe

#include "client.h"

void CL_LogoutEffect (vec3_t org, int type);
void CL_ItemRespawnParticles (vec3_t org);

extern void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up);

static vec3_t avelocities [NUMVERTEXNORMALS];

extern	struct model_s	*cl_mod_smoke;
extern	struct model_s	*cl_mod_flash;

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct
{
	int		length;
	float	value[3];
	float	map[MAX_QPATH];
} clightstyle_t;

clightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
int			lastofs;

/*
================
CL_ClearLightStyles
================
*/
void CL_ClearLightStyles (void)
{
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	lastofs = -1;
}

/*
================
CL_RunLightStyles
================
*/
void CL_RunLightStyles (void)
{
	int		ofs;
	int		i;
	clightstyle_t	*ls;

	ofs = cl.time / 100;
	if (ofs == lastofs)
		return;
	lastofs = ofs;

	for (i=0,ls=cl_lightstyle ; i<MAX_LIGHTSTYLES ; i++, ls++)
	{
		if (!ls->length)
		{
			ls->value[0] = ls->value[1] = ls->value[2] = 1.0;
			continue;
		}
		if (ls->length == 1)
			ls->value[0] = ls->value[1] = ls->value[2] = ls->map[0];
		else
			ls->value[0] = ls->value[1] = ls->value[2] = ls->map[ofs%ls->length];
	}
}


void CL_SetLightstyle (int i)
{
	char	*s;
	int		j, k;

	s = cl.configstrings[i+CS_LIGHTS];

	j = strlen (s);
	if (j >= MAX_QPATH)
		Com_Error (ERR_DROP, "svc_lightstyle length=%i", j);

	cl_lightstyle[i].length = j;

	for (k=0 ; k<j ; k++)
		cl_lightstyle[i].map[k] = (float)(s[k]-'a')/(float)('m'-'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles (void)
{
	int		i;
	clightstyle_t	*ls;

	for (i=0,ls=cl_lightstyle ; i<MAX_LIGHTSTYLES ; i++, ls++)
		V_AddLightStyle (i, ls->value[0], ls->value[1], ls->value[2]);
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

cdlight_t		cl_dlights[MAX_DLIGHTS];

/*
================
CL_ClearDlights
================
*/
void CL_ClearDlights (void)
{
	memset (cl_dlights, 0, sizeof(cl_dlights));
}

/*
===============
CL_AllocDlight

===============
*/
cdlight_t *CL_AllocDlight (int key)
{
	int		i;
	cdlight_t	*dl;

// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
void CL_NewDlight (int key, float x, float y, float z, float radius, float time)
{
	cdlight_t	*dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
}


/*
===============
CL_RunDLights

===============
*/
void CL_RunDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius)
			continue;
		
		if (dl->die < cl.time)
		{
			dl->radius = 0;
			return;
		}
		dl->radius -= cls.frametime*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}

/*
==============
CL_ParseMuzzleFlash
==============
*/
void CL_ParseMuzzleFlash (void)
{
	vec3_t forward, right;
	cdlight_t *dl;
	centity_t *player;
	int ent, weapon, silenced;
	float volume;
	char soundname[64];
	const char *sound = NULL;
	float attenuation = ATTN_NORM;
	float delay = 0;

	ent = MSG_ReadShort (&net_message);
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash: bad entity");

	weapon = MSG_ReadByte (&net_message);
	silenced = weapon & MZ_SILENCED;
	weapon &= ~MZ_SILENCED;
	player = &cl_entities[ent];

	dl = CL_AllocDlight (ent);
	VectorCopy (player->current.origin, dl->origin);
	AngleVectors (player->current.angles, forward, right, NULL);
	VectorMA (dl->origin, 18, forward, dl->origin);
	VectorMA (dl->origin, 16, right, dl->origin);
	dl->radius = (silenced ? 300 : 500) + (rand() & 31);
	dl->minlight = 32;
	dl->die = cl.time;
	volume = silenced ? 0.2f : 1.0f;
	dl->color[0] = 1.0f - (float)rand() * (0.1f / RAND_MAX);
	dl->color[1] = 0.4f + (float)rand() * (0.2f / RAND_MAX);
	dl->color[2] = 0;

	// kingpin.exe @00407720. The wire values are shared with Quake II,
	// but the sound paths and several light colors/radii are different.
	switch (weapon)
	{
	case MZ_BLASTER:
		VectorSet (dl->color, 1.0f, 1.0f, 0.0f);
		sound = "weapons/blastf1a.wav";
		break;
	case MZ_MACHINEGUN:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		sound = soundname;
		break;
	case MZ_SHOTGUN:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("weapons/shotgun/shotgf1b.wav"), volume, ATTN_NORM, 0);
		S_StartSound (NULL, ent, CHAN_AUTO, S_RegisterSound ("weapons/shotgun/shotgr1b.wav"), volume, ATTN_NORM, 0.1f);
		break;
	case MZ_CHAINGUN1:
		VectorSet (dl->color, 1.0f, 0.25f, 0.0f);
		dl->radius = 200 + (rand() & 31);
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		sound = soundname;
		break;
	case MZ_CHAINGUN2:
		VectorSet (dl->color, 1.0f, 0.5f, 0.0f);
		dl->radius = 225 + (rand() & 31);
		dl->die = cl.time + 0.1f;
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		sound = soundname;
		delay = 0.05f;
		break;
	case MZ_CHAINGUN3:
		VectorSet (dl->color, 1.0f, 1.0f, 0.0f);
		dl->radius = 250 + (rand() & 31);
		dl->die = cl.time + 0.1f;
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0);
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound (soundname), volume, ATTN_NORM, 0.033f);
		Com_sprintf (soundname, sizeof(soundname), "weapons/machinegun/machgf%ib.wav", rand()%5+1);
		sound = soundname;
		delay = 0.066f;
		break;
	case MZ_RAILGUN:
		VectorSet (dl->color, 0.5f, 0.5f, 1.0f);
		sound = "weapons/railgf1a.wav";
		break;
	case MZ_ROCKET:
		VectorSet (dl->color, 1.0f, 0.5f, 0.2f);
		sound = "weapons/rocket_launcher/rl_fire.wav";
		break;
	case MZ_GRENADE:
		VectorSet (dl->color, 1.0f, 0.5f, 0.0f);
		sound = "weapons/grenade_launcher/gl_fire.wav";
		break;
	case MZ_LOGIN:
		sound = "misc/tele1.wav";
		volume = 1.0f;
		break;
	case MZ_LOGOUT:
		VectorSet (dl->color, 1.0f, 0.0f, 0.0f);
		dl->die = cl.time + 1;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("misc/tele1.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (player->current.origin, weapon);
		break;
	case MZ_RESPAWN:
		VectorSet (dl->color, 1.0f, 1.0f, 0.0f);
		dl->die = cl.time + 1;
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ("misc/tele1.wav"), 1, ATTN_NORM, 0);
		CL_LogoutEffect (player->current.origin, weapon);
		break;
	case MZ_SSHOTGUN:
		sound = "weapons/shotgun/sshotf1b.wav";
		break;
	case MZ_HYPERBLASTER:
		VectorSet (dl->color, 1.0f, 1.0f, 0.0f);
		sound = "weapons/hyprbf1a.wav";
		break;
	case MZ_BLUEHYPERBLASTER:
		VectorSet (dl->color, 0.0f, 0.0f, 1.0f);
		sound = "weapons/hyprbf1a.wav";
		break;
	case MZ_PISTOL:
	case MZ_PISTOL_DAMAGE_MOD:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		Com_sprintf (soundname, sizeof(soundname),
			weapon == MZ_PISTOL ? "weapons/pistol/pistola%i.wav" : "weapons/pistol/pistolf%i.wav",
			rand()%4+1);
		sound = soundname;
		break;
	case MZ_SPISTOL:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		sound = "weapons/pistol/silencer.wav";
		attenuation = 3.0f;
		break;
	case MZ_BLACKJACK_SWING:
	case MZ_CROWBAR_SWING:
		dl->radius = 1;
		sound = "weapons/melee/swing.wav";
		break;
	case MZ_BLACKJACK_HIT:
	case MZ_CROWBAR_HIT:
		dl->radius = 1;
		break;
	case MZ_BARMACHINEGUN:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		sound = "weapons/hmg/hmg.wav";
		break;
	case MZ_BARMACHINEGUN2:
		VectorSet (dl->color, 1.0f, 0.6f, 0.23f);
		break;
	default:
		break;
	}
	if (sound)
		S_StartSound (NULL, ent, CHAN_WEAPON, S_RegisterSound ((char *)sound), volume, attenuation, delay);
	if (!Com_ServerState ())
		dl->radius *= 0.3f;
}

/*
==============
CL_ParseMuzzleFlash2
==============
*/
#ifdef __psp__
#define MZ2_GUNNER_MACHINEGUN_1_X (30.1f * 1.15f)
#define MZ2_GUNNER_MACHINEGUN_1_Y (3.9f * 1.15f)
#define MZ2_GUNNER_MACHINEGUN_1_Z (19.6f * 1.15f)
#endif
void CL_ParseMuzzleFlash2 (void)
{
	int ent, flash;
	vec3_t forward, right;
	cdlight_t *dl;
	centity_t *cent;

	ent = MSG_ReadShort (&net_message);
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash2: bad entity");
	flash = MSG_ReadByte (&net_message);
	cent = &cl_entities[ent];
	AngleVectors (cent->current.angles, forward, right, NULL);
	dl = CL_AllocDlight (ent);
#ifdef __psp__
    /*
     * Kingpin gamesrc emits only MZ2_GUNNER_MACHINEGUN_1.
     * Keep this local to avoid the monster_flash_offset/m_flash.c dependency.
     */
	VectorMA (cent->current.origin, MZ2_GUNNER_MACHINEGUN_1_X, forward, dl->origin);
	VectorMA (dl->origin, MZ2_GUNNER_MACHINEGUN_1_Y, right, dl->origin);
	dl->origin[2] += MZ2_GUNNER_MACHINEGUN_1_Z;
#else
	VectorMA (cent->current.origin, monster_flash_offset[flash][0], forward, dl->origin);
	VectorMA (dl->origin, monster_flash_offset[flash][1], right, dl->origin);
	dl->origin[2] += monster_flash_offset[flash][2];
#endif
	dl->radius = 200 + (rand() & 31);
	dl->minlight = 32;
	dl->die = cl.time;
	dl->color[0] = 1.0f - (float)rand() * (0.1f / RAND_MAX);
	dl->color[1] = 0.4f + (float)rand() * (0.2f / RAND_MAX);
	dl->color[2] = 0;
	// Kingpin @00407ea0 does not play a sound or spawn smoke here.
}

/*
==============
CL_ParseMuzzleFlash3

Bullet impact sounds on entities (kingpin.exe @00408000)
==============
*/
void CL_ParseMuzzleFlash3 (void)
{
	int		ent;
	int		type;
	char	name[128];

	ent = MSG_ReadShort (&net_message);
	if (ent < 1 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_ParseMuzzleFlash3: bad entity");

	type = MSG_ReadByte (&net_message);
	if (type != MZ3_LEADHIT_METAL)
		return;

	switch (rand() % 4)
	{
	case 0:
		strcpy (name, "weapons/bullethit_tin.wav");
		break;
	case 1:
		strcpy (name, "weapons/bullethit_tin2.wav");
		break;
	case 2:
		strcpy (name, "weapons/bullethit_tin3.wav");
		break;
	default:
		Com_sprintf (name, sizeof(name), "weapons/ric%d.wav", rand() % 3 + 1);
		break;
	}
	S_StartSound (NULL, ent, 0, S_RegisterSound (name), 1, ATTN_NORM, 0.1);
}

/*
===============
CL_AddDLights

===============
*/
void CL_AddDLights (void)
{
	int			i;
	cdlight_t	*dl;

	dl = cl_dlights;

//=====
//PGM
#if 1
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->radius)
			continue;
		V_AddLight (dl->origin, dl->radius,
			dl->color[0], dl->color[1], dl->color[2]);
	}
#else
	if(vidref_val == VIDREF_GL)
	{
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (!dl->radius)
				continue;
			V_AddLight (dl->origin, dl->radius,
				dl->color[0], dl->color[1], dl->color[2]);
		}
	}
	else
	{
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (!dl->radius)
				continue;

			// negative light in software. only black allowed
			if ((dl->color[0] < 0) || (dl->color[1] < 0) || (dl->color[2] < 0))
			{
				dl->radius = -(dl->radius);
				dl->color[0] = 1;
				dl->color[1] = 1;
				dl->color[2] = 1;
			}
			V_AddLight (dl->origin, dl->radius,
				dl->color[0], dl->color[1], dl->color[2]);
		}
	}
#endif
//PGM
//=====
}



/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

cparticle_t	*active_particles, *free_particles;

cparticle_t	particles[MAX_PARTICLES];
int			cl_numparticles = MAX_PARTICLES;

cparticle2_t	*active_particles2, *free_particles2;

cparticle2_t	particles2[MAX_PARTICLES2];
int				cl_numparticles2 = MAX_PARTICLES2;

#define	MAX_SPARKS		256

cspark_t	cl_sparks[MAX_SPARKS];

cvar_t		*cl_fireworks;
cvar_t		*cl_bloodtime;


/*
===============
CL_ClearParticles (@00408170)
===============
*/
void CL_ClearParticles (void)
{
	int		i;

	memset (cl_sparks, 0, sizeof(cl_sparks));

	free_particles = &particles[0];
	active_particles = NULL;
	for (i=0 ;i<cl_numparticles ; i++)
	{
		particles[i].next = &particles[i+1];
		particles[i].type = 0;
	}
	particles[cl_numparticles-1].next = NULL;

	active_particles2 = NULL;
	free_particles2 = &particles2[0];
	for (i=0 ;i<cl_numparticles2 ; i++)
	{
		particles2[i].next = &particles2[i+1];
		particles2[i].type = 0;
	}
	particles2[cl_numparticles2-1].next = NULL;
}

/*
===============
CL_AllocParticle
===============
*/
static cparticle_t *CL_AllocParticle (void)
{
	cparticle_t	*p;

	if (!free_particles)
		return NULL;
	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;
	return p;
}


/*
===============
CL_AllocParticle2
===============
*/
static cparticle2_t *CL_AllocParticle2 (void)
{
	cparticle2_t	*p;

	if (!free_particles2)
		return NULL;
	p = free_particles2;
	free_particles2 = p->next;
	p->next = active_particles2;
	active_particles2 = p;
	return p;
}


/*
===============
CL_AllocSpark (@00408500)
===============
*/
cspark_t *CL_AllocSpark (vec3_t org, vec3_t vel, float lifetime, float ptlife)
{
	int			i;
	cspark_t	*s;

	for (i=0, s=cl_sparks ; i<MAX_SPARKS ; i++, s++)
	{
		if (s->inuse)
			continue;

		VectorCopy (vel, s->vel);
		VectorNormalize2 (s->vel, s->dir);
		VectorAdd (s->dir, org, s->origin);
		VectorCopy (s->origin, s->lastorigin);
		s->lifetime = lifetime;
		s->ptlife = ptlife;
		s->age = 0;
		s->starttime = cl.time;
		s->inuse = 1;
		s->start_alpha = 1.0;
		s->end_alpha = 0;
		MakeNormalVectors (s->dir, s->right, s->up);
		s->spiral = 0;
		s->spiral_grow = 0;
		s->spiral_angle = 0;
		s->spiral_speed = 1.0;
		s->spiral_flags = 1;
		s->gravity = 400;
		s->type = 50;
		s->density = 1.0;
		s->bounce = 1.9;
		s->light = 0;
		s->smoke_scale = 0;
		return s;
	}
	return NULL;
}


/*
===============
CL_ParticleEffect (@004081f0)

Wall impact puffs
===============
*/
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<count ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = color + (rand()&7);

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j];
			p->vel[j] = crand()*10;
		}
		p->vel[2] *= 0.5;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand()*0.3);
	}
}

/*
===============
CL_ParticleEffect2 (@004082e0)
===============
*/
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = color;

		d = rand()&7;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = crand()*20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand()*0.3);
	}
}

/*
===============
CL_ParticleEffect3 (@004083f0)

Same as CL_ParticleEffect2, but the particles rise
===============
*/
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<count ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = color;

		d = rand()&7;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = crand()*20;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand()*0.3);
	}
}


/*
===============
CL_BloodEffect (@004093a0)

type 7: blood sprites flying off (their size grows with the distance to
the view). Other types: particles; type 5 leaves a pool particle that
never fades. Nothing is drawn for parental locked clients.
===============
*/
void CL_BloodEffect (vec3_t org, vec3_t dir, int type, int count)
{
	int			i, j;
	int			color;
	cparticle_t	*p;
	vec3_t		vel;
	float		scale, size, d;

	if (cl_parental_lock->value && !cl_parental_override->value)
		return;

	color = 0;
	if (type > 4)
	{
		if (type <= 7)
			color = 0x80;
	}

	if (type == 7)
	{
		scale = 1.0;
		d = VectorDistance (org, cl.refdef.vieworg);
		if (d > 128)
			scale = (d - 128) * (1.0/128) + 1.0;
		if (scale > 10)
			scale = 10;

		for (i=0 ; i<count ; i++)
		{
			size = rand()%15;
			vel[0] = rand()%150 - 75;
			vel[1] = rand()%150 - 75;
			vel[2] = rand()%(count*5) + 100;
			if (vel[2] > 100)
				vel[2] = 100;
			VectorScale (vel, 0.5, vel);
			if (count == 1)
			{
				VectorScale (vel, 0.02, vel);
				vel[2] = -20;
			}
			size = (size*0.2 + 0.5) * scale;

			re.AddSFX (SFX_SPRITE_BLOOD1 + rand()%9, count != 1, org, vel, 0,
				1.0, 1.0, 0, -1,
				size, size, size, size);
		}
		return;
	}

	p = NULL;
	for (i=0 ; i<count ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->type = type;
		p->color = color + (rand()&7);

		d = rand()&9;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = crand()*20;
		}
		p->vel[2] *= 0.5;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.5 + frand()*0.3);
	}

	if (type == 5 && p)
	{	// the last one stays as a pool
		VectorCopy (org, p->org);
		VectorClear (p->vel);
		VectorClear (p->accel);
		p->time = cl.time + 1000000;
	}
}


/*
===============
CL_TeleporterParticles
===============
*/
void CL_TeleporterParticles (entity_state_t *ent)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<8 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		p->color = 0xdb;

		for (j=0 ; j<2 ; j++)
		{
			p->org[j] = ent->origin[j] - 16 + (rand()&31);
			p->vel[j] = crand()*14;
		}

		p->org[2] = ent->origin[2] - 8 + (rand()&7);
		p->vel[2] = 80 + (rand()&7);

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.5;
	}
}


/*
===============
CL_LogoutEffect

===============
*/
void CL_LogoutEffect (vec3_t org, int type)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<500 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		if (type == MZ_LOGIN)
			p->color = 0xd0 + (rand()&7);	// green
		else if (type == MZ_LOGOUT)
			p->color = 0x40 + (rand()&7);	// red
		else
			p->color = 0xe0 + (rand()&7);	// yellow

		p->org[0] = org[0] - 16 + frand()*32;
		p->org[1] = org[1] - 16 + frand()*32;
		p->org[2] = org[2] - 24 + frand()*56;

		for (j=0 ; j<3 ; j++)
			p->vel[j] = crand()*20;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (1.0 + frand()*0.3);
	}
}


/*
===============
CL_ItemRespawnParticles

===============
*/
void CL_ItemRespawnParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<64 ; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;

		p->color = 0xd4 + (rand()&3);	// green

		p->org[0] = org[0] + crand()*8;
		p->org[1] = org[1] + crand()*8;
		p->org[2] = org[2] + crand()*8;

		for (j=0 ; j<3 ; j++)
			p->vel[j] = crand()*8;

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY*0.2;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (1.0 + frand()*0.3);
	}
}


/*
===============
CL_ExplosionParticles (@00409aa0)
===============
*/
void CL_ExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<256 ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = 0xe0 + (rand()&7);

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%384)-192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.8 / (0.5 + frand()*0.3);
	}
}


/*
===============
CL_BigTeleportParticles (@00409ba0)
===============
*/
void CL_BigTeleportParticles (vec3_t org)
{
	int			i;
	cparticle_t	*p;
	float		angle, dist;
	static int colortable[4] = {2*8,13*8,21*8,18*8};

	for (i=0 ; i<4096 ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;

		p->color = colortable[rand()&3];

		angle = M_PI*2*(rand()&1023)/1023.0;
		dist = rand()&31;
		p->org[0] = org[0] + cos(angle)*dist;
		p->vel[0] = cos(angle)*(70+(rand()&63));
		p->accel[0] = -cos(angle)*100;

		p->org[1] = org[1] + sin(angle)*dist;
		p->vel[1] = sin(angle)*(70+(rand()&63));
		p->accel[1] = -sin(angle)*100;

		p->org[2] = org[2] + 8 + (rand()%90);
		p->vel[2] = -100 + (rand()&31);
		p->accel[2] = PARTICLE_GRAVITY*4;
		p->alpha = 1.0;

		p->alphavel = -0.3 / (0.5 + frand()*0.3);
	}
}


/*
===============
CL_BlasterParticles (@00409d10)
===============
*/
void CL_BlasterParticles (vec3_t org, vec3_t dir)
{
	int			i, j;
	cparticle_t	*p;
	float		d;

	for (i=0 ; i<40 ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = 0xe0 + (rand()&7);

		d = rand()&15;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()&7)-4) + d*dir[j];
			p->vel[j] = dir[j] * 30 + crand()*40;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -1.0 / (0.5 + frand()*0.3);
	}
}


/*
===============
CL_BlasterTrail

===============
*/
void CL_BlasterTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3+frand()*0.2);
		p->color = 0xe0;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand()*5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_QuadTrail

===============
*/
void CL_QuadTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8+frand()*0.2);
		p->color = 115;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand()*16;
			p->vel[j] = crand()*5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_FlagTrail

===============
*/
void CL_FlagTrail (vec3_t start, vec3_t end, float color)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.8+frand()*0.2);
		p->color = color;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand()*16;
			p->vel[j] = crand()*5;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}

/*
===============
CL_DiminishingTrail

kingpin.exe @0040a130 (only used for EF_GIB): a blood sprite every 16
units. trailcount is only kept at 100 or more, it never decays.
===============
*/
void CL_DiminishingTrail (vec3_t start, vec3_t end, centity_t *old, int flags)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	VectorScale (vec, 16, vec);

	while (len > 0)
	{
		len -= 16;
		CL_BloodEffect (move, vec3_origin, 7, 1);
		if (old->trailcount < 100)
			old->trailcount = 100;
		VectorAdd (move, vec, move);
	}
}

void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	float		d;

	// this rotate and negat guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}

/*
===============
CL_RocketTrail

kingpin.exe @0040a290: a smoke puff sprite trail walking back from the
rocket (not drawn within 64 units of the view) that thins out and grows
with distance, and a glow sprite on the rocket when the view can see it.
===============
*/
void CL_RocketTrail (vec3_t start, vec3_t end, centity_t *old)
{
	vec3_t		pos, vec, vel;
	float		len, step, life, a, size;
	trace_t		tr;

	if (VectorDistance (end, cl.refdef.vieworg) < 64)
		return;

	VectorCopy (end, pos);
	VectorSubtract (start, end, vec);
	len = VectorNormalize (vec);
	step = 8;
	life = 1.0;
	VectorScale (vec, 8, vec);

	while (len > 0)
	{
		life -= step * (1.0/750);
		if (life < 0)
			break;
		a = ((frand()*0.3 - 0.15) + 1.0) * life;
		step = (1.0 - a) * 24 + 8;
		vel[0] = frand()*10 - 5;
		vel[1] = frand()*10 - 5;
		vel[2] = frand()*10 - 5;
		size = (a + 0.5) * 32;
		re.AddSFX (SFX_SPRITE_EXPLOSION1, 0x500, pos, vel, 0,
			a, 1.0 - a, 1.0, 0,
			step, size, step, size);
		len -= step;
		VectorAdd (pos, vec, pos);
	}

	tr = CL_PMTrace (cl.refdef.vieworg, vec3_origin, vec3_origin, end);
	if (!tr.startsolid && !tr.allsolid && tr.fraction == 1.0)
		re.AddSFX (SFX_SPRITE_CORONA_BLUE, 0x8c, end, vec3_origin, 0,
			0.01, 1.0, 1.0, 0,
			50, 50, 50, 50);
}

/*
===============
CL_RailTrail

===============
*/
void CL_RailTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			j;
	cparticle_t	*p;
	float		dec;
	vec3_t		right, up;
	int			i;
	float		d, c, s;
	vec3_t		dir;
	byte		clr = 0x74;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	MakeNormalVectors (vec, right, up);

	for (i=0 ; i<len ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		
		p->time = cl.time;
		VectorClear (p->accel);

		d = i * 0.1;
		c = cos(d);
		s = sin(d);

		VectorScale (right, c, dir);
		VectorMA (dir, s, up, dir);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.2);
		p->color = clr + (rand()&7);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + dir[j]*3;
			p->vel[j] = dir[j]*6;
		}

		VectorAdd (move, vec, move);
	}

	dec = 0.75;
	VectorScale (vec, dec, vec);
	VectorCopy (start, move);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->time = cl.time;
		VectorClear (p->accel);

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.6+frand()*0.2);
		p->color = 0x0 + rand()&15;

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand()*3;
			p->vel[j] = crand()*3;
			p->accel[j] = 0;
		}

		VectorAdd (move, vec, move);
	}
}

// RAFAEL
/*
===============
CL_IonripperTrail
===============
*/
void CL_IonripperTrail (vec3_t start, vec3_t ent)
{
	vec3_t	move;
	vec3_t	vec;
	float	len;
	int		j;
	cparticle_t *p;
	int		dec;
	int     left = 0;

	VectorCopy (start, move);
	VectorSubtract (ent, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);

		p->time = cl.time;
		p->alpha = 0.5;
		p->alphavel = -1.0 / (0.3 + frand() * 0.2);
		p->color = 0xe4 + (rand()&3);

		for (j=0; j<3; j++)
		{
			p->org[j] = move[j];
			p->accel[j] = 0;
		}
		if (left)
		{
			left = 0;
			p->vel[0] = 10;
		}
		else 
		{
			left = 1;
			p->vel[0] = -10;
		}

		p->vel[1] = 0;
		p->vel[2] = 0;

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_BubbleTrail (@0040a6b0)
===============
*/
void CL_BubbleTrail (vec3_t start, vec3_t end)
{
	vec3_t		move;
	vec3_t		vec;
	float		len;
	int			i, j;
	cparticle_t	*p;
	float		dec;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 32;
	VectorScale (vec, dec, vec);

	for (i=0 ; i<len ; i+=dec)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		VectorClear (p->accel);
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (1+frand()*0.2);
		p->color = 4 + (rand()&7);
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand()*2;
			p->vel[j] = crand()*5;
		}
		p->vel[2] += 6;

		VectorAdd (move, vec, move);
	}
}


/*
===============
CL_FlyParticles
===============
*/

#define	BEAMLENGTH			16
void CL_FlyParticles (vec3_t origin, int count)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sp, sy, cp, cy;
	vec4_t		forward;	// aligned on a 16-byte boundary
	float		dist = 64;
	float		ltime;


	if (count > NUMVERTEXNORMALS)
		count = NUMVERTEXNORMALS;

	if (!avelocities[0][0])
	{
		for (i=0 ; i<NUMVERTEXNORMALS ; i++)
		{
			avelocities[i][0] = (rand()&255) * 0.01;
			avelocities[i][1] = (rand()&255) * 0.01;
			avelocities[i][2] = (rand()&255) * 0.01;
		}
	}

	ltime = (float)cl.time / 1000.0;

#ifdef __psp__
	__asm__ volatile (
		".set		push\n"					// save assembler option
		".set		noreorder\n"			// suppress reordering
		"lv.s		S102,  0 + %0\n"		// S102 = origin[0]
		"lv.s		S112,  4 + %0\n"		// S112 = origin[1]
		"lv.s		S122,  8 + %0\n"		// S122 = origin[2]
		"mfc1		$8, %1\n"				// FPU->CPU
		"mtv		$8, S012\n"				// CPU->VFPU S012 = ltime
		"vfim.s		S013, 64.0\n"			// S013 = 64
		"vfim.s		S030, "M_ATOS(BEAMLENGTH)".0\n"		// S030 = BEAMLENGTH
		"vcst.s		S010, VFPU_2_PI\n"		// S010 = VFPU_2_PI = 2 / PI
		"vmul.s		S011, S012, S010\n"		// S011 = ltime * 2 / PI
		".set		pop\n"					// restore assembler option
		::	"m"(*origin), "f"(ltime)
		:	"$8"
	);
#endif

	for (i=0 ; i<count ; i+=2)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#ifdef __psp__
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"lv.s		S022,  0 + %1\n"		// S022 = avelocities[i][0]
			"lv.s		S023,  4 + %1\n"		// S023 = avelocities[i][1]
			"lv.s		S101,  0 + %2\n"		// S101 = bytedirs[i][0]
			"lv.s		S111,  4 + %2\n"		// S111 = bytedirs[i][1]
			"lv.s		S121,  8 + %2\n"		// S121 = bytedirs[i][2]
			"lv.s		S021, %3\n"				// S021 = i
			"vi2f.s		S021, S021, 0\n"		// S021 = float(S022)
			"vadd.s		S021, S012, S021\n"		// S021 = (ltime + i)
			"vmul.t		C021, C021, C010[X,Y,Y]\n"	// C021 = (ltime + i) * 2 / PI, avelocities[i][] * ltime * 2 / PI
			"vsin.t		C031, C021\n"			// C031 = sin(avelocities[i][] * ltime * 2 / PI)
			"vcos.p		C020, C022\n"			// C020 = cos(avelocities[i][] * ltime * 2 / PI)
			"vmul.s		S031, S031, S013\n"		// S031 = sin((ltime + i) * 2 / PI)*64
			"vmul.s		S100, S021, S020\n"		// S100 = forward[0] = cp1*cy0
			"vmul.s		S110, S021, S032\n"		// S110 = forward[1] = cp1*sy0
			"vneg.s		S120, S033\n"			// S120 = forward[2] = -sp1
			"vhdp.t		S000, C030, C100\n"		// S000 = BEAMLENGTH * forward[0] + dist * bytedirs[i][0] + origin[0]
			"vhdp.t		S001, C030, C110\n"		// S001 = BEAMLENGTH * forward[1] + dist * bytedirs[i][1] + origin[1]
			"vhdp.t		S002, C030, C120\n"		// S002 = BEAMLENGTH * forward[2] + dist * bytedirs[i][2] + origin[2]
			"sv.s		S000, 0 + %0\n"			// p->org[0] = S000
			"sv.s		S001, 4 + %0\n"			// p->org[1] = S001
			"sv.s		S002, 8 + %0\n"			// p->org[2] = S002
			".set		pop\n"					// restore assembler option
			:	"=m"(*p->org)
			:	"m"(*avelocities[i]), "m"(*bytedirs[i]), "m"(i)
		);
#else
		angle = ltime * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = ltime * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);

		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin(ltime + i)*64;

		p->org[0] = origin[0] + bytedirs[i][0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = origin[1] + bytedirs[i][1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = origin[2] + bytedirs[i][2]*dist + forward[2]*BEAMLENGTH;
#endif
		p->time = cl.time;

		VectorClear (p->vel);
		VectorClear (p->accel);

		p->color = 0;
		p->colorvel = 0;

		p->alpha = 1;
		p->alphavel = -100;
	}
}

void CL_FlyEffect (centity_t *ent, vec3_t origin)
{
	int		n;
	int		count;
	int		starttime;

	if (ent->fly_stoptime < cl.time)
	{
		starttime = cl.time;
		ent->fly_stoptime = cl.time + 60000;
	}
	else
	{
		starttime = ent->fly_stoptime - 60000;
	}

	n = cl.time - starttime;
	if (n < 20000)
		count = n * 162 / 20000.0;
	else
	{
		n = ent->fly_stoptime - cl.time;
		if (n < 20000)
			count = n * 162 / 20000.0;
		else
			count = 162;
	}

	CL_FlyParticles (origin, count);
}


/*
===============
CL_BfgParticles
===============
*/

#define	BEAMLENGTH			16
void CL_BfgParticles (entity_t *ent)
{
	int			i;
	cparticle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist = 64;
	vec3_t		v;
	float		ltime;
	
	if (!avelocities[0][0])
	{
		for (i=0 ; i<NUMVERTEXNORMALS ; i++)
		{
			avelocities[i][0] = (rand()&255) * 0.01;
			avelocities[i][1] = (rand()&255) * 0.01;
			avelocities[i][2] = (rand()&255) * 0.01;
		}
	}


	ltime = (float)cl.time / 1000.0;

#ifdef __psp__
	__asm__ volatile (
		".set		push\n"					// save assembler option
		".set		noreorder\n"			// suppress reordering
		"lv.s		S102,  0 + %0\n"		// S102 = origin[0]
		"lv.s		S112,  4 + %0\n"		// S112 = origin[1]
		"lv.s		S122,  8 + %0\n"		// S122 = origin[2]
		"mfc1		$8, %1\n"				// FPU->CPU
		"mtv		$8, S012\n"				// CPU->VFPU S012 = ltime
		"vfim.s		S013, 64.0\n"			// S013 = 64
		"vfim.s		S030, "M_ATOS(BEAMLENGTH)".0\n"		// S030 = BEAMLENGTH
		"vcst.s		S010, VFPU_2_PI\n"		// S010 = VFPU_2_PI = 2 / PI
		"vmul.s		S011, S012, S010\n"		// S011 = ltime * 2 / PI
		".set		pop\n"					// restore assembler option
		::	"m"(*ent->origin), "f"(ltime)
		:	"$8"
	);
#endif

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

#ifdef __psp__
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"lv.s		S022,  0 + %1\n"		// S022 = avelocities[i][0]
			"lv.s		S023,  4 + %1\n"		// S023 = avelocities[i][1]
			"lv.s		S101,  0 + %2\n"		// S101 = bytedirs[i][0]
			"lv.s		S111,  4 + %2\n"		// S111 = bytedirs[i][1]
			"lv.s		S121,  8 + %2\n"		// S121 = bytedirs[i][2]
			"lv.s		S021, %3\n"				// S021 = i
			"vi2f.s		S021, S021, 0\n"		// S021 = float(S022)
			"vadd.s		S021, S012, S021\n"		// S021 = (ltime + i)
			"vmul.t		C021, C021, C010[X,Y,Y]\n"	// C021 = (ltime + i) * 2 / PI, avelocities[i][] * ltime * 2 / PI
			"vsin.t		C031, C021\n"			// C031 = sin(avelocities[i][] * ltime * 2 / PI)
			"vcos.p		C020, C022\n"			// C020 = cos(avelocities[i][] * ltime * 2 / PI)
			"vmul.s		S031, S031, S013\n"		// S031 = sin((ltime + i) * 2 / PI)*64
			"vmul.s		S100, S021, S020\n"		// S100 = forward[0] = cp1*cy0
			"vmul.s		S110, S021, S032\n"		// S110 = forward[1] = cp1*sy0
			"vneg.s		S120, S033\n"			// S120 = forward[2] = -sp1
			"vhdp.t		S000, C030, C100\n"		// S000 = BEAMLENGTH * forward[0] + dist * bytedirs[i][0] + origin[0]
			"vhdp.t		S001, C030, C110\n"		// S001 = BEAMLENGTH * forward[1] + dist * bytedirs[i][1] + origin[1]
			"vhdp.t		S002, C030, C120\n"		// S002 = BEAMLENGTH * forward[2] + dist * bytedirs[i][2] + origin[2]
			"sv.s		S000, 0 + %0\n"			// p->org[0] = S000
			"sv.s		S001, 4 + %0\n"			// p->org[1] = S001
			"sv.s		S002, 8 + %0\n"			// p->org[2] = S002
			".set		pop\n"					// restore assembler option
			:	"=m"(*p->org)
			:	"m"(*avelocities[i]), "m"(*bytedirs[i]), "m"(i)
		);
#else
		angle = ltime * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = ltime * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = ltime * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		dist = sin(ltime + i)*64;
		p->org[0] = ent->origin[0] + bytedirs[i][0]*dist + forward[0]*BEAMLENGTH;
		p->org[1] = ent->origin[1] + bytedirs[i][1]*dist + forward[1]*BEAMLENGTH;
		p->org[2] = ent->origin[2] + bytedirs[i][2]*dist + forward[2]*BEAMLENGTH;
#endif

		p->time = cl.time;

		VectorClear (p->vel);
		VectorClear (p->accel);

		VectorSubtract (p->org, ent->origin, v);
		dist = VectorLength(v) / 90.0;
		p->color = floor (0xd0 + dist * 7);
		p->colorvel = 0;

		p->alpha = 1.0 - dist;
		p->alphavel = -100;
	}
}


/*
===============
CL_TrapParticles
===============
*/
// RAFAEL
void CL_TrapParticles (entity_t *ent)
{
	vec3_t		move;
	vec3_t		vec;
	vec3_t		start, end;
	float		len;
	int			j;
	cparticle_t	*p;
	int			dec;

	ent->origin[2]-=14;
	VectorCopy (ent->origin, start);
	VectorCopy (ent->origin, end);
	end[2]+=64;

	VectorCopy (start, move);
	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);

	dec = 5;
	VectorScale (vec, 5, vec);

	// FIXME: this is a really silly way to have a loop
	while (len > 0)
	{
		len -= dec;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		VectorClear (p->accel);
		
		p->time = cl.time;

		p->alpha = 1.0;
		p->alphavel = -1.0 / (0.3+frand()*0.2);
		p->color = 0xe0;
		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = move[j] + crand();
			p->vel[j] = crand()*15;
			p->accel[j] = 0;
		}
		p->accel[2] = PARTICLE_GRAVITY;

		VectorAdd (move, vec, move);
	}

	{

	
	int			i, j, k;
	cparticle_t	*p;
	float		vel;
	vec3_t		dir;
	vec3_t		org;

	
	ent->origin[2]+=14;
	VectorCopy (ent->origin, org);


	for (i=-2 ; i<=2 ; i+=4)
		for (j=-2 ; j<=2 ; j+=4)
			for (k=-2 ; k<=4 ; k+=4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->time = cl.time;
				p->color = 0xe0 + (rand()&3);

				p->alpha = 1.0;
				p->alphavel = -1.0 / (0.3 + (rand()&7) * 0.02);
				
				p->org[0] = org[0] + i + ((rand()&23) * crand());
				p->org[1] = org[1] + j + ((rand()&23) * crand());
				p->org[2] = org[2] + k + ((rand()&23) * crand());
	
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;
	
				VectorNormalize (dir);						
				vel = 50 + rand()&63;
				VectorScale (dir, vel, p->vel);

				p->accel[0] = p->accel[1] = 0;
				p->accel[2] = -PARTICLE_GRAVITY;
			}
	}
}


/*
===============
CL_BFGExplosionParticles (@0040aff0)
===============
*/
void CL_BFGExplosionParticles (vec3_t org)
{
	int			i, j;
	cparticle_t	*p;

	for (i=0 ; i<256 ; i++)
	{
		if (!(p = CL_AllocParticle ()))
			return;

		p->time = cl.time;
		p->color = 0xd0 + (rand()&7);

		for (j=0 ; j<3 ; j++)
		{
			p->org[j] = org[j] + ((rand()%32)-16);
			p->vel[j] = (rand()%384)-192;
		}

		p->accel[0] = p->accel[1] = 0;
		p->accel[2] = -PARTICLE_GRAVITY;
		p->alpha = 1.0;

		p->alphavel = -0.8 / (0.5 + frand()*0.3);
	}
}


/*
===============
CL_TeleportParticles

===============
*/
void CL_TeleportParticles (vec3_t org)
{
	int			i, j, k;
	cparticle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<=16 ; i+=4)
		for (j=-16 ; j<=16 ; j+=4)
			for (k=-16 ; k<=32 ; k+=4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->time = cl.time;
				p->color = 7 + (rand()&7);

				p->alpha = 1.0;
				p->alphavel = -1.0 / (0.3 + (rand()&7) * 0.02);
				
				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);
	
				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);

				p->accel[0] = p->accel[1] = 0;
				p->accel[2] = -PARTICLE_GRAVITY;
			}
}


/*
==============================================================

RAIN AND SNOW

==============================================================
*/


/*
===============
CL_SnowParticle (@00409270)
===============
*/
void CL_SnowParticle (vec3_t org, float top, float bottom)
{
	cparticle2_t	*p;

	if (!(p = CL_AllocParticle2 ()))
		return;

	p->time = cl.time;
	p->color = 10 + (rand()&7);
	VectorCopy (org, p->org);
	p->vel[0] = p->vel[1] = 0;
	p->vel[2] = -50;
	VectorClear (p->accel);
	p->alpha = 0.4;
	p->alphavel = 0;
	p->top = top;
	p->bottom = bottom;
}

/*
===============
CL_RainParticle (@00409300)
===============
*/
void CL_RainParticle (vec3_t org, float top, float bottom)
{
	cparticle2_t	*p;

	if (!(p = CL_AllocParticle2 ()))
		return;

	p->time = cl.time;
	p->color = 10 + (rand()&7);
	VectorCopy (org, p->org);
	p->vel[0] = p->vel[1] = 0;
	p->vel[2] = -225;
	p->accel[0] = p->accel[1] = 0;
	p->accel[2] = -20;
	p->alpha = 0.25;
	p->alphavel = 0;
	p->top = top;
	p->bottom = bottom;
	p->type = 0;
}


/*
==============================================================

SPARKS

==============================================================
*/


/*
===============
CL_UpdateSparks (@00408610)

Moves the sparks, bounces them off the world and lays down their trails
===============
*/
void CL_UpdateSparks (void)
{
	static float	lasttime;
	float		now, dt;
	int			i, j;
	cspark_t	*s;
	trace_t		tr;
	float		speed, newspeed, backoff, f;
	float		pathlen, viewdist, step, minstep, farscale, dist, size, lifetime;
	float		c, sn;
	vec3_t		vieworg, pos;
	cparticle_t	*p;

	now = cl.time * 0.001;
	if (now < lasttime)
	{
		lasttime = now;
		return;
	}
	if (now == lasttime)
		return;

	dt = now - lasttime;
	VectorCopy (cl.refdef.vieworg, vieworg);
	if (dt > 0.05)
		dt = 0.05;
	lasttime = now;

	for (i=0, s=cl_sparks ; i<MAX_SPARKS ; i++, s++)
	{
		if (!s->inuse)
			continue;

		// glow
		if (s->light > 0 && s->spiral == 0 && s->lifetime > 0.2)
		{
			f = (s->lifetime - s->age) / s->lifetime;
			if (f * s->start_alpha * 32 > 8)
				V_AddLight (s->origin, f * s->light * s->start_alpha, 1, 0.954, 0.66);
		}

		if (s->gravity != 0)
			s->vel[2] -= dt * s->gravity;

		speed = VectorNormalize2 (s->vel, s->dir);
		VectorMA (s->origin, dt, s->vel, s->origin);
		tr = CL_PMTrace (s->lastorigin, vec3_origin, vec3_origin, s->origin);

		if (s->bounce != 0 && tr.fraction < 1)
		{
			backoff = DotProduct (tr.plane.normal, s->vel) * s->bounce;
			for (j=0 ; j<3 ; j++)
			{
				s->vel[j] -= backoff * tr.plane.normal[j];
				if (s->vel[j] > -0.1 && s->vel[j] < 0.1)
					s->vel[j] = 0;
			}
			newspeed = VectorNormalize2 (s->vel, s->dir);
			if (newspeed < 1)
			{
				s->inuse = 0;
				continue;
			}
			if (newspeed > speed)
				VectorScale (s->dir, speed, s->vel);
			VectorAdd (tr.endpos, tr.plane.normal, s->origin);
		}
		else if (tr.startsolid || tr.allsolid || tr.fraction < 1)
		{
			s->inuse = 0;
			continue;
		}

		if (s->spiral_grow != 0)
			s->spiral += dt * s->spiral_grow;

		// trail spacing: denser near the view
		pathlen = VectorDistance (s->origin, s->lastorigin);
		viewdist = VectorDistance (s->origin, vieworg);
		if (viewdist <= 128)
			f = 0.3;
		else if (viewdist <= 1024)
			f = (viewdist - 128) * (1.0/1920) * 4.5 + 0.3;
		else
			f = 4.5;
		step = f * s->density;

		minstep = pathlen / (600 / (1.0/dt));
		if (step < minstep)
			step = minstep;
		if (step < pathlen * 0.05)
			step = pathlen * 0.05;

		if (viewdist < 512)
			farscale = 1;
		else
		{
			farscale = (viewdist - 512) * (1.0/1024) + 1;
			if (farscale > 10)
				farscale = 10;
		}

		if (step > pathlen)
			step = pathlen;
		if (step == 0)
			step = 1;

		VectorCopy (s->lastorigin, pos);
		for (dist = step ; dist <= pathlen ; dist += step)
		{
			VectorMA (pos, step, s->dir, pos);

			if (!free_particles)
			{
				dist = pathlen;
				break;
			}

			f = (s->lifetime - (dist/pathlen*dt + s->age)) / s->lifetime;

			if (s->type == SPARK_SMOKE)
			{
				size = s->density * 0.5 * farscale;
				if (size < 0.5)
					size = 0.5;
				else if (size > 2)
					size = 2;

				if (viewdist < 128)
					continue;

				lifetime = (frand() + 1) * 0.5 * (f * s->ptlife);
				if (s->smoke_scale == 0)
					re.AddSFX (SFX_SPRITE_EXPLOSION1, 0x400, pos, vec3_origin, 0,
						lifetime, 1, 1, 0,
						size*2, (f*20 + 2)*size, size*2.4, (f*24 + 2.4)*size);
				else
					re.AddSFX (SFX_SPRITE_EXPLOSION1, 0x400, pos, vec3_origin, 0,
						lifetime, 1, 1, 0,
						size, (f*s->smoke_scale*2 + 1)*size,
						size*1.2, (f*s->smoke_scale*2.4 + 1.2)*size);
				continue;
			}

			p = CL_AllocParticle ();
			p->type = s->type;
			f = dist/pathlen*dt;
			p->time = s->starttime - (int)((f + s->age) * -1000);
			p->color = 0;
			p->alpha = ((s->lifetime - (f + s->age)) / s->lifetime)
				* (s->start_alpha - s->end_alpha) + s->end_alpha;
			VectorClear (p->accel);
			p->alphavel = -1.0 / s->ptlife;
			VectorCopy (pos, p->org);
			VectorClear (p->vel);

			if (s->spiral != 0)
			{
				s->spiral_angle += s->spiral_speed * s->density * 0.3;
				c = cos (s->spiral_angle);
				sn = sin (s->spiral_angle);
				if (s->spiral_flags & 1)
				{
					VectorMA (p->org, c*s->spiral, s->right, p->org);
					VectorMA (p->org, sn*s->spiral, s->up, p->org);
				}
				if (s->spiral_flags & 2)
				{
					VectorMA (p->vel, c*s->spiral*(1.0/s->ptlife), s->right, p->vel);
					VectorMA (p->vel, sn*s->spiral*(1.0/s->ptlife), s->up, p->vel);
				}
			}
			p->delay = 0;
		}

		// continue the trail from where the last particle went
		VectorMA (s->lastorigin, dist - step, s->dir, s->origin);
		VectorCopy (s->origin, s->lastorigin);

		s->age += dt;
		if (s->age > s->lifetime)
			s->inuse = 0;
	}
}

/*
===============
CL_MetalSparks (@00408dd0)

TE_METAL_SPARKS: glowing sparks with two faint companions each
===============
*/
void CL_MetalSparks (vec3_t org, vec3_t dir, int count)
{
	int			i, j;
	vec3_t		vel, start;
	cspark_t	*s;

	for (i=0 ; i<count ; i++)
	{
		for (j=0 ; j<3 ; j++)
			vel[j] = (frand()*300 + 200) * dir[j] * 0.5;
		for (j=0 ; j<3 ; j++)
			start[j] = org[j] + dir[j];

		s = CL_AllocSpark (start, vel, frand() * 0.64, 0.1);
		if (s)
		{
			s->light = 32;
			CL_AllocSpark (start, vel, 0.2, 0.05);
			CL_AllocSpark (start, vel, 0.2, 0.05);
		}
	}
}

/*
===============
CL_RailSparks (@00408ec0)

Spiralling sparks from both ends of a rail trail
===============
*/
void CL_RailSparks (vec3_t start, vec3_t end, vec3_t dir)
{
	int			i, j, count;
	vec3_t		vec, v, org;
	cspark_t	*s;

	VectorSubtract (end, start, vec);
	VectorNormalize (vec);
	VectorCopy (start, org);

	count = rand()%3 + 2;
	for (i=0 ; i<count ; i++)
	{
		VectorCopy (vec, v);
		for (j=0 ; j<2 ; j++)
			v[j] += (rand()%20 - 10) * 0.03;
		VectorScale (v, 200, v);

		s = CL_AllocSpark (org, v, frand() + 0.5, 0.5);
		if (!s)
			return;
		s->spiral = 0.4;
		s->gravity = 200;
		s->density = 1;
		s->type = 0x33;
		s->spiral_speed = 0.5;
		s->spiral_flags = 3;
		s->spiral_grow = (1.0 / s->lifetime) * -0.4;
	}

	count = (rand()%4) + 3;
	for (i=0 ; i<count ; i++)
	{
		VectorCopy (dir, v);
		for (j=0 ; j<2 ; j++)
			v[j] += (rand()%20 - 10) * 0.03;
		VectorScale (v, 300, v);

		s = CL_AllocSpark (end, v, frand() + 0.5, 0.1);
		if (!s)
			return;
		s->spiral = 0.4;
		s->gravity = 200;
		s->density = 1;
		s->type = 0x33;
		s->spiral_speed = 0.5;
		s->spiral_flags = 3;
		s->spiral_grow = (1.0 / s->lifetime) * -0.4;
	}
}

/*
===============
CL_ExplosionDebris (@00409130)

Smoke trailing debris thrown out of explosions
===============
*/
void CL_ExplosionDebris (vec3_t org, vec3_t dir, int count, float speed,
	float lifescale, float ptlifescale, float gravity, float smoke_scale)
{
	int			i, j;
	float		f;
	vec3_t		v;
	cspark_t	*s;

	for (i=0 ; i<count ; i++)
	{
		f = (frand() + 1.0) * 0.5;
		VectorCopy (dir, v);
		for (j=0 ; j<3 ; j++)
			v[j] += (rand()%20 - 10) * 0.1;
		if (dir[2] == 1.0)
			v[2] = (dir[2] - 0.5) + (rand()%20) * 0.05;
		VectorScale (v, speed, v);
		rand ();

		s = CL_AllocSpark (org, v, f*lifescale, f*ptlifescale);
		if (!s)
			return;
		s->spiral = 0;
		s->gravity = gravity;
		s->density = 2;
		s->type = SPARK_SMOKE;
		s->smoke_scale = smoke_scale;
		s->bounce = 1.1;
	}
}


/*
===============
CL_AddParticles (@0040b0f0)

Particles of type >= 50 may carry a start delay. The color is not
animated in Kingpin.
===============
*/
void CL_AddParticles (void)
{
	cparticle_t		*p, *next;
	float			alpha;
	float			time, time2;
	vec3_t			org;
	int				color;
	cparticle_t		*active, *tail;
	qboolean		waiting;

	CL_UpdateSparks ();

	active = NULL;
	tail = NULL;

	for (p=active_particles ; p ; p=next)
	{
		next = p->next;
		waiting = false;

		time = (cl.time - p->time)*0.001;
		if (p->type >= 0x32 && p->delay != 0)
		{
			if (p->delay <= time)
				time -= p->delay;
			else
			{	// not started yet
				waiting = true;
				p->delay -= time;
				p->time = cl.time;
			}
		}

		alpha = time*p->alphavel + p->alpha;
		if (alpha <= 0)
		{	// faded out
			p->next = free_particles;
			free_particles = p;
			p->type = 0;
			continue;
		}

		p->next = NULL;
		if (!tail)
			active = tail = p;
		else
		{
			tail->next = p;
			tail = p;
		}

		if (waiting)
			continue;

		if (alpha > 1.0)
			alpha = 1;
		color = p->color;

		time2 = time*time;

		org[0] = p->org[0] + p->vel[0]*time + p->accel[0]*time2;
		org[1] = p->org[1] + p->vel[1]*time + p->accel[1]*time2;
		org[2] = p->org[2] + p->vel[2]*time + p->accel[2]*time2;

		V_AddParticleEx (org, color, alpha, p->type, (int)(p->time - 1000000));
	}

	active_particles = active;
}

/*
===============
CL_AddParticles2 (@0040b270)

Falling particles restart at the top when they reach the floor or after
two seconds. Type 0 wraps around, others restart at a random height.
===============
*/
void CL_AddParticles2 (void)
{
	cparticle2_t	*p, *next;
	cparticle2_t	*active, *tail;
	float			time, time2;
	vec3_t			org;
	int				color, range;

	active = NULL;
	tail = NULL;

	for (p=active_particles2 ; p ; p=next)
	{
		next = p->next;

		time = (cl.time - p->time)*0.001;
		if (time > 2)
		{
			p->time = cl.time;
			time = 0;
			range = abs ((int)(p->bottom - p->top));
			if (!range)
				range = 1;
			p->org[2] = p->top - (rand()%range + 1) + 1;
		}

		p->next = NULL;
		if (!tail)
			active = tail = p;
		else
		{
			tail->next = p;
			tail = p;
		}

		color = p->color;
		time2 = time*time;
		org[0] = p->org[0] + p->vel[0]*time + p->accel[0]*time2;
		org[1] = p->org[1] + p->vel[1]*time + p->accel[1]*time2;
		org[2] = p->org[2] + p->vel[2]*time + p->accel[2]*time2;

		if (org[2] < p->bottom)
		{
			p->time = cl.time;
			range = abs ((int)(p->bottom - org[2]));
			if (p->type == 0)
				p->org[2] = p->top - range;
			else
			{
				if (!range)
					range = 1;
				p->org[2] = p->top - rand()%range + 1;
			}
		}

		V_AddParticle2 (org, color, p->alpha, p->type, (int)(p->time - 1000000));
	}

	active_particles2 = active;
}


/*
==============
CL_EntityEvent

An entity has just been parsed that has an event value

the female events are there for backwards compatability
==============
*/
extern struct sfx_s	*cl_sfx_footsteps[9][4];
extern struct sfx_s	*cl_sfx_land[9];

void CL_EntityEvent (entity_state_t *ent)
{
	switch (ent->event)
	{
	case EV_FOOTSTEP0: case EV_FOOTSTEP1: case EV_FOOTSTEP2:
	case EV_FOOTSTEP3: case EV_FOOTSTEP4: case EV_FOOTSTEP5:
	case EV_FOOTSTEP6: case EV_FOOTSTEP7: case EV_FOOTSTEP8:
		if (cl_footsteps->value)
			S_StartSound (NULL, ent->number, CHAN_BODY,
				cl_sfx_footsteps[ent->event - EV_FOOTSTEP0][rand()&3], 1, ATTN_IDLE, 0);
		break;
	case EV_FALLSHORT0: case EV_FALLSHORT1: case EV_FALLSHORT2:
	case EV_FALLSHORT3: case EV_FALLSHORT4: case EV_FALLSHORT5:
	case EV_FALLSHORT6: case EV_FALLSHORT7: case EV_FALLSHORT8:
		S_StartSound (NULL, ent->number, CHAN_BODY,
			cl_sfx_land[ent->event - EV_FALLSHORT0], 1, ATTN_NORM, 0);
		break;
	case EV_FALL:
		S_StartSound (NULL, ent->number, CHAN_AUTO, S_RegisterSound ("*fall2.wav"), 1, ATTN_NORM, 0);
		break;
	case EV_FALLFAR:
		S_StartSound (NULL, ent->number, CHAN_AUTO, S_RegisterSound ("*fall1.wav"), 1, ATTN_NORM, 0);
		break;
	case EV_PLAYER_TELEPORT:
		S_StartSound (NULL, ent->number, CHAN_WEAPON, S_RegisterSound("misc/tele1.wav"), 1, ATTN_IDLE, 0);
		break;
	}
}


/*
==============
CL_ClearEffects

==============
*/
void CL_ClearEffects (void)
{
	CL_ClearParticles ();
	CL_ClearDlights ();
	CL_ClearLightStyles ();
	CL_ClearFlames ();
}

/*
==============================================================

KINGPIN EFFECTS

==============================================================
*/

extern cvar_t *hand;

flamejunc_t	*cl_flameactive[MAX_EDICTS];
flamejunc_t	*cl_flameshooteractive[MAX_EDICTS];
static int	cl_flame_seen[MAX_EDICTS];
static int	cl_shooter_seen[MAX_EDICTS];
static int	cl_playerflame_last_time;
static int	cl_playerflame_prev_time;
static byte	cl_playerflame_broken;
static int	cl_playerflame_surflight_time;

static void CL_FreeFlameList (flamejunc_t **head)
{
	flamejunc_t *node, *next;

	for (node = *head; node; node = next)
	{
		next = node->next;
		free (node);
	}
	*head = NULL;
}

void CL_ClearFlames (void)
{
	int i;

	for (i = 0; i < MAX_EDICTS; i++)
	{
		CL_FreeFlameList (&cl_flameactive[i]);
		CL_FreeFlameList (&cl_flameshooteractive[i]);
		cl_flame_seen[i] = cl_shooter_seen[i] = 0;
	}
	cl_playerflame_last_time = 0;
	cl_playerflame_prev_time = 0;
	cl_playerflame_broken = 0;
	cl_playerflame_surflight_time = 0;
}

void CL_GCFlames (void)
{
	int i;

	// An entity removed from a packet frame will no longer call its updater.
	for (i = 0; i < MAX_EDICTS; i++)
	{
		if (cl_flameactive[i] && (cl.time < cl_flame_seen[i] ||
			cl.time - cl_flame_seen[i] > 1000))
			CL_FreeFlameList (&cl_flameactive[i]);
		if (cl_flameshooteractive[i] && (cl.time < cl_shooter_seen[i] ||
			cl.time - cl_shooter_seen[i] > 1000))
			CL_FreeFlameList (&cl_flameshooteractive[i]);
	}
}

/*
===============
CL_GrenadeSmoke

EF_GRENADE: a puff of smoke every other frame (kingpin.exe @0040a0e0)
===============
*/
void CL_GrenadeSmoke (vec3_t origin)
{
	if (rand() & 1)
		return;
	re.AddSFX (SFX_SPRITE_SMOKE_SMALL, 0, origin, vec3_origin, 0,
		0.4, 0.5, 0, 0.2,
		1.0, 96, 1.0, 96);
}

/*
===============
CL_FlameTrail

EF_FLAMETHROWER: fire sprites around a burning entity. While it stands
still this runs once per server frame (kingpin.exe @00409700).
===============
*/
void CL_FlameTrail (centity_t *cent, vec3_t origin)
{
	int		i, size;
	vec3_t	org, v;
	float	height;

	VectorSubtract (origin, cent->flame_lastorigin, v);
	if (VectorLength (v) < 12)
	{
		if (cent->flame_serverframe == cent->serverframe)
			return;
		cent->flame_serverframe = cent->serverframe;
	}
	VectorCopy (origin, cent->flame_lastorigin);

	for (i=0 ; i<8 ; i++)
	{
		org[0] = origin[0] + crand()*4;
		org[1] = origin[1] + crand()*4;
		org[2] = origin[2] + crand()*20 + 16;

		size = rand()%10 + 20;
		height = (float)(rand()%25) + (float)(size*2);
		re.AddSFX (SFX_SPRITE_EXPLOSION1, 0, org, vec3_origin, 0,
			0.6, 1.0, 1.0, 0.15,
			(float)size, (float)(rand()%20) + (float)((size>>2) + size),
			(float)(size>>1) + (float)size, height);
	}
}

/*
===============
CL_FlameEffects / CL_FlameShooter

Persistent flame junctions from kingpin.exe @00403a90 and @00404a40.
The two effects have separate lists but share the same movement and rendering.
===============
*/
static void CL_AddFlameJunction (flamejunc_t **head, vec3_t origin,
	vec3_t forward, vec3_t right, vec3_t up, vec3_t angles,
	float fraction, qboolean player, qboolean shooter)
{
	flamejunc_t *node;
	vec3_t ringorg, ringangles;
	float scale, random_scale, ring_width, ring_height, yaw;
	int flags;

	node = (flamejunc_t *)malloc (sizeof(*node));
	if (!node)
		return;
	memset (node, 0, sizeof(*node));
	node->next = *head;
	*head = node;
	VectorCopy (origin, node->org);
	if (player)
		VectorMA (node->org, 4, right, node->org);
	VectorScale (forward, 900, node->vel);
	VectorCopy (forward, node->unitvel);

	random_scale = 1.3f - (float)rand() * (1.0f / (float)RAND_MAX) * 0.6f;
	if (shooter)
	{
		node->start_width = node->start_height = 32 * (random_scale + 1) * 0.5f;
		node->end_width = 150 * random_scale;
		node->end_height = 111.5f * random_scale;
		node->lifetime = 0.3f;
	}
	else
	{
		scale = fraction * 0.7f + 0.3f;
		node->start_width = node->start_height =
			(fraction + 1) * 0.5f * 16 * (random_scale + 1) * 0.5f;
		node->end_width = scale * 300 * random_scale;
		node->end_height = scale * 223 * random_scale;
		node->lifetime = (fraction + 1) * 0.5f * 0.8f;
		node->fadein_time = 0.1166667f;
	}
	node->start_alpha = 1.5f;
	node->width = node->start_width;
	node->height = node->start_height;

	/* The retail effect places an eight-sprite ring once a flame has a trail. */
	if (!shooter && node->next && node->next->next && fraction * 720 > 96)
	{
		flags = SFX_FLAG_SURFACE_EFFECT | SFX_FLAG_NO_FLIP |
			SFX_FLAG_VEL_IS_ANGLES | SFX_FLAG_ADDITIVE_BLEND;
		if (player)
			flags |= SFX_FLAG_DEPTHHACK;
		VectorMA (origin, node->fadein_time * 900 * 0.4f, forward, ringorg);
		ringorg[2] += 0.5f;
		VectorMA (ringorg, -0.5f - scale * 3, up, ringorg);
		VectorMA (ringorg, -1, right, ringorg);
		ring_width = node->fadein_time * 900 * 0.75f;
		ring_height = (scale * 14 + 4) * 1.25f;
		VectorCopy (angles, ringangles);
		ringangles[2] += (float)(23 - rand()%6);
		for (yaw=-90 ; yaw<90 ; yaw+=22.5f)
		{
			ringangles[2] += yaw;
			re.AddSFX (SFX_SPRITE_FLAMETHROWER_BLUE4, flags, ringorg, ringangles, 0,
				0.01f, 0.25f, 0.25f, 0,
				ring_width, ring_width, ring_height, ring_height);
			ringangles[2] -= yaw;
		}
	}
}


static trace_t CL_FlameTrace (vec3_t start, vec3_t end, float bounds, qboolean clip_entities)
{
	trace_t tr;
	vec3_t mins, maxs;

	VectorSet (mins, -bounds, -bounds, -bounds);
	VectorSet (maxs, bounds, bounds, bounds);

	tr = CL_PMTraceEx (start, mins, maxs, end, CL_PMTRACE_WATER | (clip_entities ? CL_PMTRACE_ENTITIES : 0));

	return tr;
}

static void CL_ClipFlameVelocity (flamejunc_t *node, trace_t *tr)
{
	float backoff, overbounce;
	int i;

	overbounce = 1;
	if (tr->plane.normal[2] < -0.25f)
		overbounce = 1 - (tr->plane.normal[2] + 0.25f);
	backoff = DotProduct (node->vel, tr->plane.normal) * overbounce;
	for (i=0 ; i<3 ; i++)
	{
		node->vel[i] -= backoff * tr->plane.normal[i];
		if (node->vel[i] > -0.1f && node->vel[i] < 0.1f)
			node->vel[i] = 0;
	}
	VectorCopy (tr->endpos, node->org);
	VectorMA (node->org, 4, tr->plane.normal, node->org);
	VectorNormalize2 (node->vel, node->unitvel);
	if (!node->hit_wall)
	{
		if (node->end_width > node->width)
			node->end_width *= 0.75f;
		else
			node->end_width = node->width;
		if (node->end_height > node->height)
			node->end_height *= 0.75f;
		else
			node->end_height = node->height;
		VectorScale (node->vel, 0.5f, node->vel);
	}
	node->hit_wall = true;
}

static void CL_FlameImpactLight (trace_t *tr)
{
	vec3_t origin;
	float intensity;

	if (!tr->surface ||
		(cl_playerflame_surflight_time >= cl.time - 100 &&
		 cl_playerflame_surflight_time <= cl.time))
		return;
	VectorMA (tr->endpos, 8, tr->plane.normal, origin);
	intensity = strstr (tr->surface->name, "wood") ? -0.05f : -0.2f;
	V_AddSurfLight (origin, 48, intensity, 1, 1, 1);
	cl_playerflame_surflight_time = cl.time;
}

static void CL_MoveFlameJunction (flamejunc_t *node, float amount,
	qboolean initial, qboolean player)
{
	vec3_t oldorg, end, step;
	trace_t tr;
	float remaining, walked;

	VectorCopy (node->org, oldorg);
	VectorMA (node->org, amount, node->vel, end);
	VectorCopy (end, node->org);
	tr = CL_FlameTrace (oldorg, end, 16, false);
	if (initial)
	{
		VectorCopy (tr.endpos, node->org);
		return;
	}
	if (tr.fraction >= 1)
		return;

	/* NODRAW volumes are stepped through before treating the hit as a wall. */
	if (tr.surface && (tr.surface->flags & SURF_NODRAW))
	{
		remaining = VectorLength (node->vel) * amount;
		if (!tr.startsolid)
			remaining *= 1 - tr.fraction;
		VectorMA (tr.endpos, 4, node->unitvel, step);
		for (walked=4 ; walked<remaining ; walked+=4)
		{
			tr = CL_FlameTrace (step, end, 16, false);
			VectorMA (step, 4, node->unitvel, step);
			if (!tr.startsolid &&
				(!tr.surface || !(tr.surface->flags & SURF_NODRAW)))
				break;
		}
		if (walked >= remaining || tr.fraction >= 1)
		{
			if (player)
				CL_FlameImpactLight (&tr);
			return;
		}
	}
	if (player)
		CL_FlameImpactLight (&tr);
	CL_ClipFlameVelocity (node, &tr);
}

static void CL_UpdateFlameJunctions (flamejunc_t **head, float dt,
	qboolean player)
{
	flamejunc_t *node, *next, *dead, *deadnext;
	float phase, decay;
	qboolean pruned;

	pruned = false;
	for (node = *head; node; node = next)
	{
		next = node->next;
		if (node->aged < node->fadein_time)
		{
			node->aged = node->fadein_time;
			CL_MoveFlameJunction (node,
				node->fadein_time * (player ? 1.0f : 0.5f), true, player);
			node->alpha = 0;
		}
		else if (node->alpha != 0)
		{
			node->aged += dt;
			CL_MoveFlameJunction (node, dt, false, player);
		}

		if (node->aged < node->lifetime)
			node->alpha = (node->aged / node->lifetime) *
				(node->end_alpha - node->start_alpha) + node->start_alpha;
		else
			node->alpha = 0;

		if (node->aged < node->lifetime * 0.15f)
		{
			node->width = node->start_width;
			node->height = node->start_height;
		}
		else if (node->aged <= node->lifetime - node->lifetime * 0.3f)
		{
			phase = (node->aged - node->lifetime * 0.15f) /
				(node->lifetime - node->lifetime * 0.3f);
			node->width = (node->end_width - node->start_width) * phase +
				node->start_width;
			node->height = (node->end_height - node->start_height) * phase +
				node->start_height;
		}
		else
		{
			decay = (node->aged - node->lifetime * 0.5f) * dt * 0.7f /
				(node->lifetime * 0.5f);
			node->width -= (node->end_width - node->start_width) * decay;
			node->height -= (node->end_height - node->start_height) * decay;
			if (node->width < 1)
				node->width = 1;
		}

		if (next && node->aged >= node->lifetime &&
			next->aged >= next->lifetime)
		{
			for (dead=next; dead; dead=deadnext)
			{
				deadnext = dead->next;
				free (dead);
			}
			node->next = NULL;
			pruned = true;
			break;
		}
	}
	if (!pruned && *head && !(*head)->next &&
		(*head)->aged >= (*head)->lifetime)
	{
		free (*head);
		*head = NULL;
	}
}

static void CL_DrawFlameJunctions (flamejunc_t *head, qboolean player,
	vec3_t viewforward)
{
	flamejunc_t *node, *older;
	vec3_t delta, pos, viewdir;
	float length, step, along, t, width, height, alpha, facing;

	for (node = head, older = head ? head->next : NULL;
		older; node = older, older = older->next)
	{
		if (!older->next || older->alpha <= 0)
			continue;
		VectorSubtract (older->org, node->org, delta);
		length = VectorNormalize (delta);
		if (length > 128 || DotProduct (node->unitvel, older->unitvel) <= 0.2f)
			length = 1;
		VectorSubtract (node->org, cl.refdef.vieworg, viewdir);
		VectorNormalize (viewdir);
		VectorAdd (viewdir, viewforward, viewdir);
		VectorNormalize (viewdir);
		facing = DotProduct (viewdir, delta);
		if (facing < 0)
			facing = -facing;
		step = (facing*facing*facing*0.7f + 0.3f) * node->width * 1.2f;
		if (!player)
			step *= 0.5f;
		else if (node->width < 64)
			step *= (node->width * (1.0f/64.0f) + 1) * 0.5f;
		if (step / node->width < 0.1f)
			step = node->width * 0.1f;
		if (length / step > 20)
			step = length * 0.05f;
		for (along = 0; along < length; along += step)
		{
			t = along / length;
			VectorMA (node->org, along, delta, pos);
			pos[2] += node->aged * 64 / node->lifetime;
			if (!player)
				pos[2] -= 8;
			width = node->width + t * (older->width - node->width);
			height = (node->height + t * (older->height - node->height)) * 1.4f;
			// With ALPHA_IS_FRAME this argument selects the sprite frame.
			alpha = node->aged / node->lifetime;
			re.AddSFX (SFX_SPRITE_EXPLOSION1, SFX_FLAG_ALPHA_IS_FRAME | SFX_FLAG_NO_FLIP,
				pos, vec3_origin, 0, 0.01f, alpha, alpha, 0,
				width, width, height, height);
		}
	}
}

static float CL_FlameDeltaTime (int *last_time, int *prev_last_time)
{
	float dt;

	if (*last_time != cl.time)
	{
		*prev_last_time = *last_time;
		*last_time = cl.time;
		if (*last_time - *prev_last_time > 200)
			*prev_last_time = *last_time - 100;
	}
	dt = (*last_time - *prev_last_time) * 0.001f;
	if (dt < 0) dt = 0;
	return dt;
}

void CL_FlameEffects (entity_t *ent, entity_state_t *s1, int player)
{
	vec3_t forward, right, up, origin, trace_start, end, effect_angles;
	trace_t tr;
	flamejunc_t **head;
	int *last_time, *prev_last_time;
	byte *broken;
	int number;
	float fraction, dt;

	number = s1->number;
	if (number < 0 || number >= MAX_EDICTS)
		return;
	head = &cl_flameactive[number];
	cl_flame_seen[number] = cl.time;
	if (cl_paused && cl_paused->value)
	{
		if (player)
			AngleVectors (cl.refdef.viewangles, forward, NULL, NULL);
		else
			AngleVectors (ent->angles, forward, NULL, NULL);
		s1->flamejunc_head = *head;
		CL_DrawFlameJunctions (*head, player != 0, forward);
		return;
	}
	if (player)
	{
		AngleVectors (cl.refdef.viewangles, forward, right, up);
		if (hand && (int)hand->value == 1)
			VectorNegate (right, right);
		VectorCopy (cl.refdef.vieworg, origin);
		VectorCopy (cl.refdef.viewangles, effect_angles);
		origin[2] -= 32;
		last_time = &cl_playerflame_last_time;
		prev_last_time = &cl_playerflame_prev_time;
		broken = &cl_playerflame_broken;
	}
	else
	{
		AngleVectors (ent->angles, forward, right, up);
		VectorCopy (ent->origin, origin);
		VectorCopy (ent->angles, effect_angles);
		last_time = &s1->last_time;
		prev_last_time = &s1->prev_last_time;
		broken = &s1->broken_flag;
	}
	VectorMA (origin, 24, forward, origin);
	VectorMA (origin, 7, right, origin);
	origin[2] += 30.5f;
	VectorMA (origin, -5.5f, up, origin);
	VectorMA (origin, 8, up, trace_start);
	VectorMA (trace_start, 720, forward, end);
	tr = CL_FlameTrace (trace_start, end, 2, true);
	fraction = tr.fraction;
	if (player)
	{
		VectorMA (origin, -8, forward, origin);
		if (fraction > 0.2f)
		{
			fraction += 0.12f;
			if (fraction > 1)
				fraction = 1;
		}
	}
	dt = CL_FlameDeltaTime (last_time, prev_last_time);
	if (s1->renderfx2 & RF2_FLAMETHROWER)
	{
		if (*broken)
		{
			CL_FreeFlameList (head);
			*broken = 0;
		}
		if (fraction * 720 > 48 || !(tr.contents & MASK_WATER))
			CL_AddFlameJunction (head, origin, forward, right, up,
				effect_angles, fraction, player != 0, false);
		else
			*broken = 1;
	}
	else if (*head)
		*broken = 1;
	CL_UpdateFlameJunctions (head, dt, player != 0);
	s1->flamejunc_head = *head;
	CL_DrawFlameJunctions (*head, player != 0, forward);
}

void CL_FlameShooter (entity_t *ent, entity_state_t *s1, vec3_t origin)
{
	vec3_t angles, forward, right, up, end;
	trace_t tr;
	flamejunc_t **head;
	int number;
	float dt;

	number = s1->number;
	if (number < 0 || number >= MAX_EDICTS)
		return;
	head = &cl_flameshooteractive[number];
	cl_shooter_seen[number] = cl.time;
	ent->angles[0] += 10;
	VectorCopy (ent->angles, angles);
	AngleVectors (angles, forward, right, up);
	if (cl_paused && cl_paused->value)
	{
		s1->flamejunc_head2 = *head;
		CL_DrawFlameJunctions (*head, false, forward);
		return;
	}
	VectorMA (origin, 30, forward, end);
	tr = CL_FlameTrace (origin, end, 2, true);
	dt = CL_FlameDeltaTime (&s1->last_time, &s1->prev_last_time);
	if ((s1->renderfx2 & RF2_FLAMESHOOTER) && !(s1->renderfx2 & RF2_PASSLIGHT))
	{
		if (s1->broken_flag)
		{
			CL_FreeFlameList (head);
			s1->broken_flag = 0;
		}
		if (tr.fraction * 30 > 48 || !(tr.contents & MASK_WATER))
			CL_AddFlameJunction (head, origin, forward, right, up,
				angles, tr.fraction, false, true);
		else
			s1->broken_flag = 1;
	}
	else if (*head)
		s1->broken_flag = 1;
	CL_UpdateFlameJunctions (head, dt, false);
	s1->flamejunc_head2 = *head;
	CL_DrawFlameJunctions (*head, false, forward);
}
