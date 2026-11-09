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
// cl_ents.c -- entity parsing and management

#include "client.h"


extern	struct model_s	*cl_mod_powerscreen;

//PGM
int	vidref_val;
//PGM

/*
=========================================================================

FRAME PARSING

=========================================================================
*/

#if 0

typedef struct
{
	int		modelindex;
	int		num; // entity number
	int		effects;
	vec3_t	origin;
	vec3_t	oldorigin;
	vec3_t	angles;
	qboolean present;
} projectile_t;

#define	MAX_PROJECTILES	64
projectile_t	cl_projectiles[MAX_PROJECTILES];

void CL_ClearProjectiles (void)
{
	int i;

	for (i = 0; i < MAX_PROJECTILES; i++) {
//		if (cl_projectiles[i].present)
//			Com_DPrintf("PROJ: %d CLEARED\n", cl_projectiles[i].num);
		cl_projectiles[i].present = false;
	}
}

/*
=====================
CL_ParseProjectiles

Flechettes are passed as efficient temporary entities
=====================
*/
void CL_ParseProjectiles (void)
{
	int		i, c, j;
	byte	bits[8];
	byte	b;
	projectile_t	pr;
	int lastempty = -1;
	qboolean old = false;

	c = MSG_ReadByte (&net_message);
	for (i=0 ; i<c ; i++)
	{
		bits[0] = MSG_ReadByte (&net_message);
		bits[1] = MSG_ReadByte (&net_message);
		bits[2] = MSG_ReadByte (&net_message);
		bits[3] = MSG_ReadByte (&net_message);
		bits[4] = MSG_ReadByte (&net_message);
		pr.origin[0] = ( ( bits[0] + ((bits[1]&15)<<8) ) <<1) - 4096;
		pr.origin[1] = ( ( (bits[1]>>4) + (bits[2]<<4) ) <<1) - 4096;
		pr.origin[2] = ( ( bits[3] + ((bits[4]&15)<<8) ) <<1) - 4096;
		VectorCopy(pr.origin, pr.oldorigin);

		if (bits[4] & 64)
			pr.effects = EF_BLASTER;
		else
			pr.effects = 0;

		if (bits[4] & 128) {
			old = true;
			bits[0] = MSG_ReadByte (&net_message);
			bits[1] = MSG_ReadByte (&net_message);
			bits[2] = MSG_ReadByte (&net_message);
			bits[3] = MSG_ReadByte (&net_message);
			bits[4] = MSG_ReadByte (&net_message);
			pr.oldorigin[0] = ( ( bits[0] + ((bits[1]&15)<<8) ) <<1) - 4096;
			pr.oldorigin[1] = ( ( (bits[1]>>4) + (bits[2]<<4) ) <<1) - 4096;
			pr.oldorigin[2] = ( ( bits[3] + ((bits[4]&15)<<8) ) <<1) - 4096;
		}

		bits[0] = MSG_ReadByte (&net_message);
		bits[1] = MSG_ReadByte (&net_message);
		bits[2] = MSG_ReadByte (&net_message);

		pr.angles[0] = 360*bits[0]/256;
		pr.angles[1] = 360*bits[1]/256;
		pr.modelindex = bits[2];

		b = MSG_ReadByte (&net_message);
		pr.num = (b & 0x7f);
		if (b & 128) // extra entity number byte
			pr.num |= (MSG_ReadByte (&net_message) << 7);

		pr.present = true;

		// find if this projectile already exists from previous frame 
		for (j = 0; j < MAX_PROJECTILES; j++) {
			if (cl_projectiles[j].modelindex) {
				if (cl_projectiles[j].num == pr.num) {
					// already present, set up oldorigin for interpolation
					if (!old)
						VectorCopy(cl_projectiles[j].origin, pr.oldorigin);
					cl_projectiles[j] = pr;
					break;
				}
			} else
				lastempty = j;
		}

		// not present previous frame, add it
		if (j == MAX_PROJECTILES) {
			if (lastempty != -1) {
				cl_projectiles[lastempty] = pr;
			}
		}
	}
}

/*
=============
CL_LinkProjectiles

=============
*/
void CL_AddProjectiles (void)
{
	int		i, j;
	projectile_t	*pr;
	entity_t		ent;

	memset (&ent, 0, sizeof(ent));

	for (i=0, pr=cl_projectiles ; i < MAX_PROJECTILES ; i++, pr++)
	{
		// grab an entity to fill in
		if (pr->modelindex < 1)
			continue;
		if (!pr->present) {
			pr->modelindex = 0;
			continue; // not present this frame (it was in the previous frame)
		}

		ent.model = cl.model_draw[pr->modelindex];

		// interpolate origin
		for (j=0 ; j<3 ; j++)
		{
			ent.origin[j] = ent.oldorigin[j] = pr->oldorigin[j] + cl.lerpfrac * 
				(pr->origin[j] - pr->oldorigin[j]);

		}

		if (pr->effects & EF_BLASTER)
			CL_BlasterTrail (pr->oldorigin, ent.origin);
		V_AddLight (pr->origin, 200, 1, 1, 0);

		VectorCopy (pr->angles, ent.angles);
		V_AddEntity (&ent);
	}
}
#endif

/*
=================
CL_ParseEntityBits

Returns the entity number and the header bits
=================
*/
int	bitcounts[32];	/// just for protocol profiling
int CL_ParseEntityBits (unsigned *bits)
{
	unsigned	b, total;
	int			i;
	int			number;

	total = MSG_ReadByte (&net_message);
	if (total & U_MOREBITS1)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<8;
	}
	if (total & U_MOREBITS2)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<16;
	}
	if (total & U_MOREBITS3)
	{
		b = MSG_ReadByte (&net_message);
		total |= b<<24;
	}

	// count the bits for net profiling
	for (i=0 ; i<32 ; i++)
		if (total&(1<<i))
			bitcounts[i]++;

	if (total & U_NUMBER16)
		number = MSG_ReadShort (&net_message);
	else
		number = MSG_ReadByte (&net_message);

	*bits = total;

	return number;
}

