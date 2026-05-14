/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2023 Sergey Galushko

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gu_mesh.c: triangle model functions

#include "gu_local.h"

#define DRAW_BBOX 0

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

extern	vec3_t	lightspot;

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static vec4_t	s_lerped[MAX_VERTS] __attribute__((aligned(16)));
static vec4_t	shadelight;

#if 1
static inline void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	if (!nverts)
		return;

	__asm__ volatile (
		".set		push\n"				// save assembler option
		".set		noreorder\n"		// suppress reordering
		"lv.s		S010,  0(%0)\n"		// S010 = backv[0]
		"lv.s		S011,  0(%1)\n"		// S011 = frontv[0]
		"lv.s		S012,  0(%2)\n"		// S013 = move[0]
		"lv.s		S020,  4(%0)\n"		// S020 = backv[1]
		"lv.s		S021,  4(%1)\n"		// S021 = frontv[1]
		"lv.s		S022,  4(%2)\n"		// S023 = move[1]
		"lv.s		S030,  8(%0)\n"		// S030 = backv[2]
		"lv.s		S031,  8(%1)\n"		// S031 = frontv[2]
		"lv.s		S032,  8(%2)\n"		// S033 = move[2]
		"lv.s		S000,  0(%3)\n"		// S000 = ov->v[3] and ov->lightnormalindex (first)
		"lv.s		S002,  0(%4)\n"		// S002 = v->v[3] and ov->lightnormalindex  (first)
		".set		pop\n"				// restore assembler option
		::	"r"(backv),
			"r"(frontv),
			"r"(move),
			"r"(ov->v),
			"r"(v->v)
	);

	//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"vfim.s		S013, "M_ATOS(POWERSUIT_SCALE)"\n"
			"vmov.p		R023, R003[Y,Y]\n"		// S013 = S023 = S033 = POWERSUIT_SCALE
			".set		pop\n"					// restore assembler option
		);

		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4 )
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				".word		0xD03880A4\n"			// vuc2i.s	R100, S000 !!!
				".word		0xD03840A5\n"			// vuc2i.s	R101, S002 !!!
				"vi2f.t		R100, R100, 23\n"		// R100 = float(R100)
				"vi2f.t		R101, R101, 23\n"		// R101 = float(R101)
				"lv.s		S102,  0(%3)\n"			// S102 = normal[0]
				"lv.s		S112,  4(%3)\n"			// S112 = normal[1]
				"lv.s		S122,  8(%3)\n"			// S122 = normal[2]
				"lv.s		S000,  4(%1)\n"			// S000 = ov->v[3] and ov->lightnormalindex (next)
				"lv.s		S002,  4(%2)\n"			// S002 = v->v[3] and ov->lightnormalindex  (next)
				"vhdp.q		S200, C100, C010[X,Y,W,Z]\n"		// ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE + move[0]
				"vhdp.q		S201, C110, C020[X,Y,W,Z]\n"		// ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE + move[1]
				"vhdp.q		S202, C120, C030[X,Y,W,Z]\n"		// ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE + move[2]
				"sv.q		C200,  0(%0)\n"			// lerp -> (vec4_t) !!!
				".set		pop\n"					// restore assembler option
				::	"r"(lerp),
					"r"(ov->v),
					"r"(v->v),
					"r"(normal)
			);
		}
	}
	else
	{
		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
		{
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				".word		0xD03880A4\n"			// vuc2i.s	R100, S000 !!!
				".word		0xD03840A5\n"			// vuc2i.s	R101, S002 !!!
				"vi2f.t		R100, R100, 23\n"		// R100 = float(R100)
				"vi2f.t		R101, R101, 23\n"		// R101 = float(R101)
				"lv.s		S000,  4(%1)\n"			// S000 = ov->v[3] and ov->lightnormalindex (next)
				"lv.s		S002,  4(%2)\n"			// S002 = v->v[3] and ov->lightnormalindex  (next)
				"vhdp.t		S200, C100, C010\n"		// ov->v[0]*backv[0] + v->v[0]*frontv[0] + move[0]
				"vhdp.t		S201, C110, C020\n"		// ov->v[1]*backv[1] + v->v[1]*frontv[1] + move[1]
				"vhdp.t		S202, C120, C030\n"		// ov->v[2]*backv[2] + v->v[2]*frontv[2] + move[2]
				"sv.q		C200,  0(%0)\n"			// lerp -> (vec4_t) !!!
				".set		pop\n"					// restore assembler option
				::	"r"(lerp),
					"r"(ov->v),
					"r"(v->v)
			);
		}
	}
}
#else
void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
{
	int i;

	//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{
		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4 )
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];

			lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE;
		}
	}
	else
	{
		for (i=0 ; i < nverts; i++, v++, ov++, lerp+=4)
		{
			lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0];
			lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1];
			lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2];
		}
	}

}
#endif

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp)
{
	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t		*v, *ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	float	alpha;
	float	*norm;
	vec3_t	move, delta, vectors[3];
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp;
	int		prim;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ currententity->frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ currententity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if (currententity->flags & RF_TRANSLUCENT)
		alpha = currententity->alpha;
	else
		alpha = 1.0;

	frontlerp = 1.0 - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);
	AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct (delta, vectors[0]);	// forward
	move[1] = -DotProduct (delta, vectors[1]);	// left
	move[2] = DotProduct (delta, vectors[2]);	// up

	VectorAdd (move, oldframe->translate, move);

	for (i=0 ; i<3 ; i++)
	{
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i];
	}

	for (i=0 ; i<3 ; i++)
	{
		frontv[i] = frontlerp*frame->scale[i];
		backv[i] = backlerp*oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GL_LerpVerts( paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv );

	// PMM - added double shell
	if (currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM))
	{
		sceGuDisable (GU_TEXTURE_2D);
		sceGuColor (GU_HCOLOR_C4F (shadelight[0], shadelight[1], shadelight[2], alpha));

		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done
			if (count < 0)
			{
				count = -count;
				prim = GU_TRIANGLE_FAN;
			}
			else
			{
				prim = GU_TRIANGLE_STRIP;
			}

			gu_vert_fv_t* const out = (gu_vert_fv_t*)sceGuGetMemory (sizeof(gu_vert_fv_t) * count);
			gu_vert_fv_t* out_ptr = out;
			for (i = 0; i < count; i++, order += 3, out_ptr++)
			{
				index_xyz = order[2];
				VectorCopy (s_lerped[index_xyz], out_ptr->xyz);
			}
			sceGuDrawArray (prim, GU_VERTEX_32BITF, count, 0, out);
		}

		sceGuColor (GU_HCOLOR_DEFAULT);
		sceGuEnable (GU_TEXTURE_2D);
	}
	else
	{
		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done
			if (count < 0)
			{
				count = -count;
				prim = GU_TRIANGLE_FAN;
			}
			else
			{
				prim = GU_TRIANGLE_STRIP;
			}

			gu_vert_ftnv_t* const out = (gu_vert_ftnv_t*)sceGuGetMemory (sizeof(gu_vert_ftnv_t) * count);
			gu_vert_ftnv_t* out_ptr = out;
			for (i = 0; i < count; i++, order += 3, out_ptr++)
			{
				index_xyz = order[2];

				// texture coordinates come from the draw list
				out_ptr->u = ((float *)order)[0];
				out_ptr->v = ((float *)order)[1];

				// normals
				norm = r_avertexnormals[verts[index_xyz].lightnormalindex];
				VectorCopy (norm, out_ptr->nxyz);

				// vert
				VectorCopy (s_lerped[index_xyz], out_ptr->xyz);
			}
			sceGuDrawArray (prim, GU_TEXTURE_32BITF | GU_NORMAL_32BITF | GU_VERTEX_32BITF, count, 0, out);
		}
	}
}


