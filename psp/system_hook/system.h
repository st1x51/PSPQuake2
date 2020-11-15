/*
Func exported from kernel
*/
#ifndef SYSMEM_H
#define SYSMEM_H

#include <pspmodulemgr.h>

SceUID sysKernelLoadModule(const char *path, int flags, SceKernelLMOption *option);
SceUID sysKernelLoadModuleByID  (SceUID fid, int flags, SceKernelLMOption *option);
int sysKernelStartModule(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
int sysKernelStopModule (SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
int sysKernelUnloadModule(SceUID modid);
int sysKernelQueryModuleInfo(SceUID modid, SceKernelModuleInfo *info);

#endif //SYSMEM_H