/*
==================
CL_ParseDelta

Can go from either a baseline or a previous packet_entity
==================
*/
#ifdef KINGPIN_ENGINE
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits)
{
	int			i, lights, index;
	cmlight_t	*light;

	// set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);
	if (bits & U_NUM_PARTS)
		to->num_parts = MSG_ReadByte (&net_message);
	if (bits & U_MODEL_PARTS)
		MSG_ReadDeltaModelParts (&net_message, to->model_parts);

	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong(&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(&net_message);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(&net_message);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(&net_message);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(&net_message);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
	{
		to->renderfx = MSG_ReadLong(&net_message);
		to->renderfx2 = MSG_ReadLong(&net_message);
	}
	else if (bits & U_RENDERFX8)
	{
		to->renderfx = MSG_ReadByte(&net_message);
		to->renderfx2 = MSG_ReadByte(&net_message);
	}
	else if (bits & U_RENDERFX16)
	{
		to->renderfx = MSG_ReadShort(&net_message);
		to->renderfx2 = MSG_ReadShort(&net_message);
	}

	if (bits & U_ORIGIN_OFFSET)
	{	// quarter units from the previous origin
		if (bits & U_ORIGIN1)
			to->origin[0] = (MSG_ReadByte (&net_message) - 128) * 0.25 + from->origin[0];
		if (bits & U_ORIGIN2)
			to->origin[1] = (MSG_ReadByte (&net_message) - 128) * 0.25 + from->origin[1];
		if (bits & U_ORIGIN3)
			to->origin[2] = (MSG_ReadByte (&net_message) - 128) * 0.25 + from->origin[2];
	}
	else
	{
		if (bits & U_ORIGIN1)
			to->origin[0] = MSG_ReadCoord (&net_message);
		if (bits & U_ORIGIN2)
			to->origin[1] = MSG_ReadCoord (&net_message);
		if (bits & U_ORIGIN3)
			to->origin[2] = MSG_ReadCoord (&net_message);
	}

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);
	if (bits & U_SOUND16)
		to->sound = MSG_ReadShort (&net_message);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (&net_message);
	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);

	if (bits & U_SCALE)
		to->scale = MSG_ReadByte (&net_message) * (1.0 / 127) - 1.0;

	if (bits & U_LIGHTING)
	{	// up to three 10 bit indexes into the CS_JUNIORS light list, +1, 0 terminates
		lights = MSG_ReadLong (&net_message);
		to->model_lighting.num_dir_lights = 0;
		for (i=0 ; i<MAX_MODEL_DIR_LIGHTS ; i++)
		{
			index = (lights >> (i*10)) & 0x3ff;
			if (!index)
				break;
			index--;
			light = &cl_modellights[index];

			to->model_lighting.light_indexes[i] = index;
			VectorCopy (light->origin, to->model_lighting.light_orgs[i]);
			VectorCopy (light->color, to->model_lighting.light_colors[i]);
			to->model_lighting.light_intensities[i] = light->intensity;
			to->model_lighting.light_styles[i] = light->style;
			to->model_lighting.num_dir_lights++;
		}
	}
}
#else
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int number, int bits)
{
	// set everything to the state we are delta'ing from
	*to = *from;

	VectorCopy (from->origin, to->old_origin);
	to->number = number;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte (&net_message);
	if (bits & U_MODEL2)
		to->modelindex2 = MSG_ReadByte (&net_message);
	if (bits & U_MODEL3)
		to->modelindex3 = MSG_ReadByte (&net_message);
	if (bits & U_MODEL4)
		to->modelindex4 = MSG_ReadByte (&net_message);
		
	if (bits & U_FRAME8)
		to->frame = MSG_ReadByte (&net_message);
	if (bits & U_FRAME16)
		to->frame = MSG_ReadShort (&net_message);

	if ((bits & U_SKIN8) && (bits & U_SKIN16))		//used for laser colors
		to->skinnum = MSG_ReadLong(&net_message);
	else if (bits & U_SKIN8)
		to->skinnum = MSG_ReadByte(&net_message);
	else if (bits & U_SKIN16)
		to->skinnum = MSG_ReadShort(&net_message);

	if ( (bits & (U_EFFECTS8|U_EFFECTS16)) == (U_EFFECTS8|U_EFFECTS16) )
		to->effects = MSG_ReadLong(&net_message);
	else if (bits & U_EFFECTS8)
		to->effects = MSG_ReadByte(&net_message);
	else if (bits & U_EFFECTS16)
		to->effects = MSG_ReadShort(&net_message);

	if ( (bits & (U_RENDERFX8|U_RENDERFX16)) == (U_RENDERFX8|U_RENDERFX16) )
		to->renderfx = MSG_ReadLong(&net_message);
	else if (bits & U_RENDERFX8)
		to->renderfx = MSG_ReadByte(&net_message);
	else if (bits & U_RENDERFX16)
		to->renderfx = MSG_ReadShort(&net_message);

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord (&net_message);
	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord (&net_message);
		
	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle(&net_message);
	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle(&net_message);

	if (bits & U_OLDORIGIN)
		MSG_ReadPos (&net_message, to->old_origin);

	if (bits & U_SOUND)
		to->sound = MSG_ReadByte (&net_message);

	if (bits & U_EVENT)
		to->event = MSG_ReadByte (&net_message);
	else
		to->event = 0;

	if (bits & U_SOLID)
		to->solid = MSG_ReadShort (&net_message);
}
#endif

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity (frame_t *frame, int newnum, entity_state_t *old, int bits)
{
	centity_t	*ent;
	entity_state_t	*state;

	ent = &cl_entities[newnum];

	state = &cl_parse_entities[cl.parse_entities & (MAX_PARSE_ENTITIES-1)];
	cl.parse_entities++;
	frame->num_entities++;

	CL_ParseDelta (old, state, newnum, bits);

	// some data changes will force no lerping
	if (state->modelindex != ent->current.modelindex
#ifdef KINGPIN_ENGINE
		|| (state->num_parts > 0 && memcmp (state->model_parts, ent->current.model_parts, sizeof(state->model_parts)))
#else
		|| state->modelindex2 != ent->current.modelindex2
		|| state->modelindex3 != ent->current.modelindex3
		|| state->modelindex4 != ent->current.modelindex4
#endif
		|| abs(state->origin[0] - ent->current.origin[0]) > 512
		|| abs(state->origin[1] - ent->current.origin[1]) > 512
		|| abs(state->origin[2] - ent->current.origin[2]) > 512
		|| state->event == EV_PLAYER_TELEPORT
		|| state->event == EV_OTHER_TELEPORT
		)
	{
		ent->serverframe = -99;
	}

	if (ent->serverframe != cl.frame.serverframe - 1)
	{	// wasn't in last update, so initialize some things
		ent->trailcount = 1024;		// for diminishing rocket / grenade trails
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		if (state->event == EV_OTHER_TELEPORT)
		{
			VectorCopy (state->origin, ent->prev.origin);
			VectorCopy (state->origin, ent->lerp_origin);
		}
		else
		{
			VectorCopy (state->old_origin, ent->prev.origin);
			VectorCopy (state->old_origin, ent->lerp_origin);
		}
	}
	else
	{	// shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->serverframe = cl.frame.serverframe;
	ent->current = *state;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities (frame_t *oldframe, frame_t *newframe)
{
	int			newnum;
	int			bits;
	entity_state_t	*oldstate;
	int			oldindex, oldnum;

	newframe->parse_entities = cl.parse_entities;
	newframe->num_entities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	if (!oldframe)
		oldnum = 99999;
	else
	{
		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}

	while (1)
	{
		newnum = CL_ParseEntityBits (&bits);
		if (newnum >= MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: bad number:%i", newnum);

		if (net_message.readcount > net_message.cursize)
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");

		if (!newnum)
			break;

		while (oldnum < newnum)
		{	// one or more entities from the old packet are unchanged
			if (cl_shownet->value == 3)
				Com_Printf ("   unchanged: %i\n", oldnum);
			CL_DeltaEntity (newframe, oldnum, oldstate, 0);
			
			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
		}

		if (bits & U_REMOVE)
		{	// the entity present in oldframe is not in the current frame
			if (cl_shownet->value == 3)
				Com_Printf ("   remove: %i\n", newnum);
			if (oldnum != newnum)
				Com_Printf ("U_REMOVE: oldnum != newnum\n");

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum == newnum)
		{	// delta from previous state
			if (cl_shownet->value == 3)
				Com_Printf ("   delta: %i\n", newnum);
			CL_DeltaEntity (newframe, newnum, oldstate, bits);

			oldindex++;

			if (oldindex >= oldframe->num_entities)
				oldnum = 99999;
			else
			{
				oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
				oldnum = oldstate->number;
			}
			continue;
		}

		if (oldnum > newnum)
		{	// delta from baseline
			if (cl_shownet->value == 3)
				Com_Printf ("   baseline: %i\n", newnum);
			CL_DeltaEntity (newframe, newnum, &cl_entities[newnum].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while (oldnum != 99999)
	{	// one or more entities from the old packet are unchanged
		if (cl_shownet->value == 3)
			Com_Printf ("   unchanged: %i\n", oldnum);
		CL_DeltaEntity (newframe, oldnum, oldstate, 0);
		
		oldindex++;

		if (oldindex >= oldframe->num_entities)
			oldnum = 99999;
		else
		{
			oldstate = &cl_parse_entities[(oldframe->parse_entities+oldindex) & (MAX_PARSE_ENTITIES-1)];
			oldnum = oldstate->number;
		}
	}
}



/*
===================
CL_ParsePlayerstate
===================
*/
void CL_ParsePlayerstate (frame_t *oldframe, frame_t *newframe)
{
	int			flags;
	player_state_t	*state;
	int			i;
	int			statbits;

	state = &newframe->playerstate;

	// clear to old value before delta parsing
	if (oldframe)
		*state = oldframe->playerstate;
	else
		memset (state, 0, sizeof(*state));

	flags = MSG_ReadShort (&net_message);

	//
	// parse the pmove_state_t
	//
	if (flags & PS_M_TYPE)
		state->pmove.pm_type = MSG_ReadByte (&net_message);

	if (flags & PS_M_ORIGIN)
	{
		state->pmove.origin[0] = MSG_ReadShort (&net_message);
		state->pmove.origin[1] = MSG_ReadShort (&net_message);
		state->pmove.origin[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_VELOCITY)
	{
		state->pmove.velocity[0] = MSG_ReadShort (&net_message);
		state->pmove.velocity[1] = MSG_ReadShort (&net_message);
		state->pmove.velocity[2] = MSG_ReadShort (&net_message);
	}

	if (flags & PS_M_TIME)
		state->pmove.pm_time = MSG_ReadByte (&net_message);

	if (flags & PS_M_FLAGS)
		state->pmove.pm_flags = MSG_ReadByte (&net_message);

	if (flags & PS_M_GRAVITY)
		state->pmove.gravity = MSG_ReadShort (&net_message);

	if (flags & PS_M_DELTA_ANGLES)
	{
		state->pmove.delta_angles[0] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[1] = MSG_ReadShort (&net_message);
		state->pmove.delta_angles[2] = MSG_ReadShort (&net_message);
	}

	if (cl.attractloop)
		state->pmove.pm_type = PM_FREEZE;		// demo playback

	//
	// parse the rest of the player_state_t
	//
	if (flags & PS_VIEWOFFSET)
	{
		state->viewoffset[0] = MSG_ReadChar (&net_message) * 0.25;
		state->viewoffset[1] = MSG_ReadChar (&net_message) * 0.25;
#ifdef KINGPIN_ENGINE
		state->viewoffset[2] = MSG_ReadAngle16 (&net_message);
#else
		state->viewoffset[2] = MSG_ReadChar (&net_message) * 0.25;
#endif
	}

	if (flags & PS_VIEWANGLES)
	{
		state->viewangles[0] = MSG_ReadAngle16 (&net_message);
		state->viewangles[1] = MSG_ReadAngle16 (&net_message);
		state->viewangles[2] = MSG_ReadAngle16 (&net_message);
	}

	if (flags & PS_KICKANGLES)
	{
		state->kick_angles[0] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[1] = MSG_ReadChar (&net_message) * 0.25;
		state->kick_angles[2] = MSG_ReadChar (&net_message) * 0.25;
	}

	if (flags & PS_WEAPONINDEX)
	{
		state->gunindex = MSG_ReadByte (&net_message);
#ifdef KINGPIN_ENGINE
		MSG_ReadDeltaModelParts (&net_message, state->model_parts);
#endif
	}

	if (flags & PS_WEAPONFRAME)
	{
		state->gunframe = MSG_ReadByte (&net_message);
		state->gunoffset[0] = MSG_ReadChar (&net_message)*0.25;
		state->gunoffset[1] = MSG_ReadChar (&net_message)*0.25;
		state->gunoffset[2] = MSG_ReadChar (&net_message)*0.25;
		state->gunangles[0] = MSG_ReadChar (&net_message)*0.25;
		state->gunangles[1] = MSG_ReadChar (&net_message)*0.25;
		state->gunangles[2] = MSG_ReadChar (&net_message)*0.25;
	}

	if (flags & PS_BLEND)
	{
		state->blend[0] = MSG_ReadByte (&net_message)/255.0;
		state->blend[1] = MSG_ReadByte (&net_message)/255.0;
		state->blend[2] = MSG_ReadByte (&net_message)/255.0;
		state->blend[3] = MSG_ReadByte (&net_message)/255.0;
	}

	if (flags & PS_FOV)
		state->fov = MSG_ReadByte (&net_message);

	if (flags & PS_RDFLAGS)
		state->rdflags = MSG_ReadByte (&net_message);

	// parse stats
	statbits = MSG_ReadLong (&net_message);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			state->stats[i] = MSG_ReadShort(&net_message);
}


/*
==================
CL_FireEntityEvents

==================
*/
void CL_FireEntityEvents (frame_t *frame)
{
	entity_state_t		*s1;
	int					pnum, num;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		num = (frame->parse_entities + pnum)&(MAX_PARSE_ENTITIES-1);
		s1 = &cl_parse_entities[num];
		if (s1->event)
			CL_EntityEvent (s1);

		// EF_TELEPORTER acts like an event, but is not cleared each frame
		if (s1->effects & EF_TELEPORTER)
			CL_TeleporterParticles (s1);
	}
}


/*
================
CL_ParseFrame
================
*/
void CL_ParseFrame (void)
{
	int			cmd;
	int			len;
	frame_t		*old;

	memset (&cl.frame, 0, sizeof(cl.frame));

#if 0
	CL_ClearProjectiles(); // clear projectiles for new frame
#endif

	cl.frame.serverframe = MSG_ReadLong (&net_message);
	cl.frame.deltaframe = MSG_ReadLong (&net_message);
	cl.frame.servertime = cl.frame.serverframe*100;

	// BIG HACK to let old demos continue to work
	if (cls.serverProtocol != 26)
		cl.surpressCount = MSG_ReadByte (&net_message);

	if (cl_shownet->value == 3)
		Com_Printf ("   frame:%i  delta:%i\n", cl.frame.serverframe,
		cl.frame.deltaframe);

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if (cl.frame.deltaframe <= 0)
	{
		cl.frame.valid = true;		// uncompressed frame
		old = NULL;
		cls.demowaiting = false;	// we can start recording now
	}
	else
	{
		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if (!old->valid)
		{	// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		}
		if (old->serverframe != cl.frame.deltaframe)
		{	// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Printf ("Delta frame too old.\n");
		}
		else if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES-128)
		{
			Com_Printf ("Delta parse_entities too old.\n");
		}
		else
			cl.frame.valid = true;	// valid delta parse
	}

	// clamp time 
	if (cl.time > cl.frame.servertime)
		cl.time = cl.frame.servertime;
	else if (cl.time < cl.frame.servertime - 100)
		cl.time = cl.frame.servertime - 100;

	// read areabits
	len = MSG_ReadByte (&net_message);
	MSG_ReadData (&net_message, &cl.frame.areabits, len);

	// read playerinfo
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_playerinfo)
		Com_Error (ERR_DROP, "CL_ParseFrame: not playerinfo");
	CL_ParsePlayerstate (old, &cl.frame);

	// read packet entities
	cmd = MSG_ReadByte (&net_message);
	SHOWNET(svc_strings[cmd]);
	if (cmd != svc_packetentities)
		Com_Error (ERR_DROP, "CL_ParseFrame: not packetentities");
	CL_ParsePacketEntities (old, &cl.frame);

#if 0
	if (cmd == svc_packetentities2)
		CL_ParseProjectiles();
#endif

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.serverframe & UPDATE_MASK] = cl.frame;

	if (cl.frame.valid)
	{
		// getting a valid frame message ends the connection process
		if (cls.state != ca_active)
		{
			cls.state = ca_active;
			cl.force_refdef = true;
			cl.predicted_origin[0] = cl.frame.playerstate.pmove.origin[0]*0.125;
			cl.predicted_origin[1] = cl.frame.playerstate.pmove.origin[1]*0.125;
			cl.predicted_origin[2] = cl.frame.playerstate.pmove.origin[2]*0.125;
			VectorCopy (cl.frame.playerstate.viewangles, cl.predicted_angles);
			if (cls.disable_servercount != cl.servercount
				&& cl.refresh_prepped)
				SCR_EndLoadingPlaque ();	// get rid of loading plaque
		}
		cl.sound_prepped = true;	// can start mixing ambient sounds
	
		// fire entity events
		CL_FireEntityEvents (&cl.frame);
		CL_CheckPredictionError ();
	}
}

/*
==========================================================================

INTERPOLATE BETWEEN FRAMES TO GET RENDERING PARMS

==========================================================================
*/

struct model_s *S_RegisterSexedModel (entity_state_t *ent, char *base)
{
	int				n;
	char			*p;
	struct model_s	*mdl;
	char			model[MAX_QPATH];
	char			buffer[MAX_QPATH];

	// determine what model the client is using
	model[0] = 0;
	n = CS_PLAYERSKINS + ent->number - 1;
	if (cl.configstrings[n][0])
	{
		p = strchr(cl.configstrings[n], '\\');
		if (p)
		{
			p += 1;
			strcpy(model, p);
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}
	// if we can't figure it out, they're male
	if (!model[0])
		strcpy(model, "male");

	Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", model, base+1);
	mdl = re.RegisterModel(buffer);
	if (!mdl) {
		// not found, try default weapon model
		Com_sprintf (buffer, sizeof(buffer), "players/%s/weapon.md2", model);
		mdl = re.RegisterModel(buffer);
		if (!mdl) {
			// no, revert to the male model
			Com_sprintf (buffer, sizeof(buffer), "players/%s/%s", "male", base+1);
			mdl = re.RegisterModel(buffer);
			if (!mdl) {
				// last try, default male weapon.md2
				Com_sprintf (buffer, sizeof(buffer), "players/male/weapon.md2");
				mdl = re.RegisterModel(buffer);
			}
		} 
	}

	return mdl;
}

// PMM - used in shell code 
extern int Developer_searchpath (int who);
// pmm
#ifdef KINGPIN_ENGINE
/*
==========================================================================

KINGPIN PACKET ENTITIES

Ported from kingpin.exe CL_AddPacketEntities @004056c0,
CL_AddViewWeapon @00406aa0 and the model helper @00405560.

==========================================================================
*/

vec3_t		cl_vehicle_angles;			// angles of the local player's vehicle model
model_lighting_t	cl_playerlighting;	// the local player's lighting, used for the view weapon

// the local player carries a flamethrower (set up for the view weapon)
qboolean		cl_playerflame;
entity_t		*cl_playerflame_ent;
entity_state_t	*cl_playerflame_state;
static entity_t	cl_playerflame_copy;

/*
===============
CL_GetModel

Sets ent->model for a model index, registering the model and the skins
listed for it in CS_MODELSKINS (3 characters each) on first use.
===============
*/
void CL_GetModel (entity_t *ent, int modelindex)
{
	char	*skins;
	char	skin[32];
	int		i, count;

	if (!cl.model_draw[modelindex] && cl.configstrings[CS_MODELS+modelindex][0])
	{
		cl.model_draw[modelindex] = re.RegisterModel (cl.configstrings[CS_MODELS+modelindex]);

		skins = cl.configstrings[CS_MODELSKINS+modelindex];
		if (cl.model_draw[modelindex] && skins[0])
		{
			count = strlen (skins) / 3;
			for (i=0 ; i<count ; i++, skins+=3)
			{
				strncpy (skin, skins, 3);
				skin[3] = 0;
				re.RegisterModelSkin (cl.model_draw[modelindex], skin);
				strcat (skin, "p1");
				re.RegisterModelSkin (cl.model_draw[modelindex], skin);
				skin[3] = 0;
				strcat (skin, "p2");
				re.RegisterModelSkin (cl.model_draw[modelindex], skin);
			}
		}
	}

	ent->model = cl.model_draw[modelindex];
}

/*
===============
CL_AddVehicleTilt

Rolls and pitches the local player's vehicle model: a spring towards a
target that follows steering (bike) or turning and random bumps
(hovercar). Constants and quirks as in kingpin.exe.
===============
*/
static int		vehicle_tilttime = -999;
static vec3_t	vehicle_target;
static vec3_t	vehicle_cur;
static vec3_t	vehicle_vel;
static vec3_t	vehicle_lastviewangles;

static void CL_AddVehicleTilt (entity_t *ent, int pm_type)
{
	int		i;
	float	maxtilt, turn, f;
	float	target, diff, speed, accel, limit, old;
	vec3_t	v;

	ent->angles[PITCH] = cl.viewangles[PITCH];

	if (pm_type == PM_HOVERCAR_GROUND)
		maxtilt = 3.5;
	else if (pm_type == PM_BIKE)
	{
		for (i=0 ; i<3 ; i++)
			v[i] = cl.frame.playerstate.pmove.velocity[i] * 0.125;
		f = VectorLength (v) * (1.0/600);
		if (f > 1.0)
			f = 1.0;
		maxtilt = f * 56;
	}
	else
		maxtilt = 7.0;

	if (vehicle_tilttime < -998)
		vehicle_tilttime = cl.time;
	else if (pm_type == PM_BIKE)
	{	// lean into the steering direction
		vehicle_tilttime = 0;
		if (cl_moveleft_held && !cl_moveright_held)
			vehicle_target[ROLL] = maxtilt;
		else if (cl_moveright_held && !cl_moveleft_held)
			vehicle_target[ROLL] = -maxtilt;
		else
			vehicle_target[ROLL] = 0;
	}
	else
	{
		turn = AngleDiff (cl.viewangles[YAW], vehicle_lastviewangles[YAW]);
		if (fabs(turn) <= cls.frametime * 35)
		{	// wobble a little while flying straight
			if (vehicle_tilttime < cl.time - 500)
			{
				for (i=0 ; i<3 ; i++)
				{
					int r = rand();
					vehicle_target[i] = ((r * (1.0/32768) + r * (1.0/32768)) - 1.0) * 1.5;
				}
				vehicle_tilttime = cl.time;
			}
		}
		else
		{	// lean into the turn
			f = fabs(turn) / (cls.frametime * 90);
			vehicle_tilttime = 0;
			if (turn <= 0)
				vehicle_target[ROLL] = f * maxtilt;
			else
				vehicle_target[ROLL] = -maxtilt * f;
		}
	}

	for (i=0 ; i<3 ; i++)
	{
		target = vehicle_target[i] + cl.vehicle_groundangles[i];
		diff = target - vehicle_cur[i];

		if (pm_type == PM_BIKE)
		{
			speed = fabs(diff) * 0.5 + 1.0;
			speed += speed;
			if (speed > 60)
				speed = 60;
			if (i == 0)
				speed += speed;
		}
		else if (!vehicle_tilttime && i == ROLL)
			speed = maxtilt;
		else
		{
			f = fabs(vehicle_cur[i] - cl.vehicle_groundangles[i]);
			if (f <= maxtilt * 0.25)
			{
				if (f >= 3 || fabs(vehicle_cur[i]) >= 1.5)
					speed = 1.0;
				else
					speed = 0.3;
			}
			else
				speed = f / maxtilt * 5;
		}

		accel = speed * ((target < 0) + 1) * cls.frametime * 7.5;
		limit = (speed == 1.0) ? 1.0 : (speed - 1.0) * 0.5 + 1.0;

		if (diff > 0)
		{
			if (diff <= vehicle_vel[i] + vehicle_vel[i])
				vehicle_vel[i] = diff + diff;
			else
			{
				vehicle_vel[i] += accel;
				if (vehicle_vel[i] > limit * 10)
					vehicle_vel[i] = ((speed - 1.0) * 0.5 + 1.0) * 10;
				if (vehicle_vel[i] + vehicle_vel[i] < diff)
					vehicle_vel[i] = diff + diff;
			}
		}
		else if (diff < 0)
		{
			if (vehicle_vel[i] + vehicle_vel[i] <= diff)
				vehicle_vel[i] = diff + diff;
			else
			{
				vehicle_vel[i] -= accel;
				if (vehicle_vel[i] < limit * -10)
					vehicle_vel[i] = limit * -10;
				if (diff < vehicle_vel[i] + vehicle_vel[i])
					vehicle_vel[i] = diff + diff;
			}
		}
		else
		{	// on target: damp, stop when the sign flips
			old = vehicle_vel[i];
			vehicle_vel[i] -= cls.frametime * vehicle_vel[i];
			if ((vehicle_vel[i] < 0) != (old < 0))
				vehicle_vel[i] = 0;
		}

		if (pm_type != PM_BIKE || fabs(cls.frametime * vehicle_vel[i]) <= fabs(diff))
			vehicle_cur[i] += cls.frametime * vehicle_vel[i];
		else
			vehicle_cur[i] = target;

		ent->angles[i] += vehicle_cur[i];
		vehicle_target[i] = target - cl.vehicle_groundangles[i];
	}

	VectorCopy (cl.viewangles, vehicle_lastviewangles);
	VectorCopy (ent->angles, cl_vehicle_angles);
}

/*
===============
CL_AddPacketEntities
===============
*/
void CL_AddPacketEntities (frame_t *frame)
{
	entity_t			ent;
	entity_state_t		*s1;
	float				autorotate;
	int					i, pnum;
	centity_t			*cent;
	int					autoanim;
	clientinfo_t		*ci;
	unsigned int		effects, renderfx;
	int					renderfx2;
	qboolean			colorshell;
	vec3_t				forward, right, up, bagofs, start;
	model_part_t		*part;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(cl.time/10);

	// brush models can auto animate their frames
	autoanim = 2*cl.time/1000;

	memset (&ent, 0, sizeof(ent));

	CL_GCFlames ();

	cl_playerflame = false;
	cl_playerflame_ent = NULL;
	cl_playerflame_state = NULL;

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;
		renderfx2 = s1->renderfx2;

		// only parental locked clients see what is left after a gib
		if ((renderfx2 & RF2_ONLY_PARENTAL_LOCKED)
			&& !(cl_parental_lock->value && !cl_parental_override->value))
			continue;

		// alpha travels in the effects field
		if (renderfx2 & RF2_PASSALPHA)
		{
			if (!effects)
				ent.alpha = s1->alpha;
			else
			{
				ent.alpha = 1.0 - (effects - 1) * (1.0/254);
				s1->alpha = ent.alpha;
			}
			effects = 0;
			s1->effects = 0;
		}
		else
			ent.alpha = 0;

		memset (ent.skinarray, 0, sizeof(ent.skinarray));
		ent.use_skinarray = 0;

		// set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}
		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}

		if (effects & EF_REFL)
			renderfx |= RF_REFL_MAP;

		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0 - cl.lerpfrac;
		ent.scale = s1->scale;

		if (renderfx & (RF_FRAMELERP|RF_BEAM))
		{	// step origin discretely, because the frames
			// do the animation properly
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);
		}
		else
		{	// interpolate origin
			for (i=0 ; i<3 ; i++)
			{
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac *
					(cent->current.origin[i] - cent->prev.origin[i]);
			}
		}

		ent.model_lighting = s1->model_lighting;

		if (renderfx == RF_TRANSLUCENT)
			ent.alpha = 0.7;

		colorshell = (effects & EF_COLOR_SHELL) != 0;
		// render effects (fullbright, translucent, etc)
		if (colorshell)
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;
		ent.flags2 = renderfx2;

		// calculate angles
		if (effects & EF_ROTATE)
		{	// some bonus items auto-rotate
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		else if (effects & EF_SPINNINGLIGHTS)
		{
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time/2) + s1->angles[1];
			ent.angles[2] = 180;
			AngleVectors (ent.angles, forward, NULL, NULL);
			VectorMA (ent.origin, 64, forward, start);
			V_AddLight (start, 100, 1, 0, 0);
		}
		else if (effects & EF_FLASHLIGHT)
		{	// angles are left as they are
			V_AddLight (ent.origin, 300, 0.8, 0.6, 0.6);
			V_AddLight (ent.origin, 100, 1, 1, 1);
		}
		else
		{	// interpolate angles
			for (i=0 ; i<3 ; i++)
				ent.angles[i] = LerpAngle (cent->prev.angles[i], cent->current.angles[i], cl.lerpfrac);
		}

		if (s1->number == cl.playernum+1)
		{
			if ((s1->renderfx2 & RF2_FLAMETHROWER) || cl_flameactive[s1->number])
			{
				cl_playerflame_copy = ent;
				cl_playerflame_ent = &cl_playerflame_copy;
				cl_playerflame = true;
				cl_playerflame_state = s1;
			}
			else
				cl_playerflame = false;

			cl_playerlighting = s1->model_lighting;

			i = cl.frame.playerstate.pmove.pm_type;
			if (i == PM_HOVERCAR || i == PM_HOVERCAR_GROUND || i == PM_BIKE)
				CL_AddVehicleTilt (&ent, i);

			if (!(cl.frame.playerstate.pmove.pm_flags & PMF_CHASECAM))
			{
				ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
				// FIXME: still pass to refresh

				if (effects & EF_FLAG1)
					V_AddLight (ent.origin, 225, 1.0, 0.1, 0.1);
				else if (effects & EF_FLAG2)
					V_AddLight (ent.origin, 225, 0.1, 0.1, 1.0);
				continue;
			}
		}
		else
		{
			if ((s1->renderfx2 & RF2_FLAMETHROWER) || cl_flameactive[s1->number])
				CL_FlameEffects (&ent, s1, 0);

			if (((s1->renderfx2 & RF2_FLAMESHOOTER) && !(s1->renderfx2 & RF2_PASSLIGHT))
				|| cl_flameshooteractive[s1->number])
			{
				if (s1->renderfx2 & RF2_FLAMESHOOTER_MOD)
					CL_FlameShooter (&ent, s1, ent.origin);
				else
				{
					VectorCopy (ent.origin, start);
					start[2] += 20;
					CL_FlameShooter (&ent, s1, start);
				}
			}
		}

		if (renderfx & RF_BEAM)
		{	// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.3;
			ent.skinnum = (s1->skinnum >> ((rand() % 4)*8)) & 0xff;
			ent.model = NULL;
		}
		else if (s1->modelindex == 255)
		{	// use custom player skin: one entity per body part
			ci = &cl.clientinfo[s1->skinnum & 0xff];
			i = 0;
			while (ci->model[i])
			{
				ent.skinnum = 0;
				ent.skin = ci->skin[i];
				ent.model = ci->model[i];
				if ((i < 3 && !ent.skin) || !ent.model)
				{
					if (i >= 3)
						break;
					ent.skin = cl.baseclientinfo.skin[i];
					ent.model = cl.baseclientinfo.model[i];
				}

				if (!cl_parental_lock->value)
					ent.invisible_objects = s1->model_parts[i].invisible_objects;
				else
					ent.invisible_objects = 0;
				V_AddEntity (&ent);

				i++;
				if (i > 2 && s1->model_parts[3].modelindex != 255
					&& s1->model_parts[0].invisible_objects)
					break;
			}

			ent.skin = NULL;
			ent.invisible_objects = 0;

			if (s1->model_parts[3].modelindex == 255)
			{	// view weapon model
				i = s1->skinnum >> 8;
				if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				if (!ent.model)
				{
					if (i != 0)
						ent.model = ci->weaponmodel[0];
					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
				V_AddEntity (&ent);
			}

			if (s1->renderfx2 & RF2_MONEYBAG)
			{	// teamplay: money bag on the player's back
				ent.model = re.RegisterModel ("models/pu_icon/money/money_lg.md2");
				ent.skinnum = 0;
				ent.frame = 0;
				ent.oldframe = 0;

				AngleVectors (s1->angles, forward, right, up);
				VectorScale (forward, -22, bagofs);
				VectorMA (bagofs, 14, up, bagofs);
				VectorMA (bagofs, -4, right, bagofs);

				VectorAdd (ent.origin, bagofs, ent.origin);
				VectorAdd (ent.oldorigin, bagofs, ent.oldorigin);
				ent.angles[PITCH] += 30;
				V_AddEntity (&ent);
				ent.angles[PITCH] -= 30;
				VectorSubtract (ent.origin, bagofs, ent.origin);
				VectorSubtract (ent.oldorigin, bagofs, ent.oldorigin);

				ent.frame = s1->frame;
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
			}
		}
		else
		{
			ent.skinnum = s1->skinnum;
			ent.skin = NULL;
			CL_GetModel (&ent, s1->modelindex);
		}

		if (!s1->modelindex && !s1->num_parts)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30;
		}

		if (effects & EF_PLASMA)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6;
		}

		// add to refresh list
		if (s1->modelindex && s1->modelindex != 255)
			V_AddEntity (&ent);

		// .mdx parts
		if (s1->num_parts && s1->modelindex != 255)
		{
			for (i=0, part=s1->model_parts ; i<MAX_MODEL_PARTS ; i++, part++)
			{
				if (!part->modelindex)
					continue;
				ent.invisible_objects = part->invisible_objects;
				CL_GetModel (&ent, part->modelindex);
				memcpy (ent.skinarray, part->skinnum, sizeof(ent.skinarray));
				ent.use_skinarray = 1;
				V_AddEntity (&ent);
			}
		}

		// color shells generate a separate entity for the main model
		if (colorshell)
		{
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 0;

		if (effects & EF_POWERSCREEN)
		{
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		// add automatic particle trails
		if ( (effects&~EF_ROTATE) )
		{
			if (effects & EF_ROCKET)
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
			else if (effects & EF_BLASTER)
			{
				CL_BlasterTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 200, 1, 1, 0);
			}
			else if (effects & EF_FLAMETHROWER)
				CL_FlameTrail (cent, ent.origin);
			else if (effects & EF_HYPERBLASTER)
				V_AddLight (ent.origin, 200, 1, 1, 0);
			else if (effects & EF_GIB)
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			else if (effects & EF_GRENADE)
			{
				CL_GrenadeSmoke (ent.origin);
				V_AddLight (ent.origin, 64, 0.9, 0.8, 0.8);
			}
			else if (effects & EF_FLIES)
				CL_FlyEffect (cent, ent.origin);
			else if (effects & EF_BFG)
			{
				static int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					i = 200;
				}
				else
					i = bfg_lightramp[s1->frame];
				V_AddLight (ent.origin, i, 0, 1, 0);
			}
			else if (effects & EF_TRAP)
			{
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				i = (rand()%100) + 100;
				V_AddLight (ent.origin, i, 1, 0.8, 0.1);
			}
			else if (effects & EF_FLAG1)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1, 0.1, 0.1);
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1, 0.1, 1);
			}
			else if (effects & EF_IONRIPPER)
			{
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5, 0.5);
			}
			else if (effects & EF_BLUEHYPERBLASTER)
				V_AddLight (ent.origin, 255, 0.5, 0.5, 0.5);
			else if (effects & EF_PLASMA)
			{
				if (effects & EF_ANIM_ALLFAST)
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 130, 1, 0.5, 0.5);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}

