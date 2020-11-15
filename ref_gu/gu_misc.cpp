#include "gu_local.h"

/*
==================
R_InitParticleTexture
==================
*/
static unsigned char __attribute__((aligned(16))) tex_notex[128] = {
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define X 0x0f,
#define o 0xff,

static unsigned char __attribute__((aligned(16))) tex_particle[256] = {
	o o o o o o o o o o o o o o o o
	o o o o o o X X X X o o o o o o
	o o o o X X X X X X X X o o o o
	o o o X X X X X X X X X X o o o
	o o X X X X X X X X X X X X o o
	o o X X X X X X X X X X X X o o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o X X X X X X X X X X X X X X o
	o o X X X X X X X X X X X X o o
	o o X X X X X X X X X X X X o o
	o o o X X X X X X X X X X o o o
	o o o o X X X X X X X X o o o o
	o o o o o o X X X X o o o o o o
	o o o o o o o o o o o o o o o o
};

#undef X
#undef o

void R_InitParticleTexture (void)
{
    r_notexture = GL_LoadPic ("***r_notexture***", 8, 8, (byte*)tex_notex, true, GU_LINEAR, 0, it_wall, 8);
	r_particletexture = GL_LoadPic ("***particle***", 16, 16, (byte*)tex_particle, true, GU_LINEAR, 0,  it_sprite, 8);
}

extern unsigned r_rawpalette[256];
void GU_DefaultState(void) 
{
	sceGuFrontFace(GU_CW);
	sceGuEnable(GU_TEXTURE_2D);

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuDisable(GU_BLEND);

	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuShadeModel(GU_FLAT);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    	
    sceGuColor(0xffffffff);
	sceGuEnable(GU_COLOR_TEST);
	sceGuColorFunc(GU_NOTEQUAL, 0xffffff, 0xffffff);
   	sceGuAmbientColor(0xffffffff);

	//Set the default palette
    GL_SetTexturePalette(r_rawpalette);
}

void GU_RotateForEntity(entity_t* e) 
{
	ScePspFVector3 trn = 
	{
		e->origin[0], 
		e->origin[1],
		e->origin[2]
	};
	sceGumTranslate(&trn);

	ScePspFVector3 rot = 
	{
		-e->angles[ROLL] * (GU_PI / 180.0f),
		-e->angles[PITCH] * (GU_PI / 180.0f),
		 e->angles[YAW] * (GU_PI / 180.0f)
	};
	sceGumRotateZYX(&rot);
		
}
