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

#include "qcommon.h"



#define	STEPSIZE	18

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct
{
	vec3_t		origin;			// full float precision
	vec3_t		velocity;		// full float precision

	vec3_t		forward, right, up;
	float		frametime;


	csurface_t	*groundsurface;
	cplane_t	groundplane;
	int			groundcontents;

	vec3_t		previous_origin;
	qboolean	ladder;
} pml_t;

pmove_t		*pm;
pml_t		pml;


// movement parameters
float	pm_stopspeed = 100;
float	pm_maxspeed = 300;
float	pm_duckspeed = 100;
float	pm_accelerate = 10;
float	pm_airaccelerate = 0;
float	pm_wateraccelerate = 10;
float	pm_friction = 6;
float	pm_waterfriction = 1;
float	pm_waterspeed = 400;
#ifdef KINGPIN_ENGINE
vec3_t	pm_vehicle_angles;

/*
==================
PM_VehicleSlope

kingpin.exe @00432510 tilts vehicles to the ground: it drops two small boxes
128 units from 10 units either side of the vehicle along axis and returns
the pitch of the line between where they land, in -180..180 (the angle
maths is @004341b0, which unlike vectoangles does not round to integers).
==================
*/
static vec3_t pm_vehicle_probe_mins = {-4, -4, 0};
static vec3_t pm_vehicle_probe_maxs = {4, 4, 4};

static float PM_VehicleSlope (vec3_t axis)
{
	vec3_t	start, end, first, delta;
	trace_t	tr;
	float	pitch, forward;

	VectorMA (pml.origin, 10, axis, start);
	start[2] = pml.origin[2];
	VectorCopy (start, end);
	end[2] = pml.origin[2] - 128;
	tr = pm->trace (start, pm_vehicle_probe_mins, pm_vehicle_probe_maxs, end);
	VectorCopy (tr.endpos, first);

	VectorMA (pml.origin, -10, axis, start);
	start[2] = pml.origin[2];
	VectorCopy (start, end);
	end[2] = pml.origin[2] - 128;
	tr = pm->trace (start, pm_vehicle_probe_mins, pm_vehicle_probe_maxs, end);

	VectorSubtract (tr.endpos, first, delta);
	VectorNormalize (delta);

	if (delta[0] == 0 && delta[1] == 0)
		pitch = delta[2] > 0 ? 90 : 270;
	else
	{
		forward = sqrt (delta[0]*delta[0] + delta[1]*delta[1]);
		pitch = atan2 (delta[2], forward) * 180 / M_PI;
		if (pitch < 0)
			pitch += 360;
	}

	pitch = -pitch;
	if (pitch > 180)
		pitch -= 360;
	if (pitch < -180)
		pitch += 360;
	return pitch;
}
#endif

/*

  walking up a step should kill some velocity

*/


/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1

void PM_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	int		i;
	
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
}




/*
==================
PM_StepSlideMove

Each intersection will try to step over the obstruction instead of
sliding along it.

Returns a new origin, velocity, and contact entity
Does not modify any world state?
==================
*/
#define	MIN_STEP_NORMAL	0.7		// can't step up onto very steep slopes
#define	MAX_CLIP_PLANES	5
void PM_StepSlideMove_ (void)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	int			i, j;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	
	numbumps = 4;
	
	VectorCopy (pml.velocity, primal_velocity);
	numplanes = 0;
	
	time_left = pml.frametime;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i=0 ; i<3 ; i++)
			end[i] = pml.origin[i] + time_left * pml.velocity[i];

		trace = pm->trace (pml.origin, pm->mins, pm->maxs, end);

		if (trace.allsolid)
		{	// entity is trapped in another solid
			pml.velocity[2] = 0;	// don't build up falling damage
			return;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, pml.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		// save entity for contact
		if (pm->numtouch < MAXTOUCH && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
		
		time_left -= time_left * trace.fraction;

		// slide along this plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			VectorCopy (vec3_origin, pml.velocity);
			break;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

#ifdef KINGPIN_ENGINE
		// kingpin.exe @00431400 reports near-vertical impacts (vehicle crash sounds)
		if (fabs (trace.plane.normal[2]) < 0.6)
			pm->wall_collision = 1;
#endif

#if 0
	float		rub;

		//
		// modify velocity so it parallels all of the clip planes
		//
		if (numplanes == 1)
		{	// go along this plane
			VectorCopy (pml.velocity, dir);
			VectorNormalize (dir);
			rub = 1.0 + 0.5 * DotProduct (dir, planes[0]);

			// slide along the plane
			PM_ClipVelocity (pml.velocity, planes[0], pml.velocity, 1.01);
			// rub some extra speed off on xy axis
			// not on Z, or you can scrub down walls
			pml.velocity[0] *= rub;
			pml.velocity[1] *= rub;
			pml.velocity[2] *= rub;
		}
		else if (numplanes == 2)
		{	// go along the crease
			VectorCopy (pml.velocity, dir);
			VectorNormalize (dir);
			rub = 1.0 + 0.5 * DotProduct (dir, planes[0]);

			// slide along the plane
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, pml.velocity);
			VectorScale (dir, d, pml.velocity);

			// rub some extra speed off
			VectorScale (pml.velocity, rub, pml.velocity);
		}
		else
		{
//			Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
			VectorCopy (vec3_origin, pml.velocity);
			break;
		}

#else
//
// modify original_velocity so it parallels all of the clip planes
//
		for (i=0 ; i<numplanes ; i++)
		{
			PM_ClipVelocity (pml.velocity, planes[i], pml.velocity, 1.01);
			for (j=0 ; j<numplanes ; j++)
				if (j != i)
				{
					if (DotProduct (pml.velocity, planes[j]) < 0)
						break;	// not ok
				}
			if (j == numplanes)
				break;
		}
		
		if (i != numplanes)
		{	// go along this plane
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
//				Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
				VectorCopy (vec3_origin, pml.velocity);
				break;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, pml.velocity);
			VectorScale (dir, d, pml.velocity);
		}