/*
==============
CL_AddViewWeapon

The view weapon is drawn from its .mdx parts only
==============
*/
void CL_AddViewWeapon (player_state_t *ps, player_state_t *ops)
{
	entity_t	gun;		// view model
	int			i;
	model_part_t	*part;

	// allow the gun to be completely removed
	if (!cl_gun->value)
		return;

	// don't draw gun if in wide angle view
	if (ps->fov > 90)
		return;

	memset (&gun, 0, sizeof(gun));

	if (gun_model)
		gun.model = gun_model;	// development tool
	else
		CL_GetModel (&gun, ps->gunindex);
	if (!gun.model)
		return;

	gun.model_lighting = cl_playerlighting;

	// set up gun position
	for (i=0 ; i<3 ; i++)
	{
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
			+ cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i],
			ps->gunangles[i], cl.lerpfrac);
	}

	if (gun_frame)
	{
		gun.frame = gun_frame;	// development tool
		gun.oldframe = gun_frame;	// development tool
	}
	else
	{
		gun.frame = ps->gunframe;
		if (gun.frame == 0)
			gun.oldframe = 0;	// just changed weapons, don't lerp from old
		else
			gun.oldframe = ops->gunframe;
	}

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
	gun.flags2 |= RF2_DIR_LIGHTS;
	if (ps->rdflags & RDF_NOLERP)
		gun.flags2 |= RF2_8;			// TODO: meaning of this bit for ref_gl
	gun.backlerp = 1.0 - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	// don't lerp at all

	for (i=0, part=ps->model_parts ; i<MAX_MODEL_PARTS ; i++, part++)
	{
		if (!part->modelindex)
			continue;
		gun.invisible_objects = part->invisible_objects;
		CL_GetModel (&gun, part->modelindex);
		memcpy (gun.skinarray, part->skinnum, sizeof(gun.skinarray));
		gun.use_skinarray = 1;
		V_AddEntity (&gun);
	}
}


