/*
  Kernel module Hook
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspsysevent.h>

#include <psptypes.h>

#include <pspmodulemgr.h>

#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("Q2SystemHook", PSP_MODULE_KERNEL, 1, 0);

SceUID sysKernelLoadModule(const char *path, int flags, SceKernelLMOption *option)
{
	int k1 = pspSdkSetK1(0);
	SceUID ret = sceKernelLoadModule(path, flags, option);
    pspSdkSetK1(k1);
    return ret;
}

SceUID sysKernelLoadModuleByID (SceUID fid, int flags, SceKernelLMOption *option)
{
    int k1 = pspSdkSetK1(0);
	SceUID ret = sceKernelLoadModuleByID(fid, flags, option);
    pspSdkSetK1(k1);
    return ret;
}

int sysKernelStartModule (SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option)
{
    int k1 = pspSdkSetK1(0);
	int ret = sceKernelStartModule(modid, argsize, argp, status, option);
	pspSdkSetK1(k1);
    return ret;
}

int sysKernelStopModule (SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option)
{
    int k1 = pspSdkSetK1(0);
	int ret = sceKernelStopModule (modid, argsize, argp, status, option);
	pspSdkSetK1(k1);
    return ret;
}

int sysKernelUnloadModule (SceUID modid)
{
    int k1 = pspSdkSetK1(0);
	int ret = sceKernelUnloadModule(modid);
	pspSdkSetK1(k1);
    return ret;
}

int sysKernelQueryModuleInfo (SceUID modid, SceKernelModuleInfo *info)
{
    int k1 = pspSdkSetK1(0);
	int ret = sceKernelQueryModuleInfo(modid, info);
	pspSdkSetK1(k1);
    return ret;
}

int module_start (SceSize args, void *argp)
{
	return 0;
}

int module_stop (SceSize args, void *argp)
{
	return 0;
}
