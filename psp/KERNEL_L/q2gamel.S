	.set noreorder

#include "pspstub.s"

	STUB_START "q2gamel",0x40090000,0x00020005
	STUB_FUNC  0x8551F08D,Q2_ModuleLoader
	STUB_FUNC  0xB39BCDD0,Q2_ModuleUnLoader
	STUB_END
