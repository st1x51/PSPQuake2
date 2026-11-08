/*
===============
GL_DrawPointParticles
===============
*/
void GL_DrawPointParticles (int num_particles, const particle_t *particles)
{
	const particle_t	*p;
	int					i;

	sceGuDepthMask (GU_TRUE);
	sceGuEnable (GU_BLEND);
	sceGuDisable (GU_TEXTURE_2D);

	gu_vert_fcv_t* const out = (gu_vert_fcv_t*)sceGuGetMemory (sizeof(gu_vert_fcv_t) * num_particles);
	gu_vert_fcv_t* out_ptr = out;

	for (i = 0, p = particles; i < num_particles; i++, p++)
	{
		out_ptr->c = d_8to24table[p->color];
		out_ptr->a = p->alpha * 255;
	
		VectorCopy (p->origin, out_ptr->xyz);
	
		out_ptr++;
	}
	sceGuDrawArray (GU_POINTS, GU_COLOR_8888 | GU_VERTEX_32BITF, num_particles, 0, out);

	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuDepthMask (GU_FALSE);
	sceGuEnable (GU_TEXTURE_2D);
}

/*
===============
GL_DrawSpriteParticles
===============
*/
void GL_DrawSpriteParticles (int num_particles, const particle_t *particles)
{
	const particle_t	*p;
	int					i;
	vec3_t				up, right;
	float				scale;

    GL_Bind (r_particletexture);

	sceGuDepthMask (GU_TRUE);		// no z buffering
	sceGuEnable (GU_BLEND);
	GL_TexEnv (GU_TFX_MODULATE);

	//VectorScale (vup, 1.5, up);
	//VectorScale (vright, 1.5, right);

	gu_vert_ftcv_t* const out = (gu_vert_ftcv_t*)sceGuGetMemory (sizeof(gu_vert_ftcv_t) * num_particles * 2);
	gu_vert_ftcv_t* out_ptr = out;

	__asm__ volatile (
		".set		push\n"
		".set		noreorder\n"
		//"break\n"
		"lv.s		S000, 0(%0)\n" // C000 - vup
		"lv.s		S001, 4(%0)\n"
		"lv.s		S002, 8(%0)\n"
		"lv.s		S010, 0(%1)\n" // C010 - vright
		"lv.s		S011, 4(%1)\n"
		"lv.s		S012, 8(%1)\n"
		"lv.s		S020, 0(%2)\n" // C020 - vpn
		"lv.s		S021, 4(%2)\n"
		"lv.s		S022, 8(%2)\n"
		"lv.s		S030, 0(%3)\n" // C030 - r_origin
		"lv.s		S031, 8(%3)\n"
		"lv.s		S032, 4(%3)\n"
		"vadd.t		C000, C000, C010\n" // C000 - vup + vright
		".set		pop\n"
		::	"r"(vup), "r"(vright), "r"(vpn), "r"(r_origin) 
		:	"memory"
	);

	for (i = 0, p = particles; i < num_particles; i++, p++, out_ptr += 2)
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = gl_particle_scale->value;
		else
			scale = gl_particle_scale->value + scale * 0.004f;

		__asm__ volatile (
			".set		push\n"
			".set		noreorder\n"
			"lv.s		S100, 0(%2)\n" // C100 - p->origin
			"lv.s		S101, 4(%2)\n"
			"lv.s		S102, 8(%2)\n"
			"vsub.t		C110, C100, C030\n" // C110 = p->origin - r_origin
			//"vdot.t		S103, C110, C020\n" // S103 = (p->origin - r_origin) * vpn
			"vone.s		S103\n"
			"vscl.t		C120, C000, S103\n" // C120 = (vup + vright) * scale
			"vadd.t		C110, C100, C120\n" // C110 = p->origin + (vup + vright) * scale
			"vsub.t		C120, C100, C120\n" // C110 = p->origin - (vup + vright) * scale
			"sv.s		S110, 0 + %0\n"
			"sv.s		S111, 4 + %0\n"
			"sv.s		S112, 8 + %0\n"
			"sv.s		S120, 0 + %1\n"
			"sv.s		S121, 4 + %1\n"
			"sv.s		S122, 8 + %1\n"
			".set		pop\n"
			:	"=m"(out_ptr[0].xyz), "=m"(out_ptr[1].xyz)
			:	"r"(p->origin)
			:	"memory"
		);

		//VectorScale (vup, scale, up);
		//VectorScale (vright, scale, right);

		out_ptr[0].u = 0.0f;
		out_ptr[0].v = 0.0f;
		out_ptr[0].c = d_8to24table[p->color];
		out_ptr[0].a = p->alpha * 255; // replace alpha
		//point[0].x = p->origin[0] + right[0] + up[0];
		//point[0].y = p->origin[1] + right[1] + up[1];
		//point[0].z = p->origin[2] + right[2] + up[2];

		out_ptr[1].u = 1.0f;
		out_ptr[1].v = 1.0f;
		out_ptr[1].c = out_ptr[0].c;
		//point[1].x = p->origin[0] - right[0] - up[0];
		//point[1].y = p->origin[1] - right[1] - up[1];
		//point[1].z = p->origin[2] - right[2] - up[2];
	}
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF, num_particles * 2, 0, out);

	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuDepthMask (GU_FALSE);		// back to normal Z buffering
	GL_TexEnv (GU_TFX_REPLACE);
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if ( gl_particle_point->value )
		GL_DrawPointParticles (r_newrefdef.num_particles, r_newrefdef.particles);
	else
		GL_DrawSpriteParticles (r_newrefdef.num_particles, r_newrefdef.particles);
}


/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256] __attribute__((aligned(16)));

void R_SetPalette (const unsigned char *palette)
{
	int		i;
	byte	color[4];

	byte *rp = (byte *)r_rawpalette;

	if ( palette )
	{
		for (i = 0; i < 256; i++)
		{

			rp[i * 4 + 0] = gl_state.gammatable[palette[i * 3 + 0]];
			rp[i * 4 + 1] = gl_state.gammatable[palette[i * 3 + 1]];
			rp[i * 4 + 2] = gl_state.gammatable[palette[i * 3 + 2]];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = gl_state.gammatable[ d_8to24table[i] & 0xff];
			rp[i * 4 + 1] = gl_state.gammatable[(d_8to24table[i] >> 8) & 0xff];
			rp[i * 4 + 2] = gl_state.gammatable[(d_8to24table[i] >> 16) & 0xff];
			rp[i * 4 + 3] = (d_8to24table[i] >> 24) & 0xff;
		}
	}

	GL_SetTexturePalette (r_rawpalette);

	sceGuClearColor (0x00000000);
	sceGuClear (GU_COLOR_BUFFER_BIT | GU_FAST_CLEAR_BIT);
	sceGuClearColor (GU_HCOLOR_4F(1.0, 0.0, 0.5 , 0.5));
}