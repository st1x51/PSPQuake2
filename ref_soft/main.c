#include <pspkernel.h>
#include <stdio.h>

PSP_MODULE_INFO("gamepsp", PSP_MODULE_USER, 1, 0);

void Com_Printf(char* msg, ...);

int module_start( SceSize arglen, void *argp )
{
	return 0;
}

int module_stop( SceSize arglen, void *argp )
{
	Com_Printf("Crbot prx Stoped\n");
	return 0;
}




