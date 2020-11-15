#include "gu_local.h"
#include "gu_model.h"

#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

typedef float vec4_t[4];

static	vec4_t	s_lerped[MAX_VERTS];

vec3_t	shadevector;
float	shadelight[3];

#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

float* shadedots = r_avertexnormal_dots[0];

unsigned int GU_colorFloat(float r, float g, float b, float a) {
#if 1
	if(r < 0.0f) r = 0.0f;
	if(r > 1.0f) r = 1.0f;
	if(g < 0.0f) g = 0.0f;
	if(g > 1.0f) g = 1.0f;
	if(b < 0.0f) b = 0.0f;
	if(b > 1.0f) b = 1.0f;
	if(a < 0.0f) a = 0.0f;
	if(a > 1.0f) a = 1.0f;
#endif
	return (((unsigned char)(r * 255)) <<  0)
		+  (((unsigned char)(g * 255)) <<  8)
		+  (((unsigned char)(b * 255)) << 16)
		+  (((unsigned char)(a * 255)) << 24);
}

void GU_LerpVerts(int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] ) 
{
	int i;

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

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GU_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp) 
{
	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t	*v, *ov, *verts;
	int		*order;
	int		count;
	float	frontlerp;
	float	alpha;
	vec3_t	move, delta, vectors[3];
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp;

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->frame * paliashdr->framesize
	);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames 
		+ currententity->oldframe * paliashdr->framesize
	);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if (currententity->flags & RF_TRANSLUCENT) 
	{
		alpha = currententity->alpha;
	} 
	else 
	{
		alpha = 1.0;
	}

	if(currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)) 
	{
		sceGuDisable(GU_TEXTURE_2D);
	}

	frontlerp = 1.0 - backlerp;

	VectorSubtract (currententity->oldorigin, currententity->origin, delta);
	AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct (delta, vectors[0]);	// forward
	move[1] = -DotProduct (delta, vectors[1]);	// left
	move[2] = DotProduct (delta, vectors[2]);	// up

	VectorAdd(move, oldframe->translate, move);

	for(i = 0; i < 3; i++) {
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i];
	}

	for (i = 0; i < 3; i++) {
		frontv[i] = frontlerp*frame->scale[i];
		backv[i] = backlerp*oldframe->scale[i];
	}

	lerp = s_lerped[0];

	GU_LerpVerts( paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv );

	while(1) 
	{
		int primitive = 0;

		count = *order++;
		if(!count)
			break;

		// PSP_CHECK
		if(count < 0) 
		{
			count = -count;
			primitive = GU_TRIANGLE_FAN;
		} 
		else 
		{
			primitive = GU_TRIANGLE_STRIP;
		}

		if(currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE )) 
		{
			typedef struct 
			{
				unsigned int c;
				float x, y, z;
			} VERT;

			VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * count);
			int i;
			for(i = 0; i < count; i++) {
				index_xyz = order[2];
				order += 3;

				l = shadedots[verts[index_xyz].lightnormalindex];
			
				int r,g,b;
				r = shadelight[0];
				g = shadelight[1];
				b = shadelight[2];
				
				v[i].c = GU_colorFloat(shadelight[0], shadelight[1], shadelight[2], alpha);
				v[i].x = s_lerped[index_xyz][0];
				v[i].y = s_lerped[index_xyz][1];
				v[i].z = s_lerped[index_xyz][2];
			}
			sceGumDrawArray(primitive,
				GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
				count, 0, v
			);
		} 
		else 
		{
			typedef struct 
			{
				float s, t;
				unsigned int c;
				float x, y, z;
			} VERT;

			VERT* v = (VERT*)sceGuGetMemory(sizeof(VERT) * count);
			int i;
			for(i = 0; i < count; i++) 
			{
				v[i].s = ((float*)order)[0];
				v[i].t = ((float*)order)[1];
				index_xyz = order[2];
				order += 3;

				l = shadedots[verts[index_xyz].lightnormalindex];

				v[i].c = GU_colorFloat(l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha);
				v[i].x = s_lerped[index_xyz][0];
				v[i].y = s_lerped[index_xyz][1];
				v[i].z = s_lerped[index_xyz][2];
			}
			sceGumDrawArray(primitive,
				GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
				count, 0, v
			);
		}
	}

	if(currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)) 
	{
		sceGuEnable(GU_TEXTURE_2D);
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (dmdl_t *paliashdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;
	daliasframe_t	*frame;

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
		int prim;
		if (count < 0)
		{
			count = -count;
			prim = GU_TRIANGLE_FAN;
		}
		else
			prim = GU_TRIANGLE_STRIP;

		// Allocate the vertices.
		struct vertex
		{ 
			float x, y, z;
		};

		vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * count));
		for (int vertex_index = 0; vertex_index < count; ++vertex_index)
		{
		  // normals and vertexes come from the frame list
/*
			point[0] = verts[order[2]].v[0] * frame->scale[0] + frame->translate[0];
			point[1] = verts[order[2]].v[1] * frame->scale[1] + frame->translate[1];
			point[2] = verts[order[2]].v[2] * frame->scale[2] + frame->translate[2];
*/

			memcpy( point, s_lerped[order[2]], sizeof( point )  );

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//		height -= 0.001;

			out[vertex_index].x = point[0];
			out[vertex_index].y = point[1];
			out[vertex_index].z = point[2];

			order += 3;
//		verts++;
		}
		sceGuDrawArray(prim, GU_VERTEX_32BITF, count, 0, out);
	}	
}