#endif
		//
		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		//
		if (DotProduct (pml.velocity, primal_velocity) <= 0)
		{
			VectorCopy (vec3_origin, pml.velocity);
			break;
		}
	}

	if (pm->s.pm_time)
	{
		VectorCopy (primal_velocity, pml.velocity);
	}

#ifdef KINGPIN_ENGINE
	// kingpin.exe @00431400: keep contact with the ground when walking down
	// slopes and stairs
	if (pm->groundentity && pml.velocity[2] <= 0)
	{
		VectorCopy (pml.origin, end);
		end[2] -= pml.frametime * 400;
		trace = pm->trace (pml.origin, pm->mins, pm->maxs, end);
		if (trace.fraction < 1 && !trace.startsolid)
			VectorCopy (trace.endpos, pml.origin);
	}

	// bikes and cars that lost speed against a slope get deflected up it
	// (or launched off a ramp) instead of stopping
	if (pm->s.pm_type == PM_BIKE || pm->s.pm_type == PM_CAR)
	{
		float	speed;

		speed = VectorLength (primal_velocity);
		if (speed - VectorLength (pml.velocity) > 10)
		{
			VectorAdd (pml.origin, primal_velocity, end);
			trace = pm->trace (pml.origin, pm->mins, pm->maxs, end);
			if (trace.fraction < 1 && trace.plane.normal[2] < 0.5 && trace.plane.normal[2] > -0.8)
			{
				VectorNormalize2 (primal_velocity, dir);
				VectorAdd (dir, trace.plane.normal, dir);
				VectorNormalize (dir);
				VectorScale (dir, speed, pml.velocity);
				if (trace.plane.normal[2] > -0.3)
					pml.velocity[2] += 180;
				pm->groundentity = NULL;
			}
		}
	}
#endif
}

/*
==================
PM_StepSlideMove

==================
*/
void PM_StepSlideMove (void)
{
	vec3_t		start_o, start_v;
	vec3_t		down_o, down_v;
	trace_t		trace;
	float		down_dist, up_dist;
//	vec3_t		delta;
	vec3_t		up, down;

	VectorCopy (pml.origin, start_o);
	VectorCopy (pml.velocity, start_v);

	PM_StepSlideMove_ ();

	VectorCopy (pml.origin, down_o);
	VectorCopy (pml.velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	trace = pm->trace (up, pm->mins, pm->maxs, up);
	if (trace.allsolid)
		return;		// can't step up

	// try sliding above
	VectorCopy (up, pml.origin);
	VectorCopy (start_v, pml.velocity);

	PM_StepSlideMove_ ();

	// push down the final amount
	VectorCopy (pml.origin, down);
	down[2] -= STEPSIZE;
	trace = pm->trace (pml.origin, pm->mins, pm->maxs, down);
	if (!trace.allsolid)
	{
		VectorCopy (trace.endpos, pml.origin);
	}

#if 0
	VectorSubtract (pml.origin, up, delta);
	up_dist = DotProduct (delta, start_v);

	VectorSubtract (down_o, start_o, delta);
	down_dist = DotProduct (delta, start_v);
#else
	VectorCopy(pml.origin, up);

	// decide which one went farther
    down_dist = (down_o[0] - start_o[0])*(down_o[0] - start_o[0])
        + (down_o[1] - start_o[1])*(down_o[1] - start_o[1]);
    up_dist = (up[0] - start_o[0])*(up[0] - start_o[0])
        + (up[1] - start_o[1])*(up[1] - start_o[1]);
#endif

	if (down_dist > up_dist || trace.plane.normal[2] < MIN_STEP_NORMAL)
	{
		VectorCopy (down_o, pml.origin);
		VectorCopy (down_v, pml.velocity);
		return;
	}
	//!! Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[2] = down_v[2];
}


/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
void PM_Friction (void)
{
	float	*vel;
	float	speed, newspeed, control;
	float	friction;
	float	drop;

#ifdef KINGPIN_ENGINE
	if (pm->s.pm_type == PM_CAR)
		return;		// kingpin.exe @00431b80
#endif
	
	vel = pml.velocity;
	
	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1] + vel[2]*vel[2]);
	if (speed < 1)
	{
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	drop = 0;

// apply ground friction
	if ((pm->groundentity && pml.groundsurface && !(pml.groundsurface->flags & SURF_SLICK) ) || (pml.ladder) )
	{
		friction = pm_friction;
#ifdef KINGPIN_ENGINE
		if (pm->s.pm_type == PM_HOVERCAR ||
			pm->s.pm_type == PM_HOVERCAR_GROUND || pm->s.pm_type == PM_BIKE)
		{
			friction *= 0.1f;
			if (pm->s.pm_type == PM_BIKE && speed > 1000)
				friction *= 0.3f;
		}
#endif

		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control*friction*pml.frametime;
	}

// apply water friction
#ifdef KINGPIN_ENGINE
	// only when swimming; the jetpack gets a fixed air drag instead
	if (pm->waterlevel >= 2 && !pml.ladder)
		drop += speed*pm_waterfriction*pm->waterlevel*pml.frametime;
	else if (pm->s.pm_type == PM_NORMAL_WITH_JETPACK)
		drop += speed*pm_waterfriction*2*pml.frametime;
#else
	if (pm->waterlevel && !pml.ladder)
		drop += speed*pm_waterfriction*pm->waterlevel*pml.frametime;
#endif

// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
	{
		newspeed = 0;
	}
	newspeed /= speed;

	vel[0] = vel[0] * newspeed;
	vel[1] = vel[1] * newspeed;
	vel[2] = vel[2] * newspeed;
}


