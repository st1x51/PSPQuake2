#include <pspmoduleexport.h>
#define NULL ((void *) 0)

void extern GetRefAPI;
static const unsigned int __pspref_exports[2] __attribute__((section(".rodata.sceResident"))) = {
	0xA87EB57C,
	(unsigned int) &GetRefAPI,
};

const struct _PspLibraryEntry __library_exports[1] __attribute__((section(".lib.ent"), used)) = {
	{ "pspref", 0x0000, 0x0001, 4, 0, 1, &__pspref_exports },
};