/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( vec3_t bbox[8], entity_t *e )
{
	int i;
	vec3_t		mins, maxs;
	dmdl_t		*paliashdr;
	vec3_t		vectors[3];
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	paliashdr = (dmdl_t *)currentmodel->extradata;

	if ( ( e->frame >= paliashdr->num_frames ) || ( e->frame < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n", 
			currentmodel->name, e->frame);
		e->frame = 0;
	}
	if ( ( e->oldframe >= paliashdr->num_frames ) || ( e->oldframe < 0 ) )
	{
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n", 
			currentmodel->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->frame * paliashdr->framesize);

	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + 
		                              paliashdr->ofs_frames +
									  e->oldframe * paliashdr->framesize);

	/*
	** compute axially aligned mins and maxs
	*/
	if ( pframe == poldframe )
	{
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i]*255;
		}
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i]*255;

			oldmins[i]  = poldframe->translate[i];
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255;

			if ( thismins[i] < oldmins[i] )
				mins[i] = thismins[i];
			else
				mins[i] = oldmins[i];

			if ( thismaxs[i] > oldmaxs[i] )
				maxs[i] = thismaxs[i];
			else
				maxs[i] = oldmaxs[i];
		}
	}

	/*
	** compute a full bounding box
	*/
	for ( i = 0; i < 8; i++ )
	{
		vec3_t   tmp;

		if ( i & 1 )
			tmp[0] = mins[0];
		else
			tmp[0] = maxs[0];

		if ( i & 2 )
			tmp[1] = mins[1];
		else
			tmp[1] = maxs[1];

		if ( i & 4 )
			tmp[2] = mins[2];
		else
			tmp[2] = maxs[2];

		VectorCopy( tmp, bbox[i] );
	}

	/*
	** rotate the bounding box
	*/
	VectorCopy( e->angles, angles );
	angles[YAW] = -angles[YAW];
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] );

	for ( i = 0; i < 8; i++ )
	{
		vec3_t tmp;

		VectorCopy( bbox[i], tmp );

		bbox[i][0] = DotProduct( vectors[0], tmp );
		bbox[i][1] = -DotProduct( vectors[1], tmp );
		bbox[i][2] = DotProduct( vectors[2], tmp );

		VectorAdd( e->origin, bbox[i], bbox[i] );
	}

	{
		int p, f, aggregatemask = ~0;

		for ( p = 0; p < 8; p++ )
		{
			int mask = 0;

			for ( f = 0; f < 4; f++ )
			{
				float dp = DotProduct( frustum[f].normal, bbox[p] );

				if ( ( dp - frustum[f].dist ) < 0 )
				{
					mask |= ( 1 << f );
				}
			}

			aggregatemask &= mask;
		}

		if ( aggregatemask )
		{
			return true;
		}

		return false;
	}
}

