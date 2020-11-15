	.set noreorder

#include "pspstub.s"

	STUB_START "gamepsp",0x00090000,0x00010005
	STUB_FUNC  0x495C3DF5,GetGameAPI
	STUB_END