/*
===============
CL_ChaseCam

Third person camera (PMF_CHASECAM): trails behind the view, eases towards
its wanted offset with cam_speed and looks at a point in front of the
player (kingpin.exe @00406c90)
===============
*/
static void CL_ChaseCam (player_state_t *ps)
{
	static int		chase_time = -1;
	static vec3_t	chase_offset;
	float			dist, d;
	vec3_t			forward, up, angles;
	vec3_t			wanted, delta, dir, end, target;
	trace_t			tr;
	int				dt;
	static vec3_t	mins = {-8, -8, -8};
	static vec3_t	maxs = {8, 8, 8};

	dist = cam_dist->value;

	if (ps->pmove.pm_type == PM_HOVERCAR_GROUND || ps->pmove.pm_type == PM_BIKE)
	{
		VectorCopy (cl_vehicle_angles, angles);
		AngleVectors (angles, forward, NULL, up);
	}
	else
	{
		VectorCopy (cl.v_forward, forward);
		VectorCopy (cl.v_up, up);
	}

	dt = cl.time - chase_time;
	if (chase_time == -1 || cl.time < chase_time || dt > 1000)
	{	// snap into place
		chase_time = cl.time;
		VectorScale (forward, -dist, chase_offset);
		VectorScale (forward, -1, dir);
	}
	else
	{
		d = dist;
		if (d > 0)
			d = -d;
		VectorScale (forward, d, wanted);
		VectorSubtract (wanted, chase_offset, delta);
		VectorMA (chase_offset, dt * 0.001 * cam_speed->value, delta, chase_offset);
		VectorNormalize2 (chase_offset, dir);
	}

	if (ps->pmove.pm_type == PM_BIKE)
		cl.refdef.vieworg[2] += 8;

	VectorMA (cl.refdef.vieworg, dist, dir, end);
	end[2] = cam_zofs->value + end[2] - 12;

	tr = CL_PMTraceEx (cl.refdef.vieworg, mins, maxs, end, 0); // world only

	// look at a point in front of the player
	VectorMA (cl.refdef.vieworg, 64, forward, target);
	VectorMA (target, -12, up, target);
	VectorSubtract (target, tr.endpos, delta);
	VectorNormalize (delta);
	vectoangles (delta, cl.refdef.viewangles);

	VectorCopy (tr.endpos, cl.refdef.vieworg);
	chase_time = cl.time;
}

