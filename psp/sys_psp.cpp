extern "C"
{
#include "../client/client.h"
void* GetGameAPI(void*);
void* GetRefAPI(void*);
void IN_Frame(void);
}
#include <stdexcept>
#include <vector>

#include <sys/unistd.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspkernel.h>
#include <psputility.h>
#include <pspmoduleinfo.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspctrl.h>
#include <valloc.h>

extern"C"
{
#include "m33libs/kubridge.h"
#include "keyboard/danzeff.h"

#if defined (SLIM) && defined (Q2K)
#include "Q2Kernel/Q2Kernel.h"
#endif

#if 0
#include "system_hook/system.h"
#endif

int noregsound = 0;
}

#include <time.h>


PSP_MODULE_INFO("QuakeII", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-2048); //2 mb for mp3 thread and game modules


void disableFloatingPointExceptions()
{

 asm(
	".set push\n"
	".set noreorder\n"
	"cfc1    $2, $31\n"		// Get the FP Status Register.
	"lui     $8, 0x80\n"	// (?)
	"and     $8, $2, $8\n"	// Mask off all bits except for 23 of FCR. (? - where does the 0x80 come in?)
	"ctc1    $8, $31\n"		// Set the FP Status Register.
	".set pop\n"
	:						// No inputs.
	:						// No outputs.
	: "$2", "$8"			// Clobbered registers.
	);
}
//===============================================================================
#define MAX_NUM_ARGVS	50
#define MAX_ARG_LENGTH  64
static char *empty_string = "";
char    *f_argv[MAX_NUM_ARGVS];
int     f_argc;
cvar_t* sys_cpufreq;

#if 0
cvar_t* sys_modunload;
int kernel_modmgr = 1;
#endif

unsigned int sys_frame_time;
int maphunk;

void Sys_SendKeyEvents (void) 
{
	sys_frame_time = Sys_Milliseconds();	
	IN_Frame();
}

void Sys_Init(void) 
{
	sys_cpufreq = Cvar_Get("psp_cpufreq", "333", CVAR_ARCHIVE);
#if 0
#ifdef PSP_PHAT
    sys_modunload = Cvar_Get("sys_modunload", "0", CVAR_ARCHIVE);
#else
    sys_modunload = Cvar_Get("sys_modunload", "1", CVAR_ARCHIVE);
#endif
#endif
}

void Sys_Quit(void) 
{
	CL_Shutdown();
	Qcommon_Shutdown();
    sceKernelExitGame();
}

void Sys_Frame(void) 
{
	if(sys_cpufreq->modified) 
	{
		sys_cpufreq->modified = false;

		if(sys_cpufreq->value <= 111.0f) 
		{
			sys_cpufreq->value = 111.0f;
			scePowerSetClockFrequency(111, 111,  56);
		} 
		else if(sys_cpufreq->value >= 266.0f) 
		{
			sys_cpufreq->value = 333.0f;
			scePowerSetClockFrequency(333, 333, 166);
		} 
		else 
		{
			sys_cpufreq->value = 222.0f;
			scePowerSetClockFrequency(222, 222, 111);
		}
		
		Com_Printf("PSP Cpu Speed : %d MHz   Bus Speed : %d MHz\n",
			scePowerGetCpuClockFrequency(),
			scePowerGetBusClockFrequency()
		);
	}
}

// LIB Loading
static SceUID ref_lib  = -1;
static SceUID game_lib = -1;

void* Sys_GetGameAPI(void* params) 
{
	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "%s/gamepsp.prx", FS_Gamedir());

    if(game_lib != -1)
    {
#if 0
		if(sys_modunload->value)
        {
		   Sys_Error("Sys_GetGameAPI without Sys_UnloadGame");
		}
		else
#endif
		{
	       return GetGameAPI(params);
	    }
	}
#if 0
    if(kernel_modmgr)
    {
		game_lib = sysKernelLoadModule(name, 0, NULL);
		if (game_lib < 0)
	    {
            Sys_Error("Sys_GetGameAPI: Error load module (%08x)", game_lib);
        }
        else
        {
		    int status = 0;
			int ret = sysKernelStartModule(game_lib, 0, NULL, &status, 0);
			if (ret < 0)
			    Sys_Error("Sys_GetGameAPI: Error start module (%08x)", ret);
	    }
	}
	else