#if 1
/*
=============
GL_DrawAliasShadow
=============
*/
void GL_DrawAliasShadow (dmdl_t *paliashdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	shadevector;
	float	an, height, lheight;
	int		count;
	daliasframe_t	*frame;
	int		i;
	int		prim;
	int		index;

	an = currententity->angles[YAW] / 180 * M_PI;
	shadevector[0] = cos (-an);
	shadevector[1] = sin (-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	lheight = currententity->origin[2] - lightspot[2];

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
		+ currententity->frame * paliashdr->framesize);
	verts = frame->verts;

	height = 0;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			prim = GU_TRIANGLE_FAN;
		}
		else
		{
			prim = GU_TRIANGLE_STRIP;
		}

		gu_vert_fv_t* const out = (gu_vert_fv_t*)sceGuGetMemory (sizeof(gu_vert_fv_t) * count);
		for (i = 0; i < count; i++)
		{
			// normals and vertexes come from the frame list
/*
			point[0] = verts[order[2]].v[0] * frame->scale[0] + frame->translate[0];
			point[1] = verts[order[2]].v[1] * frame->scale[1] + frame->translate[1];
			point[2] = verts[order[2]].v[2] * frame->scale[2] + frame->translate[2];
*/

			memcpy( out[i].xyz, s_lerped[order[2]], sizeof( out->xyz )  );

			out[i].xyz[0] -= shadevector[0] * (out[i].xyz[2] + lheight);
			out[i].xyz[1] -= shadevector[1] * (out[i].xyz[2] + lheight);
			out[i].xyz[2] = height;
//			height -= 0.001;

			order += 3;

//			verts++;
		}

		sceGuDrawArray (prim, GU_VERTEX_32BITF, count, 0, out);
	}
}

#endif

