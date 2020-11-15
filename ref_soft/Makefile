TARGET = gamepsp
OBJS = \
main.o \
../psp/sw_gu.o \
../game/q_shared.o \
r_aclip.o \
r_alias.o \
r_bsp.o \
r_draw.o \
r_edge.o \
r_image.o \
r_light.o \
r_main.o \
r_misc.o \
r_model.o \
r_part.o \
r_poly.o \
r_polyse.o \
r_rast.o \
r_scan.o \
r_sprite.o \
r_surf.o

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_NEWLIB_LIBC = 1

INCDIR =

CFLAGS = -O3 -G0 -Wall -ffast-math -fno-strict-aliasing -g
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lc -lm -lpspuser -lpspgu -lpspgum

LDFLAGS = -mno-crt0 -nostartfiles

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

	
