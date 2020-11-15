	.set noreorder

#include "pspstub.s"

	STUB_START "pspref",0x00090000,0x00010005
	STUB_FUNC  0xA87EB57C,GetRefAPI
	STUB_END
