TARGET = gamepsp
OBJS = \
main.o \
q_shared.o \
g_ai.o \
p_client.o \
g_cmds.o \
g_svcmds.o \
g_chase.o \
g_combat.o \
g_func.o \
g_items.o \
g_main.o \
g_misc.o \
g_monster.o \
g_phys.o \
g_save.o \
g_spawn.o \
g_target.o \
g_trigger.o \
g_turret.o \
g_utils.o \
g_weapon.o \
m_actor.o \
m_berserk.o \
m_boss2.o \
m_boss3.o \
m_boss31.o \
m_boss32.o \
m_brain.o \
m_chick.o \
m_flipper.o \
m_float.o \
m_flyer.o \
m_gladiator.o \
m_gunner.o \
m_hover.o \
m_infantry.o \
m_insane.o \
m_medic.o \
m_move.o \
m_mutant.o \
m_parasite.o \
m_soldier.o \
m_supertank.o \
m_tank.o \
p_hud.o \
p_trail.o \
p_view.o \
p_weapon.o \
m_flash.o

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_NEWLIB_LIBC = 1

INCDIR =

CFLAGS = -O3 -G0 -Wall -ffast-math -fno-strict-aliasing -Dstricmp=strcasecmp -g -DKLIBC
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lc -lm -lpspuser

LDFLAGS = -nostartfiles

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

	