void R_LightPoint (vec3_t p, vec3_t color);
/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *e) {
	int			i;
	dmdl_t		*paliashdr;
	float		an;
	vec3_t		bbox[8];
	image_t		*skin;

	if ( !( e->flags & RF_WEAPONMODEL ) )
	{
		if ( R_CullAliasModel( bbox, e ) )
			return;
	}

	if(e->flags & RF_WEAPONMODEL) 
	{
		if(r_lefthand->value == 2)
		{
			return;
		}
	}

	paliashdr = (dmdl_t *)currentmodel->extradata;

//
	// get lighting information
	//
	// PMM - rewrote, reordered to handle new shells & mixing
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy
	//
	if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
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
/*
		// PMM -special case for godmode
		if ( (currententity->flags & RF_SHELL_RED) &&
			(currententity->flags & RF_SHELL_BLUE) &&
			(currententity->flags & RF_SHELL_GREEN) )
		{
			for (i=0 ; i<3 ; i++)
				shadelight[i] = 1.0;
		}
		else if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) )
		{
			VectorClear (shadelight);

			if ( currententity->flags & RF_SHELL_RED )
			{
				shadelight[0] = 1.0;
				if (currententity->flags & (RF_SHELL_BLUE|RF_SHELL_DOUBLE) )
					shadelight[2] = 1.0;
			}
			else if ( currententity->flags & RF_SHELL_BLUE )
			{
				if ( currententity->flags & RF_SHELL_DOUBLE )
				{
					shadelight[1] = 1.0;
					shadelight[2] = 1.0;
				}
				else
				{
					shadelight[2] = 1.0;
				}
			}
			else if ( currententity->flags & RF_SHELL_DOUBLE )
			{
				shadelight[0] = 0.9;
				shadelight[1] = 0.7;
			}
		}
		else if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN ) )
		{
			VectorClear (shadelight);
			// PMM - new colors
			if ( currententity->flags & RF_SHELL_HALF_DAM )
			{
				shadelight[0] = 0.56;
				shadelight[1] = 0.59;
				shadelight[2] = 0.45;
			}
			if ( currententity->flags & RF_SHELL_GREEN )
			{
				shadelight[1] = 1.0;
			}
		}
	}
			//PMM - ok, now flatten these down to range from 0 to 1.0.
	//		max_shell_val = max(shadelight[0], max(shadelight[1], shadelight[2]));
	//		if (max_shell_val > 0)
	//		{
	//			for (i=0; i<3; i++)
	//			{
	//				shadelight[i] = shadelight[i] / max_shell_val;
	//			}
	//		}
	// pmm
*/
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		for (i=0 ; i<3 ; i++)
			shadelight[i] = 1.0;
	}
	else
	{
		R_LightPoint (currententity->origin, shadelight);

		// player lighting hack for communication back to server
		// big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
					r_lightlevel->value = 150*shadelight[0];
				else
					r_lightlevel->value = 150*shadelight[2];
			}
			else
			{
				if (shadelight[1] > shadelight[2])
					r_lightlevel->value = 150*shadelight[1];
				else
					r_lightlevel->value = 150*shadelight[2];
			}

		}
		
		if ( gu_monolightmap->string[0] != '0' )
		{
			float s = shadelight[0];

			if ( s < shadelight[1] )
				s = shadelight[1];
			if ( s < shadelight[2] )
				s = shadelight[2];

			shadelight[0] = s;
			shadelight[1] = s;
			shadelight[2] = s;
		}

	}

	if ( currententity->flags & RF_MINLIGHT )
	{
		for (i=0 ; i<3 ; i++)
			if (shadelight[i] > 0.1)
				break;
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

		scale = 0.1 * sin(r_newrefdef.time*7);
		for (i=0 ; i<3 ; i++)
		{
			min = shadelight[i] * 0.8;
			shadelight[i] += scale;
			if (shadelight[i] < min)
				shadelight[i] = min;
		}
	}