/*
===============
CL_CalcViewValues

Sets cl.refdef view values (kingpin.exe @00406c90)
===============
*/
void CL_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or involid
	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame, or the game
	// switched cameras
	if ( fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*8
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*8
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*8
		|| ps->stats[STAT_SWITCH_CAMERA])
		ops = ps;		// don't interpolate

	lerp = cl.lerpfrac;

	// calculate the origin
	if (!cl_predict->value || (ps->pmove.pm_flags & (PMF_NO_PREDICTION|PMF_CHASECAM)))
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125 + ops->viewoffset[i]
				+ lerp * (ps->pmove.origin[i]*0.125 + ps->viewoffset[i]
				- (ops->pmove.origin[i]*0.125 + ops->viewoffset[i]) );
	}
	else
	{	// use predicted values
		unsigned	delta;

		backlerp = 1.0 - lerp;
		for (i=0 ; i<3 ; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i]
				+ cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01;
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if ( ps->pmove.pm_type < PM_DEAD )
	{	// use predicted values
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = LerpAngle (ops->viewangles[i], ps->viewangles[i], lerp);
	}

	for (i=0 ; i<3 ; i++)
		cl.refdef.viewangles[i] += LerpAngle (ops->kick_angles[i], ps->kick_angles[i], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	if (cl_playerflame && cl_playerflame_ent && ps->pmove.pm_type < PM_DEAD)
		CL_FlameEffects (cl_playerflame_ent, cl_playerflame_state, 1);

	if (!(ps->pmove.pm_flags & PMF_CHASECAM))
	{
		if (ps->pmove.pm_type == PM_HOVERCAR_GROUND || ps->pmove.pm_type == PM_BIKE)
		{	// the view follows the vehicle
			cl.refdef.viewangles[0] = ps->kick_angles[0] + cl_vehicle_angles[0];
			cl.refdef.viewangles[1] = ps->kick_angles[1] + cl_vehicle_angles[1];
			cl.refdef.viewangles[2] = ps->kick_angles[2] - cl_vehicle_angles[2];
			AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);
			cl.refdef.vieworg[2] -= ps->viewoffset[2];
			VectorMA (cl.refdef.vieworg, ps->viewoffset[2], cl.v_up, cl.refdef.vieworg);
		}
	}
	else
		CL_ChaseCam (ps);

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	for (i=0 ; i<4 ; i++)
		cl.refdef.blend[i] = ps->blend[i];

	// add the weapon
	if (!(ps->pmove.pm_flags & PMF_CHASECAM))
		CL_AddViewWeapon (ps, ops);
}
#else
/*
===============
CL_AddPacketEntities

===============
*/
void CL_AddPacketEntities (frame_t *frame)
{
	entity_t			ent;
	entity_state_t		*s1;
	float				autorotate;
	int					i;
	int					pnum;
	centity_t			*cent;
	int					autoanim;
	clientinfo_t		*ci;
	unsigned int		effects, renderfx;

	// bonus items rotate at a fixed rate
	autorotate = anglemod(cl.time/10);

	// brush models can auto animate their frames
	autoanim = 2*cl.time/1000;

	memset (&ent, 0, sizeof(ent));

	for (pnum = 0 ; pnum<frame->num_entities ; pnum++)
	{
		s1 = &cl_parse_entities[(frame->parse_entities+pnum)&(MAX_PARSE_ENTITIES-1)];

		cent = &cl_entities[s1->number];

		effects = s1->effects;
		renderfx = s1->renderfx;

			// set frame
		if (effects & EF_ANIM01)
			ent.frame = autoanim & 1;
		else if (effects & EF_ANIM23)
			ent.frame = 2 + (autoanim & 1);
		else if (effects & EF_ANIM_ALL)
			ent.frame = autoanim;
		else if (effects & EF_ANIM_ALLFAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = s1->frame;

		// quad and pent can do different things on client
		if (effects & EF_PENT)
		{
			effects &= ~EF_PENT;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_RED;
		}

		if (effects & EF_QUAD)
		{
			effects &= ~EF_QUAD;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_BLUE;
		}
//======
// PMM
		if (effects & EF_DOUBLE)
		{
			effects &= ~EF_DOUBLE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_DOUBLE;
		}

		if (effects & EF_HALF_DAMAGE)
		{
			effects &= ~EF_HALF_DAMAGE;
			effects |= EF_COLOR_SHELL;
			renderfx |= RF_SHELL_HALF_DAM;
		}
// pmm
//======
		ent.oldframe = cent->prev.frame;
		ent.backlerp = 1.0 - cl.lerpfrac;

		if (renderfx & (RF_FRAMELERP|RF_BEAM))
		{	// step origin discretely, because the frames
			// do the animation properly
			VectorCopy (cent->current.origin, ent.origin);
			VectorCopy (cent->current.old_origin, ent.oldorigin);
		}
		else
		{	// interpolate origin
			for (i=0 ; i<3 ; i++)
			{
				ent.origin[i] = ent.oldorigin[i] = cent->prev.origin[i] + cl.lerpfrac * 
					(cent->current.origin[i] - cent->prev.origin[i]);
			}
		}

		// create a new entity
	
		// tweak the color of beams
		if ( renderfx & RF_BEAM )
		{	// the four beam colors are encoded in 32 bits of skinnum (hack)
			ent.alpha = 0.30;
			ent.skinnum = (s1->skinnum >> ((rand() % 4)*8)) & 0xff;
			ent.model = NULL;
		}
		else
		{
			// set skin
			if (s1->modelindex == 255)
			{	// use custom player skin
				ent.skinnum = 0;
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				ent.skin = ci->skin;
				ent.model = ci->model;
				if (!ent.skin || !ent.model)
				{
					ent.skin = cl.baseclientinfo.skin;
					ent.model = cl.baseclientinfo.model;
				}

//============
//PGM
				if (renderfx & RF_USE_DISGUISE)
				{
					if(!strncmp((char *)ent.skin, "players/male", 12))
					{
						ent.skin = re.RegisterSkin ("players/male/disguise.pcx");
						ent.model = re.RegisterModel ("players/male/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/female", 14))
					{
						ent.skin = re.RegisterSkin ("players/female/disguise.pcx");
						ent.model = re.RegisterModel ("players/female/tris.md2");
					}
					else if(!strncmp((char *)ent.skin, "players/cyborg", 14))
					{
						ent.skin = re.RegisterSkin ("players/cyborg/disguise.pcx");
						ent.model = re.RegisterModel ("players/cyborg/tris.md2");
					}
				}
//PGM
//============
			}
			else
			{
				ent.skinnum = s1->skinnum;
				ent.skin = NULL;
				ent.model = cl.model_draw[s1->modelindex];
			}
		}

		// only used for black hole model right now, FIXME: do better
		if (renderfx == RF_TRANSLUCENT)
			ent.alpha = 0.70;

		// render effects (fullbright, translucent, etc)
		if ((effects & EF_COLOR_SHELL))
			ent.flags = 0;	// renderfx go on color shell entity
		else
			ent.flags = renderfx;

		// calculate angles
		if (effects & EF_ROTATE)
		{	// some bonus items auto-rotate
			ent.angles[0] = 0;
			ent.angles[1] = autorotate;
			ent.angles[2] = 0;
		}
		// RAFAEL
		else if (effects & EF_SPINNINGLIGHTS)
		{
			ent.angles[0] = 0;
			ent.angles[1] = anglemod(cl.time/2) + s1->angles[1];
			ent.angles[2] = 180;
			{
				vec3_t forward;
				vec3_t start;

				AngleVectors (ent.angles, forward, NULL, NULL);
				VectorMA (ent.origin, 64, forward, start);
				V_AddLight (start, 100, 1, 0, 0);
			}
		}
		else
		{	// interpolate angles
			float	a1, a2;

			for (i=0 ; i<3 ; i++)
			{
				a1 = cent->current.angles[i];
				a2 = cent->prev.angles[i];
				ent.angles[i] = LerpAngle (a2, a1, cl.lerpfrac);
			}
		}

		if (s1->number == cl.playernum+1)
		{
			ent.flags |= RF_VIEWERMODEL;	// only draw from mirrors
			// FIXME: still pass to refresh

			if (effects & EF_FLAG1)
				V_AddLight (ent.origin, 225, 1.0, 0.1, 0.1);
			else if (effects & EF_FLAG2)
				V_AddLight (ent.origin, 225, 0.1, 0.1, 1.0);
			else if (effects & EF_TAGTRAIL)						//PGM
				V_AddLight (ent.origin, 225, 1.0, 1.0, 0.0);	//PGM
			else if (effects & EF_TRACKERTRAIL)					//PGM
				V_AddLight (ent.origin, 225, -1.0, -1.0, -1.0);	//PGM

			continue;
		}

		// if set to invisible, skip
		if (!s1->modelindex)
			continue;

		if (effects & EF_BFG)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.30;
		}

		// RAFAEL
		if (effects & EF_PLASMA)
		{
			ent.flags |= RF_TRANSLUCENT;
			ent.alpha = 0.6;
		}

		if (effects & EF_SPHERETRANS)
		{
			ent.flags |= RF_TRANSLUCENT;
			// PMM - *sigh*  yet more EF overloading
			if (effects & EF_TRACKERTRAIL)
				ent.alpha = 0.6;
			else
				ent.alpha = 0.3;
		}
//pmm

		// add to refresh list
		V_AddEntity (&ent);


		// color shells generate a seperate entity for the main model
		if (effects & EF_COLOR_SHELL)
		{
			// PMM - at this point, all of the shells have been handled
			// if we're in the rogue pack, set up the custom mixing, otherwise just
			// keep going
//			if(Developer_searchpath(2) == 2)
//			{
				// all of the solo colors are fine.  we need to catch any of the combinations that look bad
				// (double & half) and turn them into the appropriate color, and make double/quad something special
				if (renderfx & RF_SHELL_HALF_DAM)
				{
					if(Developer_searchpath(2) == 2)
					{
						// ditch the half damage shell if any of red, blue, or double are on
						if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_DOUBLE))
							renderfx &= ~RF_SHELL_HALF_DAM;
					}
				}

				if (renderfx & RF_SHELL_DOUBLE)
				{
					if(Developer_searchpath(2) == 2)
					{
						// lose the yellow shell if we have a red, blue, or green shell
						if (renderfx & (RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_GREEN))
							renderfx &= ~RF_SHELL_DOUBLE;
						// if we have a red shell, turn it to purple by adding blue
						if (renderfx & RF_SHELL_RED)
							renderfx |= RF_SHELL_BLUE;
						// if we have a blue shell (and not a red shell), turn it to cyan by adding green
						else if (renderfx & RF_SHELL_BLUE)
							// go to green if it's on already, otherwise do cyan (flash green)
							if (renderfx & RF_SHELL_GREEN)
								renderfx &= ~RF_SHELL_BLUE;
							else
								renderfx |= RF_SHELL_GREEN;
					}
				}
//			}
			// pmm
			ent.flags = renderfx | RF_TRANSLUCENT;
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		ent.skin = NULL;		// never use a custom skin on others
		ent.skinnum = 0;
		ent.flags = 0;
		ent.alpha = 0;

		// duplicate for linked models
		if (s1->modelindex2)
		{
			if (s1->modelindex2 == 255)
			{	// custom weapon
				ci = &cl.clientinfo[s1->skinnum & 0xff];
				i = (s1->skinnum >> 8); // 0 is default weapon model
				if (!cl_vwep->value || i > MAX_CLIENTWEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				if (!ent.model) {
					if (i != 0)
						ent.model = ci->weaponmodel[0];
					if (!ent.model)
						ent.model = cl.baseclientinfo.weaponmodel[0];
				}
			}
			else
				ent.model = cl.model_draw[s1->modelindex2];

			// PMM - check for the defender sphere shell .. make it translucent
			// replaces the previous version which used the high bit on modelindex2 to determine transparency
			if (!Q_strcasecmp (cl.configstrings[CS_MODELS+(s1->modelindex2)], "models/items/shell/tris.md2"))
			{
				ent.alpha = 0.32;
				ent.flags = RF_TRANSLUCENT;
			}
			// pmm

			V_AddEntity (&ent);

			//PGM - make sure these get reset.
			ent.flags = 0;
			ent.alpha = 0;
			//PGM
		}
		if (s1->modelindex3)
		{
			ent.model = cl.model_draw[s1->modelindex3];
			V_AddEntity (&ent);
		}
		if (s1->modelindex4)
		{
			ent.model = cl.model_draw[s1->modelindex4];
			V_AddEntity (&ent);
		}

		if ( effects & EF_POWERSCREEN )
		{
			ent.model = cl_mod_powerscreen;
			ent.oldframe = 0;
			ent.frame = 0;
			ent.flags |= (RF_TRANSLUCENT | RF_SHELL_GREEN);
			ent.alpha = 0.30;
			V_AddEntity (&ent);
		}

		// add automatic particle trails
		if ( (effects&~EF_ROTATE) )
		{
			if (effects & EF_ROCKET)
			{
				CL_RocketTrail (cent->lerp_origin, ent.origin, cent);
				V_AddLight (ent.origin, 200, 1, 1, 0);
			}
			// PGM - Do not reorder EF_BLASTER and EF_HYPERBLASTER. 
			// EF_BLASTER | EF_TRACKER is a special case for EF_BLASTER2... Cheese!
			else if (effects & EF_BLASTER)
			{
//				CL_BlasterTrail (cent->lerp_origin, ent.origin);
//PGM
				if (effects & EF_TRACKER)	// lame... problematic?
				{
					CL_BlasterTrail2 (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 0, 1, 0);		
				}
				else
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
					V_AddLight (ent.origin, 200, 1, 1, 0);
				}
//PGM
			}
			else if (effects & EF_HYPERBLASTER)
			{
				if (effects & EF_TRACKER)						// PGM	overloaded for blaster2.
					V_AddLight (ent.origin, 200, 0, 1, 0);		// PGM
				else											// PGM
					V_AddLight (ent.origin, 200, 1, 1, 0);
			}
			else if (effects & EF_GIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_GRENADE)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);
			}
			else if (effects & EF_FLIES)
			{
				CL_FlyEffect (cent, ent.origin);
			}
			else if (effects & EF_BFG)
			{
				static int bfg_lightramp[6] = {300, 400, 600, 300, 150, 75};

				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BfgParticles (&ent);
					i = 200;
				}
				else
				{
					i = bfg_lightramp[s1->frame];
				}
				V_AddLight (ent.origin, i, 0, 1, 0);
			}
			// RAFAEL
			else if (effects & EF_TRAP)
			{
				ent.origin[2] += 32;
				CL_TrapParticles (&ent);
				i = (rand()%100) + 100;
				V_AddLight (ent.origin, i, 1, 0.8, 0.1);
			}
			else if (effects & EF_FLAG1)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 242);
				V_AddLight (ent.origin, 225, 1, 0.1, 0.1);
			}
			else if (effects & EF_FLAG2)
			{
				CL_FlagTrail (cent->lerp_origin, ent.origin, 115);
				V_AddLight (ent.origin, 225, 0.1, 0.1, 1);
			}
