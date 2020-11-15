#include <pspkernel.h>
#include <stdio.h>

PSP_MODULE_INFO("q2gamel", 0x1000, 1, 1);
//PSP_MAIN_THREAD_ATTR(0);
//PSP_HEAP_SIZE(2 * 1024 * 1024)

SceUID game_lib = 0;
int __psp_free_heap(void);

int main(int argc, char **argv)
{
	sceKernelSleepThread();
	return 0;
}

int Q2_ModuleLoader(char *name)
{
    if(game_lib)
    {
	  return 2;
	}

	game_lib = sceKernelLoadModule(name, 0, NULL);
	if (game_lib < 0)
	{
		return -1;
	}
    else
    {
	    int status = 0;
		int ret = sceKernelStartModule(game_lib, 0, NULL, &status, 0);
		if (ret < 0)
		    return -2;
	}

	return 1;
}

int Q2_ModuleUnLoader(void)
{
	if(game_lib)
	{
		int ret = sceKernelStopModule(game_lib, 0, 0, 0, 0);
		if(ret < 0)
		  return -1;

		ret = sceKernelUnloadModule(game_lib);
	    if(ret < 0)
		  return -2;
	}
	game_lib = 0;

	//__psp_free_heap();

	return 1;
}