// =================
// PGM	ir goggles color override
	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}
// PGM	
// =================

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	
	an = currententity->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//

	c_alias_polys += paliashdr->num_tris;

	if(currententity->flags & RF_DEPTHHACK) 
	{
	  sceGuDepthRange(0, 19660);
	}

	if((currententity->flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0f))
    {
		sceGumMatrixMode(GU_PROJECTION);
		sceGumPushMatrix();
		sceGumLoadIdentity();
		ScePspFVector3 scl = { -1.0f, 1.0f, 1.0f };
		sceGumScale(&scl);
		sceGumPerspective(r_newrefdef.fov_y, (float)r_newrefdef.width/r_newrefdef.height, 4.0f, 4096.0f);

		sceGumMatrixMode(GU_MODEL);

		sceGuFrontFace(GU_CCW);
	}

	sceGumPushMatrix();
	
	e->angles[PITCH] = -e->angles[PITCH]; // sigh.
	GU_RotateForEntity(e);
	e->angles[PITCH] = -e->angles[PITCH]; // sigh.

	if(currententity->skin) 
	{
		skin = currententity->skin;
	} 
	else 
	{
		if(currententity->skinnum >= MAX_MD2SKINS) 
		{
			skin = currentmodel->skins[0];
		} 
		else 
		{
			skin = currentmodel->skins[currententity->skinnum];
			if(!skin) 
			{
				skin = currentmodel->skins[0];
			}
		}
	}
	if(!skin) 
	{
		skin = r_notexture;
	}

	GL_Bind(skin->texnum);

	sceGuShadeModel(GU_SMOOTH);

	if(currententity->flags & RF_TRANSLUCENT) 
	{
		sceGuEnable(GU_BLEND);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
	}
    else
    {
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB);
	}

	if((currententity->frame >= paliashdr->num_frames) || (currententity->frame < 0)) 
	{
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n",
		currentmodel->name, currententity->frame);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

	if((currententity->oldframe >= paliashdr->num_frames) || (currententity->oldframe < 0)) 
	{
		ri.Con_Printf(PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n",
		currentmodel->name, currententity->oldframe);
		currententity->frame = 0;
		currententity->oldframe = 0;
	}

   	if ( !r_lerpmodels->value )
		currententity->backlerp = 0;
		
	GU_DrawAliasFrameLerp(paliashdr, currententity->backlerp);

    sceGumPopMatrix();
    sceGumUpdateMatrix();
  	
	if(currententity->flags & RF_TRANSLUCENT) 
	{
		sceGuDisable(GU_BLEND);
	}

	if((currententity->flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0f))
	{
		sceGuFrontFace(GU_CW);
		sceGumMatrixMode(GU_PROJECTION);
		sceGumPopMatrix();
		sceGumMatrixMode(GU_MODEL);
	}

	if(currententity->flags & RF_DEPTHHACK) 
	{
	    sceGuDepthRange(0, 65535);
	}

	sceGuShadeModel(GU_FLAT);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);

#if 0 //draw bbox
	sceGuDisable( GU_CULL_FACE );
	//glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	sceGuDisable( GU_TEXTURE_2D );

	glBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < 8; i++ )
	{
		glVertex3fv( bbox[i] );
	}
	glEnd();

	sceGuEnable( GU_TEXTURE_2D );
	//glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	sceGuEnable( GU_CULL_FACE );
#endif

#if 1
  if (gu_shadows->value && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)))
	{
		sceGumPushMatrix ();
		GU_RotateForEntity (e);
		sceGumUpdateMatrix ();
		sceGuDisable (GU_TEXTURE_2D);
		sceGuEnable (GU_BLEND);
		sceGuColor (GU_COLOR(0,0,0,0.5));
		GL_DrawAliasShadow (paliashdr, currententity->frame );
		sceGuEnable (GU_TEXTURE_2D);
		sceGuDisable (GU_BLEND);
		sceGumPopMatrix ();
	  
	}
#endif

	sceGuColor(0xffffffff);
}
