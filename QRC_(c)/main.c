/*
Quake II Command Control(qIIcc) by Crow_bar 2010(c)
*/

#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdebug.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

PSP_MODULE_INFO("qIIcc", 0x200, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();

int main(int argc, char** argv)
{
    int cursor[8];
    int stb = 0;

	memset(cursor, 0,sizeof(cursor));
	
	SceCtrlData pad;
	pspDebugScreenInit();

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	while (1)
	{
			    sceDisplayWaitVblankStart();
		sceDisplayWaitVblankStart();
				sceDisplayWaitVblankStart();
		sceDisplayWaitVblankStart();
		        sceDisplayWaitVblankStart();
		sceDisplayWaitVblankStart();

    		
        pspDebugScreenClear();
        pspDebugScreenSetBackColor(0xA0602000);

		pspDebugScreenSetXY(0, 2);

		sceCtrlReadBufferPositive(&pad, 1);

		pspDebugScreenSetTextColor(0xffffff);
		pspDebugScreenPrintf("Quake II Command Control(qIIcc)\n");
        pspDebugScreenSetTextColor(0x0EFEE0);
	    pspDebugScreenPrintf("September 2010\n");
		pspDebugScreenPrintf("By Crow_Bar\n");
		pspDebugScreenPrintf("\n\n");

	    pspDebugScreenSetTextColor(0xffffff);
		pspDebugScreenPrintf("Game Mod To Start:\n");
        pspDebugScreenSetTextColor(0x00ffff);

	    if (cursor[0] == 0)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("Classic\n");
		pspDebugScreenSetTextColor(0x00ffff);

	    if (cursor[0] == 1)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("Capture the Flag\n");  //(ctf)
        pspDebugScreenSetTextColor(0x00ffff);
        
	    if (cursor[0] == 2)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("Ground Zero\n");       //(rogue)
		pspDebugScreenSetTextColor(0x00ffff);

	    if (cursor[0] == 3)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("The Reckoning\n");     //(xatrix)
		pspDebugScreenSetTextColor(0x00ffff);

	    if (cursor[0] == 4)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("Action\n");            //(action)
		pspDebugScreenSetTextColor(0x00ffff);

	    if (cursor[0] == 5)
		    pspDebugScreenSetTextColor(0x00ff00);
		pspDebugScreenPrintf("Gloom\n");             //(gloom)
		pspDebugScreenSetTextColor(0x00ffff);

		if (pad.Buttons != 0)
		{
			if(pad.Buttons & PSP_CTRL_UP)
			   cursor[stb]--;

			if(pad.Buttons & PSP_CTRL_DOWN)
			   cursor[stb]++;

		    if(pad.Buttons & PSP_CTRL_CROSS)
			   stb++;

			if(pad.Buttons & PSP_CTRL_CIRCLE)
			   stb--;
        }
        if(stb > 2)
		   stb = 2;
		if(stb < 0)
		   stb = 0;
		   
#if 1
		if(cursor[0] > 5)
		   cursor[0] = 0;
		if(cursor[0] < 0)
		   cursor[0] = 5;
#else
		if(cursor[0] > 5)
		   cursor[0] = 5;
		if(cursor[0] < 0)
		   cursor[0] = 0;
#endif
        sceKernelDelayThread(50000);
	}
}


