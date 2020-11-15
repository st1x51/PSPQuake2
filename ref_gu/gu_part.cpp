#include "gu_local.h"

void DrawSpriteParticles(void)
{
	const particle_t *p;
	int				i;
	byte			color[4];
	int             index = 0;

    GL_Bind(r_particletexture->texnum);
	sceGuDepthMask( GU_TRUE );		// no z buffering
	sceGuEnable( GU_BLEND );
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);

	vec3_t scale;
	VectorAdd(vup, vright, scale);

	struct vertex
	{
		float s, t;
		unsigned int color;
		float x, y, z;
	};
	vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * r_newrefdef.num_particles * 2));

	for ( p = r_newrefdef.particles, i=0 ; i < r_newrefdef.num_particles ; i++, p++)
	{
        vec3_t temp;
        
		*(int *)color = d_8to24table[p->color];
		color[3] = p->alpha*255;

		VectorAdd(p->origin, scale, temp);
        vertices[index].s = 0.0f;
		vertices[index].t = 0.0f;
		vertices[index].color = GU_RGBA(color[0], color[1], color[2], color[3]);
		vertices[index].x = temp[0];
		vertices[index].y = temp[1];
		vertices[index].z = temp[2];
		index++;

		VectorSubtract(p->origin, scale, temp);
		vertices[index].s = 1.0f;
		vertices[index].t = 1.0f;
		vertices[index].color = GU_RGBA(color[0], color[1], color[2], color[3]);
		vertices[index].x = temp[0];
		vertices[index].y = temp[1];
		vertices[index].z = temp[2];
		index++;
	}
	sceGumDrawArray(GU_SPRITES, GU_VERTEX_32BITF | GU_COLOR_8888 | GU_TEXTURE_32BITF | GU_TRANSFORM_3D, index, 0, vertices);


	sceGuDisable( GU_BLEND );
	sceGuColor (0xffffffff);          // back to normal color
	sceGuDepthMask(GU_FALSE);		  // back to normal Z buffering
	
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
}


static void DrawPointParticles(void) 
{
	int i;
	unsigned char color[4];
	const particle_t *p;
	
    sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_BLEND);
	sceGuDisable(GU_TEXTURE_2D);
	
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);

	// Allocate the vertices.
	struct vertex
	{
        unsigned int color;
		float x, y, z;
	};
    vertex* const vertices = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * r_newrefdef.num_particles));
	
	for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
	{
		*(int *)color = d_8to24table[p->color];
		color[3] = p->alpha*255;

		vertices[i].color = GU_RGBA(color[0], color[1], color[2], color[3]);
		
		vertices[i].x = p->origin[0];
		vertices[i].y = p->origin[1];
		vertices[i].z = p->origin[2];
	}
	sceGumDrawArray(GU_POINTS, GU_VERTEX_32BITF | GU_COLOR_8888 | GU_TRANSFORM_3D, r_newrefdef.num_particles, 0, vertices);

	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);

	sceGuDisable(GU_BLEND);
	sceGuColor (0xffffffff);           // back to normal color
	sceGuDepthMask(GU_FALSE);          // back to normal Z buffering
	sceGuEnable(GU_TEXTURE_2D);
}

void R_DrawParticles(void) 
{
	switch(int(gu_particletype->value))
	{
	case 0: //disable particles
	  break;
	case 1:
	default:
	  DrawPointParticles();
	  break;
	case 2:
	  DrawSpriteParticles();
	  break;
	}
}