//======
//ROGUE
			else if (effects & EF_TAGTRAIL)
			{
				CL_TagTrail (cent->lerp_origin, ent.origin, 220);
				V_AddLight (ent.origin, 225, 1.0, 1.0, 0.0);
			}
			else if (effects & EF_TRACKERTRAIL)
			{
				if (effects & EF_TRACKER)
				{
					float intensity;

					intensity = 50 + (500 * (sin(cl.time/500.0) + 1.0));
					// FIXME - check out this effect in rendition
					if(vidref_val == VIDREF_GL)
						V_AddLight (ent.origin, intensity, -1.0, -1.0, -1.0);
					else
						V_AddLight (ent.origin, -1.0 * intensity, 1.0, 1.0, 1.0);
					}
				else
				{
					CL_Tracker_Shell (cent->lerp_origin);
					V_AddLight (ent.origin, 155, -1.0, -1.0, -1.0);
				}
			}
			else if (effects & EF_TRACKER)
			{
				CL_TrackerTrail (cent->lerp_origin, ent.origin, 0);
				// FIXME - check out this effect in rendition
				if(vidref_val == VIDREF_GL)
					V_AddLight (ent.origin, 200, -1, -1, -1);
				else
					V_AddLight (ent.origin, -200, 1, 1, 1);
			}
