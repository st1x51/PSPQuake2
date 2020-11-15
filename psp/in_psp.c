#include "../client/client.h"
#include "keyboard/danzeff.h"

#include <pspctrl.h>
#include <psphprm.h>


cvar_t* in_analog_speed;
cvar_t* in_analog_tolerance;
cvar_t* in_analog_acceleration;

cvar_t* in_analog_anubfunc;
cvar_t* in_analog_anubinvert;

static int keystate[K_MAX];
static int keytime = 0;

static int   anubfunc     = 0;
static int   anubinvert   = 0;
static float speed        = 0;
static float deadZone     = 0;
static float acceleration = 0;

static void keymap(int key, qboolean down, int repeattime) 
{
	if(down) 
	{
		if(keystate[key] == 0) 
		{
			Key_Event(key, true, keytime);
			keystate[key] = keytime;
		} 
		else if(repeattime > 0 && keystate[key] + repeattime < keytime) 
		{
			Key_Event(key, true, keytime);
			keystate[key] += repeattime / 2;
		}
	} 
	else 
	{
		if(keystate[key] != 0) 
		{
			Key_Event(key, false, keytime);
			keystate[key] = 0;
		}
	}
}

void IN_Init(void) 
{
	memset(keystate, 0, sizeof(keystate));

	in_analog_tolerance    = Cvar_Get("in_analog_tolerance",    "0.1", CVAR_ARCHIVE);
	in_analog_speed        = Cvar_Get("in_analog_speed",        "5.0", CVAR_ARCHIVE);
	in_analog_acceleration = Cvar_Get("in_analog_acceleration", "0.0", CVAR_ARCHIVE);

	in_analog_anubfunc   = Cvar_Get("in_analog_anubfunc",   "0", CVAR_ARCHIVE);
	in_analog_anubinvert = Cvar_Get("in_analog_anubinvert", "0", CVAR_ARCHIVE);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void IN_Shutdown(void) 
{
}

int keyb_init = 0;
int keyb_count = 0;

float IN_CalcInput(int axis, float speed, float tolerance, float acceleration)
{
	float value = ((float) axis / 128.0f) - 1.0f;

	if (value == 0.0f)
	{
		return 0.0f;
	}

	float abs_value = fabs(value);

	if (abs_value < tolerance)
	{
		return 0.0f;
	}

	abs_value -= tolerance;
	abs_value /= (1.0f - tolerance);
	abs_value = powf(abs_value, acceleration);
	abs_value *= speed;

	if (value < 0.0f)
	{
		value = -abs_value;
	}
	else
	{
		value = abs_value;
	}
	return value;
}

static SceCtrlData pad;
int prevstate = 0;
void IN_Frame(void) 
{
	keytime = Sys_Milliseconds();

	
	sceCtrlPeekBufferPositive(&pad, 1);

	keymap(K_UPARROW,			  pad.Buttons & PSP_CTRL_UP,		  250);
	keymap(K_DOWNARROW,			  pad.Buttons & PSP_CTRL_DOWN,		  250);
	keymap(K_LEFTARROW,			  pad.Buttons & PSP_CTRL_LEFT,		  250);
	keymap(K_RIGHTARROW,		  pad.Buttons & PSP_CTRL_RIGHT,		  250);
/*
	//addition keys
    if(pad.Buttons & PSP_CTRL_UP)
    {
		keymap(K_EXT1,			      pad.Buttons & PSP_CTRL_CROSS,		  250);
		keymap(K_EXT2,			      pad.Buttons & PSP_CTRL_CIRCLE,	  250);
		keymap(K_EXT3,			      pad.Buttons & PSP_CTRL_SQUARE,	  250);
		keymap(K_EXT4,			      pad.Buttons & PSP_CTRL_TRIANGLE,	  250);
	}
	else
	{
*/
		keymap(K_CROSS,				  pad.Buttons & PSP_CTRL_CROSS,		  250);
		keymap(K_CIRCLE,			  pad.Buttons & PSP_CTRL_CIRCLE,	  250);
		keymap(K_SQUARE,			  pad.Buttons & PSP_CTRL_SQUARE,	  250);
		keymap(K_TRIANGLE,			  pad.Buttons & PSP_CTRL_TRIANGLE,	  250);
/*
    }
*/
	keymap(K_LTRIGGER,			  pad.Buttons & PSP_CTRL_LTRIGGER,	  250);
	keymap(K_RTRIGGER,			  pad.Buttons & PSP_CTRL_RTRIGGER,	  250);
	keymap(K_START,				  pad.Buttons & PSP_CTRL_START,		  250);
	keymap(K_SELECT,			  pad.Buttons & PSP_CTRL_SELECT,	  250);
	keymap(K_HOLD,				  pad.Buttons & PSP_CTRL_HOLD,		    0);

	if(keyb_init)
	{
          if(!danzeff_isinitialized())
          {
		      keyb_init = 0;
          }
          else
          {
			  int inpt = danzeff_readInput(pad);

			  if(inpt)
			  {
				    Key_Event(inpt + K_MAX, true, keytime);
					prevstate = inpt + K_MAX;
			  }
		      else
		      {
				    if(prevstate)
				    {
					   Key_Event(prevstate, false, keytime);
					   prevstate = 0;
				    }
			  }
		  }
	}

	if(sceHprmIsRemoteExist()) //hprm пульт ду
	{
		unsigned int epkeys;
		sceHprmPeekCurrentKey(&epkeys);
		keymap(K_EP_PLAYPAUSE,	  epkeys & PSP_HPRM_PLAYPAUSE,      250);
		keymap(K_EP_FORWARD,	  epkeys & PSP_HPRM_FORWARD,	    250);
		keymap(K_EP_BACK,		  epkeys & PSP_HPRM_BACK,			250);
		keymap(K_EP_VOLUP,		  epkeys & PSP_HPRM_VOL_UP,			250);
		keymap(K_EP_VOLDOWN,	  epkeys & PSP_HPRM_VOL_DOWN,		250);
		keymap(K_EP_HOLD,		  epkeys & PSP_HPRM_HOLD,			  0);
	}

	if(keyb_init) //skip analog
	   return; 

	if(in_analog_speed->modified)
	{
       speed = in_analog_speed->value;
	   in_analog_speed->modified = false;
	}
	if(in_analog_tolerance->modified)
	{
       deadZone = in_analog_tolerance->value;
       in_analog_tolerance->modified = false;
	}
	if(in_analog_acceleration->modified)
	{
       acceleration = in_analog_acceleration->value;
	   in_analog_acceleration->modified = false;
	}
	if(in_analog_anubfunc->modified)
	{
	   anubfunc = (int)in_analog_anubfunc->value;
	   in_analog_anubfunc->modified = false;
	}
	if(in_analog_anubinvert->modified)
	{
	   anubinvert = (int)in_analog_anubinvert->value;
	   in_analog_anubinvert->modified = false;
	}
	
    float ax = IN_CalcInput(pad.Lx, speed, deadZone, acceleration);
	float ay = IN_CalcInput(pad.Ly, speed, deadZone, acceleration);

	switch(anubfunc)
	{
	  case 1 :
		{
			char cmd[64];

			if(ay < -0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+forward %i %i\n", K_ANUB_UP, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-forward %i %i\n", K_ANUB_UP, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay > 0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+back %i %i\n", K_ANUB_DOWN, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-back %i %i\n", K_ANUB_DOWN, keytime);
			}
			Cbuf_AddText(cmd);

			cl.viewangles[YAW] -= ax;
		}
		break;
	  
		case 2 :
		{
			char cmd[64];

			if(ax < -0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+moveleft %i %i\n", K_ANUB_LEFT, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-moveleft %i %i\n", K_ANUB_LEFT, keytime);
			}
			Cbuf_AddText(cmd);

			if(ax > 0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+moveright %i %i\n", K_ANUB_RIGHT, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-moveright %i %i\n", K_ANUB_RIGHT, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay < -0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+forward %i %i\n", K_ANUB_UP, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-forward %i %i\n", K_ANUB_UP, keytime);
			}
			Cbuf_AddText(cmd);

			if(ay > 0.5f) 
			{
				Com_sprintf(cmd, sizeof(cmd), "+back %i %i\n", K_ANUB_DOWN, keytime);
			} 
			else 
			{
				Com_sprintf(cmd, sizeof(cmd), "-back %i %i\n", K_ANUB_DOWN, keytime);
			}
			Cbuf_AddText(cmd);
		}
		break;
	  
		default :
			cl.viewangles[YAW] -= ax;
			if(anubinvert)
			{
				cl.viewangles[PITCH] -= ay;
			}
			else
			{
				cl.viewangles[PITCH] += ay;
			}
	break;
	}
#if 0 //not used
	int i;
	for(i = 0; i < 3; i++)
	{
		while(cl.viewangles[i] < -180.0f) 
		{
			cl.viewangles[i] += 360.0f;
		}
		while(cl.viewangles[i] > 180.0f) 
		{
			cl.viewangles[i] -= 360.0f;
		}
	}
#endif
}