#endif
	{
		game_lib = pspSdkLoadStartModule(name, PSP_MEMORY_PARTITION_USER);
		if (game_lib < 0)
		{
			Com_Printf("Sys_GetGameAPI: pspSdkLoadStartModule failed (%08x), trying kuKernelLoadModule\n", game_lib);
	        game_lib = kuKernelLoadModule(name, 0, NULL);
			if (game_lib < 0)
		    {
	            Sys_Error("Sys_GetGameAPI: Error load module (%08x)", game_lib);
	        }
	        else
	        {
			    int status = 0;
				int ret = sceKernelStartModule(game_lib, 0, NULL, &status, 0);
				if (ret < 0)
				    Sys_Error("Sys_GetGameAPI: Error start module (%08x)", ret);
		    }
		}
    }
	return GetGameAPI(params);
}

void Sys_UnloadGame(void) 
{
#if 0
	if(!sys_modunload->value)
       return;
#endif
    if(game_lib != -1)
	{
		int status;

		int ret;
#if 0
	    if(kernel_modmgr)
        {
		   ret = sysKernelStopModule(game_lib, 0, 0, &status, NULL);
		}
		else
#endif
		{
		   ret = sceKernelStopModule(game_lib, 0, 0, &status, NULL);
		}
		if(ret < 0)
		   Sys_Error("Sys_UnloadGame: Error stop module (%08x)", ret);
#if 0
	    if(kernel_modmgr)
        {
		   ret = sysKernelUnloadModule(game_lib);

		}
		else
#endif
		{
           ret = sceKernelUnloadModule(game_lib);
		}


		if(ret < 0)
		   Sys_Error("Sys_UnloadGame: Error unload module (%08x)", ret);
	}
	game_lib = -1;

	int d = sceKernelTotalFreeMemSize();
    int b = sceKernelMaxFreeMemSize();
	Com_Printf("Heap Free: total: %i max: %i\n", d, b);

}

void* Sys_GetRefAPI(void* params) 
{
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	static const char* ref_name  = "refpspgu.prx";

	if(ref_lib) 
	{
		Com_Error(ERR_FATAL, "Sys_GetRefAPI without Sys_UnloadGame");
	}

	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "%s", ref_name);

	Com_Printf("REF_LIB: %d\n", ref_lib);

	ref_lib = sceKernelLoadModule(name, 0, 0);
	if(ref_lib <= 0) {
		Com_Printf("REF_LIB: %s %d 0x%08X\n", ref_name, ref_lib, ref_lib);
		Sys_Error("Unabled to load %s", name);
	}
	
	Com_Printf("REF_LIB: %d\n", ref_lib);

	int ref = sceKernelStartModule(ref_lib, 0, 0, 0, 0);
	Com_Printf("RETURN: %d\n", ref);
#else
	ref_lib = 1;
#endif
	return GetRefAPI(params);
}

void Sys_UnloadRef(void) 
{
#if defined(PSP_DYNAMIC_LINKING)
#error no dynamic linking
	if(ref_lib)
	{
		sceKernelStopModule(ref_lib, 0, 0, 0, 0);
		sceKernelUnloadModule(ref_lib);
	}
#endif
	ref_lib = 0;
}