//ROGUE
//======
			// RAFAEL
			else if (effects & EF_GREENGIB)
			{
				CL_DiminishingTrail (cent->lerp_origin, ent.origin, cent, effects);				
			}
			// RAFAEL
			else if (effects & EF_IONRIPPER)
			{
				CL_IonripperTrail (cent->lerp_origin, ent.origin);
				V_AddLight (ent.origin, 100, 1, 0.5, 0.5);
			}
			// RAFAEL
			else if (effects & EF_BLUEHYPERBLASTER)
			{
				V_AddLight (ent.origin, 200, 0, 0, 1);
			}
			// RAFAEL
			else if (effects & EF_PLASMA)
			{
				if (effects & EF_ANIM_ALLFAST)
				{
					CL_BlasterTrail (cent->lerp_origin, ent.origin);
				}
				V_AddLight (ent.origin, 130, 1, 0.5, 0.5);
			}
		}

		VectorCopy (ent.origin, cent->lerp_origin);
	}
}



/*
==============
CL_AddViewWeapon
==============
*/
void CL_AddViewWeapon (player_state_t *ps, player_state_t *ops)
{
	entity_t	gun;		// view model
	int			i;

	// allow the gun to be completely removed
	if (!cl_gun->value)
		return;

	// don't draw gun if in wide angle view
	if (ps->fov > 90)
		return;

	memset (&gun, 0, sizeof(gun));

	if (gun_model)
		gun.model = gun_model;	// development tool
	else
		gun.model = cl.model_draw[ps->gunindex];
	if (!gun.model)
		return;

	// set up gun position
	for (i=0 ; i<3 ; i++)
	{
		gun.origin[i] = cl.refdef.vieworg[i] + ops->gunoffset[i]
			+ cl.lerpfrac * (ps->gunoffset[i] - ops->gunoffset[i]);
		gun.angles[i] = cl.refdef.viewangles[i] + LerpAngle (ops->gunangles[i],
			ps->gunangles[i], cl.lerpfrac);
	}

	if (gun_frame)
	{
		gun.frame = gun_frame;	// development tool
		gun.oldframe = gun_frame;	// development tool
	}
	else
	{
		gun.frame = ps->gunframe;
		if (gun.frame == 0)
			gun.oldframe = 0;	// just changed weapons, don't lerp from old
		else
			gun.oldframe = ops->gunframe;
	}

	gun.flags = RF_MINLIGHT | RF_DEPTHHACK | RF_WEAPONMODEL;
	gun.backlerp = 1.0 - cl.lerpfrac;
	VectorCopy (gun.origin, gun.oldorigin);	// don't lerp at all
	V_AddEntity (&gun);
}