/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
#ifdef KINGPIN_ENGINE
/*
kingpin.exe @00431d40. Vehicles get twice the wish speed and a thrust that
grows with their speed, never push against their direction of travel
(the input is then replaced by the view direction with no speed), and are
steered: the velocity is turned towards the wanted direction by a fraction
of 2*frametime (all at once for a bike on the ground). Hovercars and
bikes only accelerate on the ground. PM_CAR is not accelerated at all.
*/
void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	static vec3_t	steer;		// kept between calls, as in kingpin.exe
	int			i;
	float		addspeed, accelspeed, currentspeed;
	float		speed, frac;
	vec3_t		veldir;
	int			type;

	type = pm->s.pm_type;
	if (type == PM_CAR)
		return;

	if (type == PM_HOVERCAR_GROUND || type == PM_HOVERCAR
		|| (type == PM_BIKE && pm->cmd.forwardmove >= 0))
	{
		wishspeed *= 2;
		accel *= VectorLength (pml.velocity) / (pm_maxspeed + pm_maxspeed) * 0.08 + 0.02;
		if (type == PM_BIKE)
			accel *= 0.75;
		if (VectorNormalize2 (pml.velocity, veldir) > 0 && DotProduct (veldir, wishdir) < 0)
		{
			wishspeed = 0;
			VectorCopy (pml.forward, wishdir);
		}
	}

	if ((type != PM_HOVERCAR_GROUND && type != PM_BIKE) || pm->groundentity)
	{
		currentspeed = DotProduct (pml.velocity, wishdir);
		addspeed = wishspeed - currentspeed;
		if (addspeed > 0)
		{
			accelspeed = pml.frametime*wishspeed*accel;
			if (accelspeed > addspeed)
				accelspeed = addspeed;
			for (i=0 ; i<3 ; i++)
				pml.velocity[i] += accelspeed*wishdir[i];
		}
	}

	// steering
	if ((type == PM_HOVERCAR_GROUND || type == PM_HOVERCAR) && pm->cmd.forwardmove > 0)
		;
	else if (type == PM_BIKE && VectorLength (pml.velocity) > 0)
		;
	else
		return;

	speed = VectorNormalize2 (pml.velocity, veldir);
	frac = pml.frametime + pml.frametime;
	if (type == PM_BIKE && pm->groundentity)
		frac = 1.0;

	if (wishdir[0] == 0 && wishdir[1] == 0)
	{
		if (DotProduct (pml.forward, veldir) <= 0)
			VectorScale (pml.forward, -frac, steer);
		else
			VectorScale (pml.forward, frac, steer);
	}
	else if (DotProduct (pml.forward, veldir) <= 0)
	{	// facing away from the travel direction: push straight on and
		// steer with what is left from the last call
		if (pm->cmd.forwardmove < 1)
			return;
		VectorMA (pml.velocity, wishspeed + wishspeed, wishdir, pml.velocity);
	}
	else if (pm->cmd.forwardmove >= 0)
		VectorScale (wishdir, frac, steer);
	else
		VectorScale (pml.forward, frac, steer);

	VectorMA (steer, 1.0 - frac, veldir, steer);
	VectorNormalize (steer);
	VectorScale (steer, speed, pml.velocity);
}
#else
void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel*pml.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	for (i=0 ; i<3 ; i++)
		pml.velocity[i] += accelspeed*wishdir[i];	
}
#endif

void PM_AirAccelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed, wishspd = wishspeed;
		
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (pml.velocity, wishdir);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * wishspeed * pml.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

#ifdef KINGPIN_ENGINE
	// kingpin.exe @004320a0: vehicles have less air control
	if (pm->s.pm_type == PM_HOVERCAR_GROUND || pm->s.pm_type == PM_HOVERCAR
		|| pm->s.pm_type == PM_BIKE)
		accelspeed *= 0.6;
#endif
	
	for (i=0 ; i<3 ; i++)
		pml.velocity[i] += accelspeed*wishdir[i];	
}