void Sys_Error (char *error, ...)
{
	// Clear the sound buffer.
	S_ClearBuffer();

	// Put the error message in a buffer.
	va_list args;
	va_start(args, error);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer) - 1, error, args);
	va_end(args);

	Com_Printf(buffer);
	// Print the error message to the debug screen.
	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0xA0602000);
	pspDebugScreenClear();
	pspDebugScreenSetTextColor(0xffffff);
	pspDebugScreenPrintf("The following error occurred:\n");
	pspDebugScreenSetTextColor(0xffffff00);
	pspDebugScreenPrintData(buffer, strlen(buffer));
	pspDebugScreenSetTextColor(0xffffff);
	pspDebugScreenPrintf("\n\nPress CROSS to quit.\n");
	
	// Wait for a button press.
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	SceCtrlData pad;
	do 
	{
		sceCtrlReadBufferPositive(&pad, 1);
	} 
	while (pad.Buttons & PSP_CTRL_CROSS);
	do 
	{
		sceCtrlReadBufferPositive(&pad, 1);
	} 
	while ((pad.Buttons & PSP_CTRL_CROSS) == 0);
	do 
	{
		sceCtrlReadBufferPositive(&pad, 1);
	} 
	while (pad.Buttons & PSP_CTRL_CROSS);

	// Quit.
	pspDebugScreenPrintf("Shutting down...\n");

	CL_Shutdown();
	Qcommon_Shutdown();

	sceKernelExitGame();
}

void Sys_ConsoleOutput(char* string) 
{
	printf("%s", string);
}
qboolean stdin_active = true;

char *Sys_ConsoleInput(void)
{
  return NULL;
}

static int running   = 0;
static int suspended = 0;

/* Exit callback */
int exitCallback(int arg1, int arg2, void *common)
{
	running = 0;
	return 0;
}

int powerCallback(int unknown, int powerInfo, void* common)
{
	if (powerInfo & PSP_POWER_CB_POWER_SWITCH || powerInfo & PSP_POWER_CB_SUSPENDING)
	{
		suspended = 1;
	}
	else if (powerInfo & PSP_POWER_CB_RESUMING)
	{
	}
	else if (powerInfo & PSP_POWER_CB_RESUME_COMPLETE)
	{
		suspended = 0;
	}

	return 0;
}

