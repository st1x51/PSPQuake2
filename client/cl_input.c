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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t	*cl_nodelta;

extern	unsigned	sys_frame_time;
unsigned	frame_msec;
unsigned	old_sys_frame_time;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


Key_Event (int key, qboolean down, unsigned time);

  +mlook src time

===============================================================================
*/


kbutton_t	in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack;
kbutton_t	in_up, in_down;
#ifdef KINGPIN_ENGINE
kbutton_t	in_activate;
#endif

int			in_impulse;

#ifdef KINGPIN_ENGINE
qboolean	cl_moveleft_held;
qboolean	cl_moveright_held;	// for the bike lean
static qboolean	cl_left_held;
static qboolean	cl_right_held;
static short	cl_keyboard_sidemove;			// before IN_Move adds mouse/controller input
#endif


void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1)
		return;		// still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = sys_frame_time - 100;

	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}

void IN_Impulse (void) {in_impulse=atoi(Cmd_Argv(1));}

#ifdef KINGPIN_ENGINE
void CL_ActivateButton (qboolean down)
{
	if (down)
		KeyDown (&in_activate);
	else
		KeyUp (&in_activate);
}
#endif

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += sys_frame_time - key->downtime;
		key->downtime = sys_frame_time;
	}

#if 0
	if (msec)
	{
		Com_Printf ("%i ", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
}




//==========================================================================

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (void)
{
	float	speed;
	float	up, down;

#ifdef KINGPIN_ENGINE
	cl_right_held = in_right.state & 1;
	cl_left_held = in_left.state & 1;
	cl_moveleft_held = in_moveleft.state & 1;
	cl_moveright_held = in_moveright.state & 1;
#endif

	if (in_speed.state & 1)
		speed = cls.frametime * cl_anglespeedkey->value;
	else
		speed = cls.frametime;

	if (!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
#ifdef KINGPIN_ENGINE
		// the strafe keys steer a bike
		if (cl.frame.playerstate.pmove.pm_type == PM_BIKE)
		{
			cl.viewangles[YAW] += speed*cl_yawspeed->value*(in_moveleft.state & 1);
			cl.viewangles[YAW] -= speed*cl_yawspeed->value*(in_moveright.state & 1);
		}
#endif
	}
	if (in_klook.state & 1)
	{
		cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_back);
	}
	
	up = CL_KeyState (&in_lookup);
	down = CL_KeyState(&in_lookdown);
	
	cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * up;
	cl.viewangles[PITCH] += speed*cl_pitchspeed->value * down;
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd)
{	
	CL_AdjustAngles ();
	
	memset (cmd, 0, sizeof(*cmd));
	
	VectorCopy (cl.viewangles, cmd->angles);
	if (in_strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

	if (! (in_klook.state & 1) )
	{	
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_forwardspeed->value * CL_KeyState (&in_back);
	}	

//
// adjust for speed key / running
//
	if ( (in_speed.state & 1) ^ (int)(cl_run->value) )
	{
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}

#ifdef KINGPIN_ENGINE
	// Retail caches this before IN_Move modifies the completed usercmd.
	cl_keyboard_sidemove = cmd->sidemove;
#endif
}

void CL_ClampPitch (void)
{
	float	pitch;
#ifdef KINGPIN_ENGINE
	pmtype_t	type = cl.frame.playerstate.pmove.pm_type;
	float		yawspeed, turnspeed, speed, diff, step;
	vec3_t		velocity;
	int			i;

	static vec3_t	oldangles = {-999, -999, -999};
	static vec3_t	lastturn;
	static vec3_t	turnaccel;
	static float	last_frametime;
#endif

	pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // wrapped
	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // wrapped

#ifdef KINGPIN_ENGINE
	if (type == PM_HOVERCAR || type == PM_HOVERCAR_GROUND || type == PM_BIKE)
	{
		if (type == PM_BIKE)
		{
			for (i = 0; i < 3; i++)
				velocity[i] = cl.frame.playerstate.pmove.velocity[i] * 0.125f;
			speed = VectorLength (velocity);

			if (speed <= 10)
			{
				yawspeed = 0;
				turnspeed = 6.75f;
			}
			else if (speed < 50)
			{
				yawspeed = 45;
				turnspeed = 6.75f;
			}
			else if (speed <= 100)
			{
				yawspeed = speed * 0.01f * 90;
				turnspeed = speed * 0.01f * 13.5f;
			}
			else
			{
				yawspeed = 90;
				turnspeed = 13.5f;
			}

			if (fabs (turnaccel[YAW] / last_frametime) < 5)
				turnspeed *= (fabs (turnaccel[YAW] / last_frametime) * 0.2f + 1) * 0.5f;

			diff = AngleDiff (cl.viewangles[YAW], oldangles[YAW]);
			if ((!cl_keyboard_sidemove ||
				(diff < 0 && cl_keyboard_sidemove > 0) ||
				(diff > 0 && cl_keyboard_sidemove < 0)) &&
				!cl_right_held && !cl_left_held)
			{
				if (speed >= 100)
					speed = 0.4f;
				else
					speed = (100 - speed) * 0.01f * 0.6f + 0.4f;
				yawspeed *= speed;
			}
		}
		else
		{
			yawspeed = 180;
			turnspeed = 45;
		}

		if (type == PM_HOVERCAR_GROUND || type == PM_BIKE)
			cl.viewangles[PITCH] = -pitch;
		else
		{
			if (cl.viewangles[PITCH] + pitch > 59)
				cl.viewangles[PITCH] = 59 - pitch;
			if (cl.viewangles[PITCH] + pitch < -59)
				cl.viewangles[PITCH] = -59 - pitch;
		}

		if (oldangles[YAW] <= -990)
			VectorCopy (cl.viewangles, oldangles);
		else
		{
			if (cls.frametime > (1.0f / 30.0f))
				turnspeed *= 1 - (1 - cls.frametime * 30) * 0.5f;

			diff = AngleDiff (cl.viewangles[YAW], oldangles[YAW]);
			step = yawspeed * cls.frametime;
			if (step < fabs (diff))
			{
				cl.viewangles[YAW] = anglemod (oldangles[YAW] + (diff <= 0 ? step : -step));
				if (cl.viewangles[YAW] > 180)
					cl.viewangles[YAW] -= 360;
			}

			diff = AngleDiff (cl.viewangles[PITCH], oldangles[PITCH]);
			step = cls.frametime * 90;
			if (step < fabs (diff))
			{
				cl.viewangles[PITCH] = anglemod (oldangles[PITCH] + (diff <= 0 ? step : -step));
				if (cl.viewangles[PITCH] > 180)
					cl.viewangles[PITCH] -= 360;
			}

			for (i = 0; i < 3; i++)
			{
				if (i == PITCH && type == PM_BIKE)
					continue;
				diff = AngleDiff (cl.viewangles[i], oldangles[i]);
				step = turnspeed * cls.frametime;
				if (step < fabs (diff - lastturn[i]))
				{
					cl.viewangles[i] = anglemod (oldangles[i] - lastturn[i] +
						(diff - lastturn[i] <= 0 ? step : -step));
					if (cl.viewangles[i] > 180)
						cl.viewangles[i] -= 360;
				}
			}
		}

		for (i = 0; i < 3; i++)
		{
			diff = AngleDiff (cl.viewangles[i], oldangles[i]);
			turnaccel[i] = diff - lastturn[i];
			lastturn[i] = diff;
			oldangles[i] = cl.viewangles[i];
		}
		last_frametime = cls.frametime;

		return;
	}
#endif

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;
	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove (usercmd_t *cmd)
{
	int		ms;
	int		i;

//
// figure button bits
//	
	if ( in_attack.state & 3 )
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;
	
	if (in_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~2;

#ifdef KINGPIN_ENGINE
	if (in_activate.state & 3)
		cmd->buttons |= BUTTON_ACTIVATE;
	in_activate.state &= ~2;
#endif

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	// send milliseconds of time to apply the move
	ms = cls.frametime * 1000;
	if (ms > 250)
		ms = 100;		// time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch ();
	for (i=0 ; i<3 ; i++)
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

	cmd->impulse = in_impulse;
	in_impulse = 0;

// send the ambient light level at the player's current position
	cmd->lightlevel = (byte)cl_lightlevel->value;
}

/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd (void)
{
	usercmd_t	cmd;

	frame_msec = sys_frame_time - old_sys_frame_time;
	if (frame_msec < 1)
		frame_msec = 1;
	if (frame_msec > 200)
		frame_msec = 200;
	
	// get basic movement from keyboard
	CL_BaseMove (&cmd);

	// allow mice or other external controllers to add to the move
	IN_Move (&cmd);

	CL_FinishMove (&cmd);

	old_sys_frame_time = sys_frame_time;

//cmd.impulse = cls.framecount;

	return cmd;
}


void IN_CenterView (void)
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("centerview",IN_CenterView);

	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
}



/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	sizebuf_t	buf;
	byte		data[128];
	int			i;
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;
	int			checksumIndex;

	// build a command even if not connected

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	cl.cmd_time[i] = cls.realtime;	// for netgraph ping calculation

	*cmd = CL_CreateCmd ();

	cl.cmd = *cmd;

	if (cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	if ( cls.state == ca_connected)
	{
		if (cls.netchan.message.cursize	|| curtime - cls.netchan.last_sent > 1000 )
			Netchan_Transmit (&cls.netchan, 0, buf.data);	
		return;
	}

	// send a userinfo update if needed
	if (userinfo_modified)
	{
		CL_FixUpGender();
		userinfo_modified = false;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
	}

	SZ_Init (&buf, data, sizeof(data));

	if (cmd->buttons && cl.cinematictime > 0 && !cl.attractloop 
		&& cls.realtime - cl.cinematictime > 1000)
	{	// skip the rest of the cinematic
		SCR_FinishCinematic ();
	}

	// begin a client move command
	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if (cl_nodelta->value || !cl.frame.valid || cls.demowaiting)
		MSG_WriteLong (&buf, -1);	// no compression
	else
		MSG_WriteLong (&buf, cl.frame.serverframe);

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	i = (cls.netchan.outgoing_sequence-2) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	memset (&nullcmd, 0, sizeof(nullcmd));
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence-1) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] = COM_BlockSequenceCRCByte(
		buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
#ifdef KINGPIN_ENGINE
		cls.netchan.outgoing_sequence, cls.challenge);
#else
		cls.netchan.outgoing_sequence);
#endif

	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);	
}


