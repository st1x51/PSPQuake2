	.set noreorder

#include "pspstub.s"

	STUB_START "system_extender_driver",0x00090000,0x00060005
	STUB_FUNC  0x35F57440,sysKernelLoadModule
	STUB_FUNC  0x3456231A,sysKernelLoadModuleByID
	STUB_FUNC  0x1CC3F506,sysKernelStartModule
	STUB_FUNC  0x9625E0C5,sysKernelStopModule
	STUB_FUNC  0xC6D5D8F6,sysKernelUnloadModule
	STUB_FUNC  0xF43662DA,sysKernelQueryModuleInfo
	STUB_END