/* Callback thread */
int callbackThread(SceSize args, void *argp)
{
	// Exit callback.
	const SceUID cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
	sceKernelRegisterExitCallback(cbid);

	// Power callback.
	const SceUID powerCallbackID = sceKernelCreateCallback("Power Callback", powerCallback, NULL);
	scePowerRegisterCallback(0, powerCallbackID);

	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
int setupCallbacks(void)
{
	int thid = 0;

	thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0) 
	{
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

//test mem
extern "C" void Mem_Free_View (void)
{
	int sizeptr = 1024 * 1024;
	printf("-------------Mem Testing---------------\n");
	printf("----------------Test 1-----------------\n");
	printf("-----------------RAM-------------------\n");
	while(1)
	{
		void *ptr = malloc(sizeptr);
		if(!ptr)
	    {
	         Com_Printf("MemFree RAM: %i\n", sizeptr - 1024 * 1024); // - 1mb
	         printf("MemFree RAM: %i\n", sizeptr - 1024 * 1024);
			 sizeptr = 1024 * 1024;
			 ptr = NULL;
			 break;
 		}
		free(ptr);	  
	    ptr = NULL;
		sizeptr += 1024 * 1024; // + 1mb
	}
 	printf("--------------Test 1 end---------------\n");
	return;
	printf("----------------Test 2-----------------\n");
	printf("-----------------VRAM------------------\n");
	while(1)
	{
		void *ptr = valloc(sizeptr);
		if(!ptr)
	    {
             Com_Printf("MemFree VRAM: %i\n", sizeptr - 1024 * 1024); // - 1mb
             printf("MemFree VRAM: %i\n", sizeptr - 1024 * 1024);
			 sizeptr = 0;
			 ptr = NULL;
			 break;
 		}
		printf("Alloc %i\n", sizeptr);
		vfree(ptr);	  
	    ptr = NULL;
		sizeptr += 1024 * 1024; // + 1mb
	}
  printf("--------------Test 2 end---------------\n");
}

void Sys_ReadCommandLineFile (char* netpath)
{
	SceUID 	in;
	int     count;
	char    buf[4096];
	int     argc = 1;

	in = sceIoOpen(netpath, PSP_O_RDONLY, 0777);

	if (in)
	{
		count = sceIoRead(in, buf, 4096);

		f_argv[0] = empty_string;

		char* lpCmdLine = buf;

		while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
		{
			while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				f_argv[argc] = lpCmdLine;
				argc++;

				while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
					lpCmdLine++;

				if (*lpCmdLine)
				{
					*lpCmdLine = 0;
					lpCmdLine++;
				}
			}
		}
		f_argc = argc;
	}
	else
	{
		f_argc = 0;
	}

	if (in)
		sceIoClose(in);
}

int CheckParm (char **args, int argc, char *parm)
{
	int  i;

	for (i=1 ; i<argc ; i++)
	{
		if (!args[i])
			continue;               // NEXTSTEP sometimes clears appkit vars.
		if (!strcmp (parm,args[i]))
			return i;
	}

	return 0;
}

#if defined (SLIM) && defined (Q2K)
void Q2KInit(void)
{

   SceUID mod = pspSdkLoadStartModule("Q2Kernel.prx",PSP_MEMORY_PARTITION_KERNEL);

   if(mod >= 0)
   {
      Q2KernelInit(4*1024);
	  if(sceGeEdramGetSize() == 4 * 1024 * 1024)
	  {
		 printf("Q2Kernel.prx - init done\n");
	  }
	  else
	  {
		 Sys_Error("Please disable all plugins!\n Vram memory not allocated!\n");
	  }
   }
   else
   {
	  Sys_Error("Couldn't load Q2Kernel.prx\n");
   }
}
#endif

int main(int argc, char** argv)
{
    int 	time, oldtime, newtime;

	disableFloatingPointExceptions();
	
	setupCallbacks();

#if defined (SLIM) && defined (Q2K)
	Q2KInit();
#endif

	// Get the current working dir.
	char currentDirectory[1024];
	char gameDirectory[1024];

	memset(gameDirectory, 0, sizeof(gameDirectory));
	memset(currentDirectory, 0, sizeof(currentDirectory));
	getcwd(currentDirectory, sizeof(currentDirectory) - 1);

	char   path_f[256];
	strcpy(path_f,currentDirectory);
	strcat(path_f,"/quake2.ini");
	Sys_ReadCommandLineFile(path_f);

	char *args[MAX_NUM_ARGVS];

	for (int k =0; k < f_argc; k++)
	{
		int len = strlen(f_argv[k]);
		args[k] = new char[len+1];
		strcpy(args[k], f_argv[k]);
	}

	if (CheckParm(args, f_argc, "-noregsound"))
	{
		noregsound = 1;
	}

	if (CheckParm(args, f_argc, "-maphunk"))
	{
		char* tempStr = args[CheckParm(args, f_argc, "-maphunk")+1];
	    maphunk = atoi(tempStr);
	    printf("Map Hunk set to: %i mb\n", maphunk);
	}
	else
#ifdef PSP_PHAT
	    maphunk = 6;
#else
        maphunk = 9;
#endif
/*
	// Catch exceptions from here.
	try
	{
*/	
			// Main Quake Init Call
			Qcommon_Init(f_argc, args);
			running = 1;
	        oldtime = Sys_Milliseconds ();
	        while (running)
	        {
                // suspend & resume system
				if (suspended)
				{
				   S_ClearBuffer();

				   FS_Suspend();
				   CDAudio_SysSuspend();

				   while (suspended && running)
				   {
						sceDisplayWaitVblankStart();
				   }

				   FS_Resume();
				   CDAudio_SysResume();
				   
					   
				   oldtime = Sys_Milliseconds ();
				 }

				 //  find time spent rendering last frame
		         do
		         {
			        newtime = Sys_Milliseconds ();
			        time = newtime - oldtime;
		         } while (time < 1);

	             Qcommon_Frame (time);
		         oldtime = newtime;
	        }
 /* 
	}
	catch (const std::exception& e)
	{
		// Report the error and quit.
		Sys_Error("C++ Exception: %s", e.what());
		return 0;
	}
*/
	Sys_Quit();
#if 0
	sceKernelStopModule(sysLib, 0, 0, 0, 0);
	sceKernelUnloadModule(sysLib);
#endif
	return 0;
}



