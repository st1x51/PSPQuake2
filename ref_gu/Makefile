TARGET = pspref
OBJS = \
main.o \
../game/q_shared.o \
clipping.o \
math.o \
../psp/hw_imp.o \
../psp/HunkLib.o \
gu_image.o \
gu_draw.o \
gu_mesh.o \
gu_model.o \
gu_main.o \
gu_misc.o \
gu_surf.o \
gu_light.o \
gu_part.o \
gu_warp.o \
gu_sky.o \
gu_dxt.o \
vram.o

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_NEWLIB_LIBC = 1

INCDIR = 
CFLAGS = -O2 -G0 -Wall -fno-strict-aliasing -Dstricmp=strcasecmp -g -DREF_LINKED
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgum_vfpu -lpspvfpu -lpspgu -lstdc++ -lm -lc -lpspuser

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

	
