/*
Copyright (C) 2010 Crow_bar.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysevent.h>

PSP_MODULE_INFO("Q2Kernel", 0x1006, 1, 0);

int sceHprm_driver_1528D408();
int sceImposeSetVideoOutMode(int, int, int);
int sceDve_driver_DEB2F80C(int);
int sceDve_driver_93828323(int);
int sceDve_driver_0B85524C(int);
int sceDve_driver_A265B504(int, int, int);

void sceGeEdramSetSize(int);
int Q2KernelInit(int kb)
{
	int k1 = pspSdkSetK1(0);
    sceGeEdramSetSize(kb*1024);
    int size = sceGeEdramGetSize();
	pspSdkSetK1(k1);
	return size;
}

int Q2KernelCheckVideoOut()
{
	int k1 = pspSdkSetK1(0);
	int intr = sceKernelCpuSuspendIntr();

	int cable = sceHprm_driver_1528D408();

	sceKernelCpuResumeIntr(intr);
	pspSdkSetK1(k1);

	return cable;
}

int Q2KernelSetVideoOut(int u, int mode, int width, int height, int x, int y, int z)
{
	int k1 = pspSdkSetK1(0);
	int res;

	res = sceDve_driver_DEB2F80C(u);
	if (res < 0)
	{
		res = -1; pspSdkSetK1(k1); return -1;
	}

	// These params will end in sceDisplaySetMode
	res = sceImposeSetVideoOutMode(mode, width, height);
	if (res < 0)
	{
		res = -2; pspSdkSetK1(k1); return -2;
	}

	res = sceDve_driver_93828323(0);
	if (res < 0)
	{
		res = -3; pspSdkSetK1(k1); return -3;
	}

	res = sceDve_driver_0B85524C(1);
	if (res < 0)
	{
        res = -4; pspSdkSetK1(k1); return -4;
	}

	res = sceDve_driver_A265B504(x, y, z);
	if (res < 0)
	{
		res = -5; pspSdkSetK1(k1); return -5;
	}
	
	pspSdkSetK1(k1);
	return res;
}

int module_start(SceSize args, void *argp)
{
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}