/*
=============
R_CullAliasModel
=============
*/
static qboolean R_CullAliasModel (dmdl_t *paliashdr, vec3_t mins, vec3_t maxs, entity_t *e)
{
	int			i, clipstatus;
	vec3_t		vectors[3];
	float		thismm, oldmm;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t		angles;

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n",
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n",
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
										e->frame * paliashdr->framesize);

	poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
										e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255.0f;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismm = pframe->translate[i];
			oldmm  = poldframe->translate[i];
			mins[i] = Q_min (thismm, oldmm);

			thismm += pframe->scale[i] * 255.0f;
			oldmm  += poldframe->scale[i] * 255.0f;
			maxs[i] = Q_max (thismm, oldmm);
		}
	}
	GU_ClipTransformAABBFromModelMatrix (mins, maxs, mins, maxs);

	return (GU_ClipCullBox(mins, maxs) == GU_CULL_OUTSIDE);
}

/*
=============
R_SetupAliasLighting
=============
*/
static qboolean R_SetupAliasLighting (void)
{
	dlight_t	*best_dls[MAX_VLIGHTS];
	int			i, num_dls;
	float		r, g, b;

	ScePspFVector3	sun_dir = { -0.5f, 0.8f, 0.5f };
	qboolean		flat_light = currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_FULLBRIGHT );


	if ( flat_light )
	{
		if ( currententity->flags & RF_FULLBRIGHT )
		{
			VectorSet (shadelight, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			VectorClear (shadelight);
			if (currententity->flags & RF_SHELL_HALF_DAM)
			{
					shadelight[0] = 0.56;
					shadelight[1] = 0.59;
					shadelight[2] = 0.45;
			}
			if ( currententity->flags & RF_SHELL_DOUBLE )
			{
				shadelight[0] = 0.9;
				shadelight[1] = 0.7;
			}
			if ( currententity->flags & RF_SHELL_RED )
				shadelight[0] = 1.0;
			if ( currententity->flags & RF_SHELL_GREEN )
				shadelight[1] = 1.0;
			if ( currententity->flags & RF_SHELL_BLUE )
				shadelight[2] = 1.0;
		}
	}
	else
	{
		R_GetWorldLight (currententity->origin, shadelight);

		sun_dir.x = lightspot[0];
		sun_dir.y = lightspot[1];
		sun_dir.z = lightspot[2];

		// player lighting hack for communication back to server
		// big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
					r_lightlevel->value = 150.0f * shadelight[0];
				else
					r_lightlevel->value = 150.0f * shadelight[2];
			}
			else
			{
				if (shadelight[1] > shadelight[2])
					r_lightlevel->value = 150.0f * shadelight[1];
				else
					r_lightlevel->value = 150.0f * shadelight[2];
			}

		}

		if ( gl_monolightmap->string[0] != '0' )
		{
			float s = Q_max3(shadelight[0], shadelight[1], shadelight[2]);
			shadelight[0] = s;
			shadelight[1] = s;
			shadelight[2] = s;
		}
	}


	// PGM	ir goggles color override
	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}
	else
	{
		if ( currententity->flags & RF_MINLIGHT )
		{
			for (i = 0; i < 3; i++)
			{
				if (shadelight[i] > 0.1)
					break;
			}
			if (i == 3)
			{
				shadelight[0] = 0.1;
				shadelight[1] = 0.1;
				shadelight[2] = 0.1;
			}
		}

		if ( currententity->flags & RF_GLOW )
		{	// bonus items will pulse with time
			float	scale;
			float	min;

			scale = 0.1f * sinf (r_newrefdef.time * 7);
			for (i=0; i < 3; i++)
			{
				min = shadelight[i] * 0.8;
				shadelight[i] += scale;
				if (shadelight[i] < min)
					shadelight[i] = min;
			}
		}
	}

	if (flat_light)
		return false;

	sceGuEnable (GU_LIGHTING);
	sceGuAmbient (GU_HCOLOR_4F(
		shadelight[0] * 0.5f,
		shadelight[1] * 0.5f,
		shadelight[2] * 0.5f,
		1.0f));

	sceGuEnable (GU_LIGHT0);
	sceGuLight (0, GU_DIRECTIONAL, GU_DIFFUSE, &sun_dir);
	sceGuLightColor (0, GU_DIFFUSE, GU_HCOLOR_C4F(
		shadelight[0],
		shadelight[1],
		shadelight[2],
		1.0f));

	R_GetBestDynamicLights (currententity->origin, best_dls, &num_dls);

	for (i = 0; i < MAX_VLIGHTS; i++)
	{
		int slot = i + 1;
		if (i < num_dls)
		{
			sceGuEnable (GU_LIGHT0 + slot);

			r = best_dls[i]->color[0];
			g = best_dls[i]->color[1];
			b = best_dls[i]->color[2];

			float intensity = best_dls[i]->intensity;
			if (intensity <= 0)
				intensity = 1.0f;

			sceGuLight (slot, GU_POINTLIGHT, GU_DIFFUSE, (const ScePspFVector3 *)best_dls[i]->origin);
			sceGuLightColor (slot, GU_DIFFUSE, GU_HCOLOR_3F(r, g, b));
			sceGuLightAtt (slot, 1.0f, 1.0f / intensity, 2.0f / (intensity * intensity));
		}
		else
			sceGuDisable (GU_LIGHT0 + slot);

	}

	sceGuColorMaterial (GU_AMBIENT | GU_DIFFUSE);
	sceGuModelColor (0xFF404040, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);

	return true;
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i, flags;
	dmdl_t		*paliashdr;
	float		an;
	vec3_t		mins, maxs;
	image_t		*skin;
	qboolean	light_en;
	ScePspFVector3	scale;

	flags = e->flags;

	if (flags & RF_WEAPONMODEL)
	{
		if ( r_lefthand->value == 2 )
			return;
	}

	paliashdr = (dmdl_t *)Mod_Extradata (currentmodel);

	sceGumPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.
	R_RotateForEntity (e); // &proj update
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.

	if (!(flags & RF_WEAPONMODEL))
	{
		if (R_CullAliasModel (paliashdr, mins, maxs, e))
		{
			sceGumPopMatrix ();
			sceGumUpdateMatrix ();
			return;
		}
	}

	//
	// get lighting information
	//
	light_en = R_SetupAliasLighting ();

	//
	// locate the proper data
	//

	c_alias_polys += paliashdr->num_tris;

	//
	// draw all the triangles
	//
	if (flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		sceGuDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	if ((flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0f))
	{
		sceGumMatrixMode (GU_PROJECTION);
		sceGumPushMatrix ();
		sceGumLoadIdentity ();

		scale.x = -1.0f;
		scale.y =  1.0f;
		scale.z =  1.0f;

		sceGumScale (&scale);
		sceGumPerspective (r_newrefdef.fov_y, (float)r_newrefdef.width / r_newrefdef.height, 4.0f, 4096.0f);

		sceGumMatrixMode (GU_MODEL);

		sceGuFrontFace (GU_CCW); // GL_BACK
	}

	// select skin
	if (currententity->skin)
		skin = currententity->skin;	// custom player skin
	else
	{
		if (currententity->skinnum >= MAX_MD2SKINS)
			skin = currentmodel->skins[0];
		else
		{
			skin = currentmodel->skins[currententity->skinnum];
			if (!skin)
				skin = currentmodel->skins[0];
		}
	}

	GL_Bind (skin);

	// draw it
	sceGuShadeModel (GU_SMOOTH);

	GL_TexEnv (GU_TFX_MODULATE);
	if (flags & RF_TRANSLUCENT)
		sceGuEnable (GU_BLEND);

	if ((currententity->frame >= paliashdr->num_frames) || (currententity->frame < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n",
			currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if ((currententity->oldframe >= paliashdr->num_frames) || (currententity->oldframe < 0))
	{
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
			currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if (!r_lerpmodels->value)
		currententity->backlerp = 0;
	GL_DrawAliasFrameLerp (paliashdr, currententity->backlerp);

	GL_TexEnv (GU_TFX_REPLACE);
	sceGuShadeModel (GU_FLAT);

	sceGumPopMatrix();

	if ((flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0f))
	{
		sceGumMatrixMode (GU_PROJECTION);
		sceGumPopMatrix ();
		sceGumMatrixMode (GU_MODEL);
		sceGuFrontFace (GU_CW); // GL_FRONT
	}

	if (flags & RF_TRANSLUCENT)
		sceGuDisable (GU_BLEND);

	if (flags & RF_DEPTHHACK)
		sceGuDepthRange (gldepthmin, gldepthmax);

	if (light_en)
		sceGuDisable (GU_LIGHTING);
#if 1
	if (gl_shadows->value && !(flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
	{
		sceGumPushMatrix ();
		R_RotateForEntity (e);
		sceGuDisable (GU_TEXTURE_2D);
		sceGuEnable (GU_BLEND);
		sceGuColor (GU_HCOLOR_4F(0.0, 0.0, 0.0, 0.5));
		GL_DrawAliasShadow (paliashdr, currententity->frame );
		sceGuEnable (GU_TEXTURE_2D);
		sceGuDisable (GU_BLEND);
		sceGumPopMatrix ();
	}
#endif
	sceGumUpdateMatrix();

	sceGuColor (GU_HCOLOR_DEFAULT);
#if CULLING_DEBUGGING
	if (!(flags & RF_WEAPONMODEL))
		DEBUG_DrawAliasAABB (mins, maxs);
#endif
}