/*
=============
PM_AddCurrents
=============
*/
void PM_AddCurrents (vec3_t	wishvel)
{
	vec3_t	v;
	float	s;

	//
	// account for ladders
	//

	if (pml.ladder && fabs(pml.velocity[2]) <= 200)
	{
		if ((pm->viewangles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = 200;
		else if ((pm->viewangles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = -200;
		else if (pm->cmd.upmove > 0)
			wishvel[2] = 200;
		else if (pm->cmd.upmove < 0)
			wishvel[2] = -200;
		else
			wishvel[2] = 0;

		// limit horizontal speed when on a ladder
		if (wishvel[0] < -25)
			wishvel[0] = -25;
		else if (wishvel[0] > 25)
			wishvel[0] = 25;

		if (wishvel[1] < -25)
			wishvel[1] = -25;
		else if (wishvel[1] > 25)
			wishvel[1] = 25;
	}


	//
	// add water currents
	//

	if (pm->watertype & MASK_CURRENT)
	{
		VectorClear (v);

		if (pm->watertype & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pm->watertype & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pm->watertype & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pm->watertype & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		s = pm_waterspeed;
		if ((pm->waterlevel == 1) && (pm->groundentity))
			s /= 2;

		VectorMA (wishvel, s, v, wishvel);
	}

	//
	// add conveyor belt velocities
	//

	if (pm->groundentity)
	{
		VectorClear (v);

		if (pml.groundcontents & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pml.groundcontents & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pml.groundcontents & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		VectorMA (wishvel, 100 /* pm->groundentity->speed */, v, wishvel);
	}
}


/*
===================
PM_WaterMove

===================
*/
void PM_WaterMove (void)
{
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;

//
// user intentions
//
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*pm->cmd.forwardmove + pml.right[i]*pm->cmd.sidemove;

	if (!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += pm->cmd.upmove;

	PM_AddCurrents (wishvel);

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	if (wishspeed > pm_maxspeed)
	{
		VectorScale (wishvel, pm_maxspeed/wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}
	wishspeed *= 0.5;

	PM_Accelerate (wishdir, wishspeed, pm_wateraccelerate);

	PM_StepSlideMove ();
}


/*
===================
PM_AirMove

===================
*/
#ifdef KINGPIN_ENGINE
/*
kingpin.exe @00432510. Besides walking this moves the vehicles and the
jetpack. Unlike Quake II gravity also pulls while on the ground (the slide
move then keeps the player on it), and after moving a vehicle the ground
under it is probed for pm_vehicle_angles.
*/
void PM_AirMove (void)
{
	int			i;
	int			type;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		maxspeed;
	float		angle;

	type = pm->s.pm_type;
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	if (type == PM_BIKE && fmove < 0)
		fmove *= 0.1;	// slow reverse

	if (type == PM_HOVERCAR)
	{	// flies where it looks
		pml.right[2] = 0;
		VectorNormalize (pml.forward);
		VectorNormalize (pml.right);
		for (i=0 ; i<3 ; i++)
			wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
		wishvel[2] += pm->cmd.upmove;
	}
	else
	{
		if (type == PM_BIKE)
			smove = 0;	// bikes only steer by turning
		for (i=0 ; i<2 ; i++)
			wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
		wishvel[2] = 0;
	}

	PM_AddCurrents (wishvel);

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

//
// clamp to server defined max speed
//
	maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? pm_duckspeed : pm_maxspeed;
	if (type == PM_HOVERCAR_GROUND || type == PM_HOVERCAR || type == PM_BIKE)
		maxspeed *= 2;

	if (wishspeed > maxspeed)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed;
	}

	if (type == PM_HOVERCAR)
	{
		PM_Accelerate (wishdir, wishspeed, 3);
		if (pml.velocity[0] || pml.velocity[1] || pml.velocity[2])
			PM_StepSlideMove ();
		return;
	}

	if (type == PM_NORMAL_WITH_JETPACK && !pm->groundentity)
	{	// hovering: weak control and no gravity; PM_CheckJump sets the climb
		PM_Accelerate (wishdir, wishspeed, pm_accelerate * (1.0f/6));
		if (pml.velocity[0] || pml.velocity[1] || pml.velocity[2])
			PM_StepSlideMove ();
		return;
	}

	if ( pml.ladder )
	{
		PM_Accelerate (wishdir, wishspeed, pm_accelerate);
		if (!wishvel[2])
		{
			if (pml.velocity[2] > 0)
			{
				pml.velocity[2] -= pm->s.gravity * pml.frametime;
				if (pml.velocity[2] < 0)
					pml.velocity[2]  = 0;
			}
			else
			{
				pml.velocity[2] += pm->s.gravity * pml.frametime;
				if (pml.velocity[2] > 0)
					pml.velocity[2]  = 0;
			}
		}
		PM_StepSlideMove ();
		return;
	}

	if (!pm->groundentity && type != PM_HOVERCAR_GROUND && type != PM_BIKE)
	{	// not on ground, so little effect on velocity
		PM_AirAccelerate (wishdir, wishspeed, pm_accelerate);
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		PM_StepSlideMove ();
		return;
	}

	if (pm->groundentity)
		pml.velocity[2] = 0;
	PM_Accelerate (wishdir, wishspeed, pm_accelerate);
	pml.velocity[2] -= pm->s.gravity * pml.frametime;

	if (type == PM_HOVERCAR_GROUND || type == PM_BIKE
		|| pml.velocity[0] || pml.velocity[1])
		PM_StepSlideMove ();

	if (type != PM_HOVERCAR_GROUND && type != PM_BIKE)
		return;

	// tilt the vehicle to the ground (bikes don't roll)
	if (type == PM_BIKE)
		pm_vehicle_angles[ROLL] = 0;
	else
		pm_vehicle_angles[ROLL] = PM_VehicleSlope (pml.right);

	angle = -PM_VehicleSlope (pml.forward);
	if (angle > 180)
		angle -= 360;
	if (angle < -180)
		angle += 360;
	pm_vehicle_angles[PITCH] = angle;
	pm_vehicle_angles[YAW] = 0;
}
#else
void PM_AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		maxspeed;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	
//!!!!! pitch should be 1/3 so this isn't needed??!
#if 0
	pml.forward[2] = 0;
	pml.right[2] = 0;
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);
#endif

	for (i=0 ; i<2 ; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] = 0;

	PM_AddCurrents (wishvel);

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

//
// clamp to server defined max speed
//
	maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? pm_duckspeed : pm_maxspeed;

	if (wishspeed > maxspeed)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed;
	}
	
	if ( pml.ladder )
	{
		PM_Accelerate (wishdir, wishspeed, pm_accelerate);
		if (!wishvel[2])
		{
			if (pml.velocity[2] > 0)
			{
				pml.velocity[2] -= pm->s.gravity * pml.frametime;
				if (pml.velocity[2] < 0)
					pml.velocity[2]  = 0;
			}
			else
			{
				pml.velocity[2] += pm->s.gravity * pml.frametime;
				if (pml.velocity[2] > 0)
					pml.velocity[2]  = 0;
			}
		}
		PM_StepSlideMove ();
	}
	else if ( pm->groundentity )
	{	// walking on ground
		pml.velocity[2] = 0; //!!! this is before the accel
		PM_Accelerate (wishdir, wishspeed, pm_accelerate);

// PGM	-- fix for negative trigger_gravity fields
//		pml.velocity[2] = 0;
		if(pm->s.gravity > 0)
			pml.velocity[2] = 0;
		else
			pml.velocity[2] -= pm->s.gravity * pml.frametime;
// PGM

		if (!pml.velocity[0] && !pml.velocity[1])
			return;
		PM_StepSlideMove ();
	}
	else
	{	// not on ground, so little effect on velocity
		if (pm_airaccelerate)
			PM_AirAccelerate (wishdir, wishspeed, pm_accelerate);
		else
			PM_Accelerate (wishdir, wishspeed, 1);
		// add gravity
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		PM_StepSlideMove ();
	}
}
#endif


/*
=============
PM_CatagorizePosition
=============
*/
void PM_CatagorizePosition (void)
{
	vec3_t		point;
	int			cont;
	trace_t		trace;
	int			sample1;
	int			sample2;
#ifdef KINGPIN_ENGINE
	int			flags;
#endif

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid	
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25;
#ifdef KINGPIN_ENGINE
	// kingpin.exe @00432cf0: vehicles stick to the ground up to 360
	if (pml.velocity[2] > ((pm->s.pm_type == PM_HOVERCAR_GROUND || pm->s.pm_type == PM_BIKE) ? 360 : 180))
#else
	if (pml.velocity[2] > 180) //!!ZOID changed from 100 to 180 (ramp accel)
#endif
	{
		pm->s.pm_flags &= ~PMF_ON_GROUND;
		pm->groundentity = NULL;
	}
	else
	{
		trace = pm->trace (pml.origin, pm->mins, pm->maxs, point);
		pml.groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;
#ifdef KINGPIN_ENGINE
		// kingpin.exe PM_CatagorizePosition @00432cf0: first matching
		// surface material wins; water at ankle height overrides it below.
		flags = trace.surface ? trace.surface->flags : 0;
		if (flags & SURF_CONCRETE)
			pm->footsteptype = 0;
		else if (flags & SURF_FABRIC)
			pm->footsteptype = 1;
		else if (flags & SURF_GRAVEL)
			pm->footsteptype = 2;
		else if (flags & SURF_METAL)
			pm->footsteptype = 3;
		else if (flags & SURF_METAL_L)
			pm->footsteptype = 4;
		else if (flags & SURF_SNOW)
			pm->footsteptype = 5;
		else if (flags & SURF_TILE)
			pm->footsteptype = 6;
		else if (flags & SURF_WOOD)
			pm->footsteptype = 7;
		else if (flags & SURF_WATER)
			pm->footsteptype = 8;
		else
			pm->footsteptype = 0;
#endif

		if (!trace.ent || (trace.plane.normal[2] < 0.7 && !trace.startsolid) )
		{
			pm->groundentity = NULL;
			pm->s.pm_flags &= ~PMF_ON_GROUND;
		}
		else
		{
			pm->groundentity = trace.ent;

			// hitting solid ground will end a waterjump
			if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
			{
				pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
				pm->s.pm_time = 0;
			}

			if (! (pm->s.pm_flags & PMF_ON_GROUND) )
			{	// just hit the ground
				pm->s.pm_flags |= PMF_ON_GROUND;
				// don't do landing time if we were just going down a slope
				if (pml.velocity[2] < -200)
				{
					pm->s.pm_flags |= PMF_TIME_LAND;
					// don't allow another jump for a little while
					if (pml.velocity[2] < -400)
						pm->s.pm_time = 25;	
					else
						pm->s.pm_time = 18;
				}
			}
		}

#if 0
		if (trace.fraction < 1.0 && trace.ent && pml.velocity[2] < 0)
			pml.velocity[2] = 0;
#endif

		if (pm->numtouch < MAXTOUCH && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

//
// get waterlevel, accounting for ducking
//
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[2] = pml.origin[2] + pm->mins[2] + 1;	
	cont = pm->pointcontents (point);

	if (cont & MASK_WATER)
	{
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents (point);
		if (cont & MASK_WATER)
		{
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents (point);
			if (cont & MASK_WATER)
				pm->waterlevel = 3;
		}
	}

#ifdef KINGPIN_ENGINE
	if (pm->waterlevel == 1)
		pm->footsteptype = 8;
#endif

}


/*
=============
PM_CheckJump
=============
*/
void PM_CheckJump (void)
{
#ifdef KINGPIN_ENGINE
	if (pm->s.pm_type == PM_NORMAL_WITH_JETPACK)
	{
		if (!pm->cmd.upmove)
			return;		// no regular jumping (PMF_JUMP_HELD is left alone)
		// kingpin.exe @00433100: rising input is three times the command
		// speed; descending input snaps to -300 below the 300 threshold.
		if (pm->cmd.upmove > 0)
		{
			pml.velocity[2] += 3 * pm->cmd.upmove * pml.frametime;
			if (pml.velocity[2] > 500)
				pml.velocity[2] = 500;
		}
		else if (pml.velocity[2] < 300)
			pml.velocity[2] = -300;
		pm->groundentity = NULL;
		return;
	}

	// The bike never enters the regular jump path in the Kingpin PMove.
	if (pm->s.pm_type == PM_BIKE)
		return;
#endif

	if (pm->s.pm_flags & PMF_TIME_LAND)
	{	// hasn't been long enough since landing to jump again
		return;
	}

	if (pm->cmd.upmove < 10)
	{	// not holding jump
		pm->s.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump to be released
	if (pm->s.pm_flags & PMF_JUMP_HELD)
		return;

	if (pm->s.pm_type == PM_DEAD)
		return;

	if (pm->waterlevel >= 2)
	{	// swimming, not jumping
		pm->groundentity = NULL;

		if (pml.velocity[2] <= -300)
			return;

		if (pm->watertype == CONTENTS_WATER)
			pml.velocity[2] = 100;
		else if (pm->watertype == CONTENTS_SLIME)
			pml.velocity[2] = 80;
		else
			pml.velocity[2] = 50;
		return;
	}

#ifdef KINGPIN_ENGINE
	if (pm->s.pm_type == PM_HOVERCAR || pm->s.pm_type == PM_HOVERCAR_GROUND)
	{
		vec3_t end;
		trace_t tr;

		VectorCopy (pml.origin, end);
		end[2] -= 48;
		tr = pm->trace (pml.origin, pm->mins, pm->maxs, end);
		if (tr.fraction < 1)
		{
			pml.velocity[2] += 500 * pml.frametime;
			if (pml.velocity[2] > 300)
				pml.velocity[2] = 300;
			if (pml.velocity[2] > 0)
				pm->groundentity = NULL;
		}
		return;
	}
#endif

	if (pm->groundentity == NULL)
		return;		// in air, so no effect

	pm->s.pm_flags |= PMF_JUMP_HELD;

	pm->groundentity = NULL;
	pml.velocity[2] += 270;
	if (pml.velocity[2] < 270)
		pml.velocity[2] = 270;
}


/*
=============
PM_CheckSpecialMovement
=============
*/
void PM_CheckSpecialMovement (void)
{
	vec3_t	spot;
	int		cont;
	vec3_t	flatforward;
	trace_t	trace;

	if (pm->s.pm_time)
		return;

	pml.ladder = false;

	// check for ladder
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize (flatforward);

	VectorMA (pml.origin, 1, flatforward, spot);
	trace = pm->trace (pml.origin, pm->mins, pm->maxs, spot);
	if ((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
		pml.ladder = true;

	// check for water jump
	if (pm->waterlevel != 2)
		return;

	VectorMA (pml.origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents (spot);
	if (!(cont & CONTENTS_SOLID))
		return;

	spot[2] += 16;
	cont = pm->pointcontents (spot);
	if (cont)
		return;
	// jump out of water
	VectorScale (flatforward, 50, pml.velocity);
	pml.velocity[2] = 350;

	pm->s.pm_flags |= PMF_TIME_WATERJUMP;
	pm->s.pm_time = 255;
}


/*
===============
PM_FlyMove
===============
*/
void PM_FlyMove (qboolean doclip)
{
	float	speed, drop, friction, control, newspeed;
	float	currentspeed, addspeed, accelspeed;
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	float		maxspeed;
	vec3_t		wishdir;
	float		wishspeed;
	vec3_t		end;
	trace_t		trace;

#ifdef KINGPIN_ENGINE
	pm->viewheight = 40;	// kingpin.exe @004334f0 spectator movement
#else
	pm->viewheight = 22;
#endif

	// friction

	speed = VectorLength (pml.velocity);
	if (speed < 1)
	{
		VectorCopy (vec3_origin, pml.velocity);
	}
	else
	{
		drop = 0;

		friction = pm_friction*1.5;	// extra friction
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control*friction*pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pml.velocity, newspeed, pml.velocity);
	}

	// accelerate
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	maxspeed = pm_maxspeed;

#ifdef KINGPIN_ENGINE
	// kingpin.exe @004334f0: spectators fly forward three times as fast
	if (pm->s.pm_type == PM_SPECTATOR)
	{
		fmove *= 3;
		maxspeed *= 3;
	}
#endif
	
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	//
	// clamp to server defined max speed
	//
	if (wishspeed > maxspeed)
	{
		VectorScale (wishvel, maxspeed / wishspeed, wishvel);
		wishspeed = maxspeed;
	}


	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = pm_accelerate*pml.frametime*wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	for (i=0 ; i<3 ; i++)
		pml.velocity[i] += accelspeed*wishdir[i];	

	if (doclip) {
		for (i=0 ; i<3 ; i++)
			end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];

		trace = pm->trace (pml.origin, pm->mins, pm->maxs, end);

		VectorCopy (trace.endpos, pml.origin);
	} else {
		// move
		VectorMA (pml.origin, pml.frametime, pml.velocity, pml.origin);
	}
}


/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->viewheight
==============
*/
void PM_CheckDuck (void)
{
	trace_t	trace;
#ifdef KINGPIN_ENGINE
	qboolean vehicle;

	vehicle = pm->s.pm_type == PM_HOVERCAR ||
		pm->s.pm_type == PM_HOVERCAR_GROUND || pm->s.pm_type == PM_BIKE;
#endif

	pm->mins[0] = -16;
	pm->mins[1] = -16;

	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	if (pm->s.pm_type == PM_GIB)
	{
		pm->mins[2] = 0;
		pm->maxs[2] = 16;
		pm->viewheight = 8;
		return;
	}

#ifdef KINGPIN_ENGINE
	// kingpin.exe @004337b0: no ducking decisions in a vehicle
	if (vehicle)
	{
		pm->mins[2] = -2;
	}
	else
#endif
	{
		pm->mins[2] = -24;

		if (pm->s.pm_type == PM_DEAD)
		{
			pm->s.pm_flags |= PMF_DUCKED;
		}
		else if (pm->cmd.upmove < 0 && (pm->s.pm_flags & PMF_ON_GROUND) )
		{	// duck
			pm->s.pm_flags |= PMF_DUCKED;
		}
		else
		{	// stand up if possible
			if (pm->s.pm_flags & PMF_DUCKED)
			{
				// try to stand up
#ifdef KINGPIN_ENGINE
				pm->maxs[2] = 48;
#else
				pm->maxs[2] = 32;
#endif
				trace = pm->trace (pml.origin, pm->mins, pm->maxs, pml.origin);
				if (!trace.allsolid)
					pm->s.pm_flags &= ~PMF_DUCKED;
			}
		}
	}

#ifdef KINGPIN_ENGINE
	if (pm->s.pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = 24;
		pm->viewheight = 18;
	}
	else
	{
		pm->maxs[2] = 48;
		pm->viewheight = 36;
	}

	if (vehicle)
	{
		pm->mins[0] = pm->mins[1] = -32;
		pm->maxs[0] = pm->maxs[1] = 32;
	}
#else
	if (pm->s.pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = 4;
		pm->viewheight = -2;
	}
	else
	{
		pm->maxs[2] = 32;
		pm->viewheight = 22;
	}
#endif
}


/*
==============
PM_DeadMove
==============
*/
void PM_DeadMove (void)
{
	float	forward;

	if (!pm->groundentity)
		return;

	// extra friction

	forward = VectorLength (pml.velocity);
	forward -= 20;
	if (forward <= 0)
	{
		VectorClear (pml.velocity);
	}
	else
	{
		VectorNormalize (pml.velocity);
		VectorScale (pml.velocity, forward, pml.velocity);
	}
}


qboolean	PM_GoodPosition (void)
{
	trace_t	trace;
	vec3_t	origin, end;
	int		i;

	if (pm->s.pm_type == PM_SPECTATOR)
		return true;

	for (i=0 ; i<3 ; i++)
		origin[i] = end[i] = pm->s.origin[i]*0.125;
	trace = pm->trace (origin, pm->mins, pm->maxs, end);

	return !trace.allsolid;
}

/*
================
PM_SnapPosition

On exit, the origin will have a value that is pre-quantized to the 0.125
precision of the network channel and in a valid position.
================
*/
void PM_SnapPosition (void)
{
	int		sign[3];
	int		i, j, bits;
	short	base[3];
	// try all single bits first
	static int jitterbits[8] = {0,4,1,2,3,5,6,7};

	// snap velocity to eigths
	for (i=0 ; i<3 ; i++)
		pm->s.velocity[i] = (int)(pml.velocity[i]*8);

	for (i=0 ; i<3 ; i++)
	{
		if (pml.origin[i] >= 0)
			sign[i] = 1;
		else 
			sign[i] = -1;
		pm->s.origin[i] = (int)(pml.origin[i]*8);
		if (pm->s.origin[i]*0.125 == pml.origin[i])
			sign[i] = 0;
	}
	VectorCopy (pm->s.origin, base);

	// try all combinations
	for (j=0 ; j<8 ; j++)
	{
		bits = jitterbits[j];
		VectorCopy (base, pm->s.origin);
		for (i=0 ; i<3 ; i++)
			if (bits & (1<<i) )
				pm->s.origin[i] += sign[i];

		if (PM_GoodPosition ())
			return;
	}

	// go back to the last position
	VectorCopy (pml.previous_origin, pm->s.origin);
//	Com_DPrintf ("using previous_origin\n");
}

#if 0 // def KINGPIN_ENGINE
// kingpin.exe @00433c10 still uses this search order (+1, 0, -1)
//NO LONGER USED
/*
================
PM_InitialSnapPosition

================
*/
void PM_InitialSnapPosition (void)
{
	int		x, y, z;
	short	base[3];

	VectorCopy (pm->s.origin, base);

	for (z=1 ; z>=-1 ; z--)
	{
		pm->s.origin[2] = base[2] + z;
		for (y=1 ; y>=-1 ; y--)
		{
			pm->s.origin[1] = base[1] + y;
			for (x=1 ; x>=-1 ; x--)
			{
				pm->s.origin[0] = base[0] + x;
				if (PM_GoodPosition ())
				{
					pml.origin[0] = pm->s.origin[0]*0.125;
					pml.origin[1] = pm->s.origin[1]*0.125;
					pml.origin[2] = pm->s.origin[2]*0.125;
					VectorCopy (pm->s.origin, pml.previous_origin);
					return;
				}
			}
		}
	}

	Com_DPrintf ("Bad InitialSnapPosition\n");
}
#else
/*
================
PM_InitialSnapPosition

================
*/
void PM_InitialSnapPosition(void)
{
	int        x, y, z;
	short      base[3];
	static int offset[3] = { 0, -1, 1 };

	VectorCopy (pm->s.origin, base);

	for ( z = 0; z < 3; z++ ) {
		pm->s.origin[2] = base[2] + offset[ z ];
		for ( y = 0; y < 3; y++ ) {
			pm->s.origin[1] = base[1] + offset[ y ];
			for ( x = 0; x < 3; x++ ) {
				pm->s.origin[0] = base[0] + offset[ x ];
				if (PM_GoodPosition ()) {
					pml.origin[0] = pm->s.origin[0]*0.125;
					pml.origin[1] = pm->s.origin[1]*0.125;
					pml.origin[2] = pm->s.origin[2]*0.125;
					VectorCopy (pm->s.origin, pml.previous_origin);
					return;
				}
			}
		}
	}

	Com_DPrintf ("Bad InitialSnapPosition\n");
}

#endif

/*
================
PM_ClampAngles

================
*/
void PM_ClampAngles (void)
{
	short	temp;
	int		i;

	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{
		pm->viewangles[YAW] = SHORT2ANGLE(pm->cmd.angles[YAW] + pm->s.delta_angles[YAW]);
		pm->viewangles[PITCH] = 0;
		pm->viewangles[ROLL] = 0;
	}
	else
	{
		// circularly clamp the angles with deltas
		for (i=0 ; i<3 ; i++)
		{
			temp = pm->cmd.angles[i] + pm->s.delta_angles[i];
			pm->viewangles[i] = SHORT2ANGLE(temp);
		}

		// don't let the player look up or down more than 90 degrees
		if (pm->viewangles[PITCH] > 89 && pm->viewangles[PITCH] < 180)
			pm->viewangles[PITCH] = 89;
		else if (pm->viewangles[PITCH] < 271 && pm->viewangles[PITCH] >= 180)
			pm->viewangles[PITCH] = 271;
	}
	AngleVectors (pm->viewangles, pml.forward, pml.right, pml.up);
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove (pmove_t *pmove)
{
	pm = pmove;

	// clear results
	pm->numtouch = 0;
	VectorClear (pm->viewangles);
	pm->viewheight = 0;
	pm->groundentity = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;
#ifdef KINGPIN_ENGINE
	pm->footsteptype = 0;
	pm->wall_collision = 0;
#endif

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	// convert origin and velocity to float values
	pml.origin[0] = pm->s.origin[0]*0.125;
	pml.origin[1] = pm->s.origin[1]*0.125;
	pml.origin[2] = pm->s.origin[2]*0.125;

	pml.velocity[0] = pm->s.velocity[0]*0.125;
	pml.velocity[1] = pm->s.velocity[1]*0.125;
	pml.velocity[2] = pm->s.velocity[2]*0.125;

	// save old org in case we get stuck
	VectorCopy (pm->s.origin, pml.previous_origin);

	pml.frametime = pm->cmd.msec * 0.001;
#ifdef KINGPIN_ENGINE
	// kingpin.exe @00433e50: hovercars near the ground feel less gravity
	if (pm->s.pm_type == PM_HOVERCAR_GROUND)
		pm->s.gravity = (short)(pm->s.gravity * 0.4);
#endif

	PM_ClampAngles ();

	if (pm->s.pm_type == PM_SPECTATOR)
	{
		PM_FlyMove (false);
		PM_SnapPosition ();
		return;
	}

	if (pm->s.pm_type >= PM_DEAD)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if (pm->s.pm_type == PM_FREEZE)
		return;		// no movement at all

	// set mins, maxs, and viewheight
	PM_CheckDuck ();

	if (pm->snapinitial)
		PM_InitialSnapPosition ();

	// set groundentity, watertype, and waterlevel
	PM_CatagorizePosition ();

	if (pm->s.pm_type == PM_DEAD)
		PM_DeadMove ();

	PM_CheckSpecialMovement ();

	// drop timing counter
	if (pm->s.pm_time)
	{
		int		msec;

		msec = pm->cmd.msec >> 3;
		if (!msec)
			msec = 1;
		if ( msec >= pm->s.pm_time) 
		{
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}
		else
			pm->s.pm_time -= msec;
	}

	if (pm->s.pm_flags & PMF_TIME_TELEPORT)
	{	// teleport pause stays exactly in place
	}
	else if (pm->s.pm_flags & PMF_TIME_WATERJUMP)
	{	// waterjump has no control, but falls
		pml.velocity[2] -= pm->s.gravity * pml.frametime;
		if (pml.velocity[2] < 0)
		{	// cancel as soon as we are falling down again
			pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->s.pm_time = 0;
		}

		PM_StepSlideMove ();
	}
	else
	{
		PM_CheckJump ();

		PM_Friction ();

		if (pm->waterlevel >= 2)
			PM_WaterMove ();
		else {
			vec3_t	angles;

			VectorCopy(pm->viewangles, angles);
			if (angles[PITCH] > 180)
				angles[PITCH] = angles[PITCH] - 360;
			angles[PITCH] /= 3;

			AngleVectors (angles, pml.forward, pml.right, pml.up);

			PM_AirMove ();
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	PM_CatagorizePosition ();

	PM_SnapPosition ();
}

