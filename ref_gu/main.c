#include <pspkernel.h>
#include <stdio.h>

PSP_MODULE_INFO("gamepsp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_CLEAR_STACK | THREAD_ATTR_VFPU);

int main(int argc, char** argv) 
{
    printf("Game is Q2\n");
	sceKernelSleepThread();
	return 0;
}