/*
===============
CL_CalcViewValues

Sets cl.refdef view values
===============
*/
void CL_CalcViewValues (void)
{
	int			i;
	float		lerp, backlerp;
	centity_t	*ent;
	frame_t		*oldframe;
	player_state_t	*ps, *ops;

	// find the previous frame to interpolate from
	ps = &cl.frame.playerstate;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if (oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid)
		oldframe = &cl.frame;		// previous frame was dropped or involid
	ops = &oldframe->playerstate;

	// see if the player entity was teleported this frame
	if ( fabs(ops->pmove.origin[0] - ps->pmove.origin[0]) > 256*8
		|| abs(ops->pmove.origin[1] - ps->pmove.origin[1]) > 256*8
		|| abs(ops->pmove.origin[2] - ps->pmove.origin[2]) > 256*8)
		ops = ps;		// don't interpolate

	ent = &cl_entities[cl.playernum+1];
	lerp = cl.lerpfrac;

	// calculate the origin
	if ((cl_predict->value) && !(cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{	// use predicted values
		unsigned	delta;

		backlerp = 1.0 - lerp;
		for (i=0 ; i<3 ; i++)
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] 
				+ cl.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i])
				- backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if (delta < 100)
			cl.refdef.vieworg[2] -= cl.predicted_step * (100 - delta) * 0.01;
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.vieworg[i] = ops->pmove.origin[i]*0.125 + ops->viewoffset[i] 
				+ lerp * (ps->pmove.origin[i]*0.125 + ps->viewoffset[i] 
				- (ops->pmove.origin[i]*0.125 + ops->viewoffset[i]) );
	}

	// if not running a demo or on a locked frame, add the local angle movement
	if ( cl.frame.playerstate.pmove.pm_type < PM_DEAD )
	{	// use predicted values
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = cl.predicted_angles[i];
	}
	else
	{	// just use interpolated values
		for (i=0 ; i<3 ; i++)
			cl.refdef.viewangles[i] = LerpAngle (ops->viewangles[i], ps->viewangles[i], lerp);
	}

	for (i=0 ; i<3 ; i++)
		cl.refdef.viewangles[i] += LerpAngle (ops->kick_angles[i], ps->kick_angles[i], lerp);

	AngleVectors (cl.refdef.viewangles, cl.v_forward, cl.v_right, cl.v_up);

	// interpolate field of view
	cl.refdef.fov_x = ops->fov + lerp * (ps->fov - ops->fov);

	// don't interpolate blend color
	for (i=0 ; i<4 ; i++)
		cl.refdef.blend[i] = ps->blend[i];

	// add the weapon
	CL_AddViewWeapon (ps, ops);
}
#endif

/*
===============
CL_AddEntities

Emits all entities, particles, and lights to the refresh
===============
*/
void CL_AddEntities (void)
{
	if (cls.state != ca_active)
		return;

	if (cl.time > cl.frame.servertime)
	{
		if (cl_showclamp->value)
			Com_Printf ("high clamp %i\n", cl.time - cl.frame.servertime);
		cl.time = cl.frame.servertime;
		cl.lerpfrac = 1.0;
	}
	else if (cl.time < cl.frame.servertime - 100)
	{
		if (cl_showclamp->value)
			Com_Printf ("low clamp %i\n", cl.frame.servertime-100 - cl.time);
		cl.time = cl.frame.servertime - 100;
		cl.lerpfrac = 0;
	}
	else
		cl.lerpfrac = 1.0 - (cl.frame.servertime - cl.time) * 0.01;

	if (cl_timedemo->value)
		cl.lerpfrac = 1.0;

//	CL_AddPacketEntities (&cl.frame);
//	CL_AddTEnts ();
//	CL_AddParticles ();
//	CL_AddDLights ();
//	CL_AddLightStyles ();

#ifdef KINGPIN_ENGINE
	CL_AddPacketEntities (&cl.frame);
	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddParticles2 ();
	CL_AddDLights ();
	CL_AddLightStyles ();
	CL_CalcViewValues ();
#else
	CL_CalcViewValues ();
	// PMM - moved this here so the heat beam has the right values for the vieworg, and can lock the beam to the gun
	CL_AddPacketEntities (&cl.frame);
#if 0
	CL_AddProjectiles ();
#endif
	CL_AddTEnts ();
	CL_AddParticles ();
	CL_AddDLights ();
	CL_AddLightStyles ();
#endif
}



/*
===============
CL_GetEntitySoundOrigin

Called to get the sound spatialization origin
===============
*/
void CL_GetEntitySoundOrigin (int ent, vec3_t org)
{
	centity_t	*old;

	if (ent < 0 || ent >= MAX_EDICTS)
		Com_Error (ERR_DROP, "CL_GetEntitySoundOrigin: bad ent");
	old = &cl_entities[ent];
	VectorCopy (old->lerp_origin, org);

	// FIXME: bmodel issues...
}
