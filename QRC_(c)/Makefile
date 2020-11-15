TARGET = qIIcc
OBJS = main.o

INCDIR = ../../include
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

LIBDIR = ../../lib
LDFLAGS = 
LIBS = ../psp/m33libs/libpspkubridge.a ../psp/m33libs/libpspsystemctrl_user.a

PSP_FW_VERSION = 500
BUILD_PRX = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = qIIcc

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

