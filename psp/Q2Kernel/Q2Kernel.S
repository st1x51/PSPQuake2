	.set noreorder

#include "pspstub.s"

	STUB_START "Q2Kernel",0x40090000,0x00030005
	STUB_FUNC  0xC14FCEB4,Q2KernelInit
	STUB_FUNC  0x6B839382,Q2KernelSetVideoOut
	STUB_FUNC  0x6751A414,Q2KernelCheckVideoOut
	STUB_END
