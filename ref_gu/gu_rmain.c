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
// gu_main.c
#include "gu_local.h"

void R_Clear (void);

viddef_t	vid;

refimport_t	ri;

model_t		*r_worldmodel;

int		gldepthmin, gldepthmax;

glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles
#ifdef KINGPIN_ENGINE
image_t		*r_reflectiontexture;
image_t		*r_speculartexture;
image_t		*r_firescreentexture;

image_t		*r_worldreflectiontexture = NULL;
#endif

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			// final blending color

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

ScePspFMatrix4	r_world_matrix;
//ScePspFMatrix4	r_base_world_matrix;

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

cvar_t	*gl_particle_scale;
cvar_t	*gl_particle_point;

cvar_t	*gl_log;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
cvar_t  *gl_monolightmap;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_skytga;
cvar_t	*gl_showtris;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
cvar_t  *gl_saturatelighting;
cvar_t	*gl_lockpvs;

#ifdef KINGPIN_ENGINE
cvar_t	*r_detail_alpha;
cvar_t	*r_detail_scale;
cvar_t	*r_detail_dist;
cvar_t	*r_directional_lighting;
cvar_t	*r_showbbox;
cvar_t	*gl_fog;
cvar_t	*gl_reflections;
cvar_t	*gl_specular;
cvar_t	*gl_test_specular;
cvar_t	*r_sfx_pause;
cvar_t	*r_debug_lighting;
cvar_t	*r_painskins;
cvar_t	*gl_maxtexsize;
#endif

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_ref;

#ifdef KINGPIN_ENGINE
cvar_t	*burn_enabled;
cvar_t	*burn_r;
cvar_t	*burn_g;
cvar_t	*burn_b;

static void R_BurnPickColor_f (void)
{
	vec3_t end;
	int result;

	if (!r_worldmodel)
	{
		ri.Con_Printf (PRINT_ALL, "Couldn't find a color\n");
		return;
	}
	VectorMA (r_origin, 8000.0f, vpn, end);
	result = RecursiveLightPoint (r_worldmodel->nodes, r_origin, end);
	if (result == -1)
	{
		ri.Con_Printf (PRINT_ALL, "Couldn't find a color\n");
		return;
	}
	if (pointcolor[0] < 0) pointcolor[0] = 0;
	if (pointcolor[0] > 1) pointcolor[0] = 1;
	if (pointcolor[1] < 0) pointcolor[1] = 0;
	if (pointcolor[1] > 1) pointcolor[1] = 1;
	if (pointcolor[2] < 0) pointcolor[2] = 0;
	if (pointcolor[2] > 1) pointcolor[2] = 1;
	ri.Cvar_SetValue ("burn_r", pointcolor[0]);
	ri.Cvar_SetValue ("burn_g", pointcolor[1]);
	ri.Cvar_SetValue ("burn_b", pointcolor[2]);
}

#endif

/*
=================
R_RotateForEntity
=================
*/
void R_RotateForEntity (entity_t *e)
{
	ScePspFVector3	fvec;

	fvec.x = e->origin[0];
	fvec.y = e->origin[1];
	fvec.z = e->origin[2];

	sceGumTranslate (&fvec);

	fvec.z = e->angles[1] * (GU_PI / 180.0f);
	fvec.y = -e->angles[0] * (GU_PI / 180.0f);
	fvec.x = -e->angles[2] * (GU_PI / 180.0f);

	sceGumRotateZYX (&fvec);

	if (e->scale != 0.0f)
	{
		fvec.x = e->scale + 1.0f;
		fvec.y = e->scale + 1.0f;
		fvec.z = e->scale + 1.0f;

		sceGumScale (&fvec);
	}

	sceGumUpdateMatrix ();
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float		alpha = 1.0F;
	vec3_t		point;
	dsprframe_t	*frame;
	float		*up, *right;
	int			frame_radius, frame_up, frame_right;
	dsprite_t	*psprite;
	vec3_t		mins, maxs;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->cache.data;

#if 0
	if (e->frame < 0 || e->frame >= psprite->numframes)
	{
		ri.Con_Printf (PRINT_ALL, "no such sprite frame %i\n", e->frame);
		e->frame = 0;
	}
#endif
	e->frame %= psprite->numframes;

	frame = &psprite->frames[e->frame];

	frame_up    = frame->height - frame->origin_y;
	frame_right = frame->width - frame->origin_x;
	frame_radius = Q_max(Q_max(frame->origin_y, frame_up), Q_max(frame->origin_x, frame_right));

	VectorSet(mins, e->origin[0] - frame_radius, e->origin[1] - frame_radius, e->origin[2] - frame_radius);
	VectorSet(maxs, e->origin[0] + frame_radius, e->origin[1] + frame_radius, e->origin[2] + frame_radius);

	if (GU_ClipCullBox(mins, maxs) == GU_CULL_OUTSIDE)
		return;
#if 0
	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
	vec3_t		v_forward, v_right, v_up;

	AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
#endif
#ifdef KINGPIN_ENGINE
	vec3_t		v_forward, v_right, v_up;
	if (e->flags & RF_FACING)
	{
		AngleVectors (e->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else if (e->flags & RF_FACINGUP)
	{
		AngleVectors (e->angles, v_forward, v_right, v_up);
		up = v_up;
		right = vright;
	}
	else
#endif
	{	// normal sprite
		up = vup;
		right = vright;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0F )
		sceGuEnable (GU_BLEND);
	else
		sceGuEnable (GU_ALPHA_TEST);

#ifdef KINGPIN_ENGINE
	if (e->flags & RF_FACING)
		sceGuDisable (GU_CULL_FACE);

#if 0
	/* Retain the original DLL's ordering, including the calls inside Begin/End. */
	qglPushMatrix ();
	qglTranslatef (e->origin[0], e->origin[1], e->origin[2]);
	qglScalef (0.5F, 0.5F, 0.5F);
	qglTranslatef (-e->origin[0], -e->origin[1], -e->origin[2]);
	qglTranslatef (0, 0, -frame->origin_y * 0.5F);
#endif
#endif

	sceGuColor (GU_HCOLOR_4F(1, 1, 1, alpha));

    GL_Bind (currentmodel->skins[e->frame]);

	GL_TexEnv (GU_TFX_MODULATE);

	gu_vert_ftv_t* const out = (gu_vert_ftv_t*)sceGuGetMemory (sizeof(gu_vert_ftv_t) * 4);
	out[0].uv[0] = 0.0f;
	out[0].uv[1] = 1.0f;
	VectorMA (e->origin, -frame->origin_y, up, out[0].xyz);
	VectorMA (out[0].xyz, -frame->origin_x, right, out[0].xyz);
	out[1].uv[0] = 0.0f;
	out[1].uv[1] = 0.0f;
	VectorMA (e->origin, frame_up, up, out[1].xyz);
	VectorMA (out[1].xyz, -frame->origin_x, right, out[1].xyz);
	out[2].uv[0] = 1.0f;
	out[2].uv[1] = 0.0f;
	VectorMA (e->origin, frame_up, up, out[2].xyz);
	VectorMA (out[2].xyz, frame_right, right, out[2].xyz);
	out[3].uv[0] = 1.0f;
	out[3].uv[1] = 1.0f;
	VectorMA (e->origin, -frame->origin_y, up, out[3].xyz);
	VectorMA (out[3].xyz, frame_right, right, out[3].xyz);
	sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 4, 0, out);

	GL_TexEnv (GU_TFX_REPLACE);

#ifdef KINGPIN_ENGINE
	if (e->flags & RF_FACING)
		sceGuEnable (GU_CULL_FACE);
#endif

	if ( alpha != 1.0F )
		sceGuDisable (GU_BLEND);
	else
		sceGuDisable (GU_ALPHA_TEST);

	sceGuColor (GU_HCOLOR_DEFAULT);

	DEBUG_DrawSpriteAABB (mins, maxs);
}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
#define NULL_MODEL_SIZE	16
void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;
	float	radians;
	uint	vertcount;

	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
#ifdef KINGPIN_ENGINE
	else if ( currententity->flags & RF_DIMLIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 0.6F;
#endif
	else
		R_LightPoint (currententity->origin, shadelight);

	sceGumPushMatrix ();
	R_RotateForEntity (currententity);

	sceGuDisable (GU_TEXTURE_2D);
	sceGuColor (GU_HCOLOR_3FV(shadelight));

	gu_vert_fv_t* out = (gu_vert_fv_t*)sceGuGetMemory (sizeof(gu_vert_fv_t) * 12);

	vertcount = 0;

	out[vertcount].x = 0;
	out[vertcount].y = 0;
	out[vertcount].z = -NULL_MODEL_SIZE;
	vertcount++;

	for (i = 0; i <= 4; i++)
	{
		radians = i * M_PI / 2.0f;
		out[vertcount].x = NULL_MODEL_SIZE * cos (radians);
		out[vertcount].y = NULL_MODEL_SIZE * sin (radians);
		out[vertcount].z = 0;

		vertcount++;
	}
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, vertcount, 0, out);

	out += vertcount;
	vertcount = 0;

	out[vertcount].x = 0;
	out[vertcount].y = 0;
	out[vertcount].z = NULL_MODEL_SIZE;
	vertcount++;

	for (i = 4; i >= 0; i--)
	{
		radians = i * M_PI / 2.0f;
		out[vertcount].x = NULL_MODEL_SIZE * cos (radians);
		out[vertcount].y = NULL_MODEL_SIZE * sin (radians);
		out[vertcount].z = 0;
		vertcount++;
	}
	sceGuDrawArray(GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, vertcount, 0, out);

	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGumPopMatrix ();
	sceGumUpdateMatrix ();
	sceGuEnable (GU_TEXTURE_2D);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value)
		return;
	
#ifdef KINGPIN_ENGINE
	R_UpdateMdxSFXIntervalMask ();
#endif

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
#ifdef KINGPIN_ENGINE
		currententity->rendered = false;
		/* Retail promotes models whose primary skin has an alpha channel to
		 * the reflection-map path before classifying the entity pass. */
		if (currententity->model && currententity->model->skins[0] &&
			currententity->model->skins[0]->flags & IMG_FLAG_HAS_ALPHA)
			currententity->flags |= RF_REFL_MAP;

		if ((currententity->flags2 & (RF2_SURF_ALPHA | RF2_PASSALPHA)) ||
			((currententity->flags & RF_TRANSLUCENT) &&
			 !(currententity->flags & RF_REFL_MAP)))
			continue;
		currententity->rendered = true;
#else
		if (currententity->flags & RF_TRANSLUCENT)
			continue;	// solid
#endif

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
#ifdef KINGPIN_ENGINE
			case mod_mdx:
				if (currententity->flags & RF_REFL_MAP)
					R_DrawMDXModel (currententity, 1);

				R_DrawMDXModel (currententity, 0);
				break;
#endif
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				/* Kingpin sprites are handled only by the late entity pass.  Shipped
				 * sprite entities carry one of the late-pass flags above. */
#ifndef KINGPIN_ENGINE
				R_DrawSpriteModel (currententity);
#endif
				break;
			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	sceGuDepthMask (GU_TRUE);		// no z writes
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
#ifdef KINGPIN_ENGINE
		if (currententity->rendered)
			continue;
		currententity->rendered = true;
#else
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;	// solid
#endif

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
#ifdef KINGPIN_ENGINE
			case mod_mdx:
				R_DrawMDXModel (currententity, 0);
				break;
#endif
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype");
				break;
			}
		}
	}
	sceGuDepthMask (GU_FALSE);		// back to writing

}


#ifdef KINGPIN_ENGINE

/*
===============
GL_DrawParticles
===============
*/
void GL_DrawParticles( int num_particles, const particle_t *particles, qboolean secondary )
{
	const particle_t	*p;
	int					i;
	vec3_t				up, right;
	float				scale;
	float				alpha;
	float				blend;
	vec3_t				from, middle, to;
	gu_color_32_t		color;

    GL_Bind (r_particletexture);

	sceGuDepthMask (GU_TRUE);		// no z buffering
	sceGuEnable (GU_BLEND);
	GL_TexEnv (GU_TFX_MODULATE);

	gu_vert_ftcv_t* const out = (gu_vert_ftcv_t *)extGuBeginMemory (NULL); //(gu_vert_ftcv_t*)sceGuGetMemory (sizeof(gu_vert_ftcv_t) * num_particles * 2);
	gu_vert_ftcv_t* out_ptr = out;
	for ( p = particles, i = 0; i < num_particles; i++, p++)
	{
		if (secondary ? (p->type != 0) : (p->type > 0 && p->type < 50))
			continue;

		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = gl_particle_scale->value;
		else
			scale = gl_particle_scale->value + scale * 0.004f;

		alpha = Q_bound (0.0f, p->alpha, 1.0f);

		color.c = secondary ? 0xff7f7f7f : d_8to24table[p->color];
		color.a = (byte)(alpha * 255.0f);

		if (p->type >= 50 && p->type <= 52)
		{
			if (p->type == 50)
			{
				VectorSet (from, 255, 135, 0);
				VectorSet (middle, 255, 255, 144);
				VectorSet (to, 255, 255, 255);
				if (alpha < 0.25f)
				{
					blend = alpha * 4.0f;
					color.r = (byte)(from[0] + (middle[0] - from[0]) * blend);
					color.g = (byte)(from[1] + (middle[1] - from[1]) * blend);
					color.b = (byte)(from[2] + (middle[2] - from[2]) * blend);
				}
				else
				{
					blend = (alpha - 0.25f) * (1.0f / 0.75f);
					color.r = (byte)(middle[0] + (to[0] - middle[0]) * blend);
					color.g = (byte)(middle[1] + (to[1] - middle[1]) * blend);
					color.b = (byte)(middle[2] + (to[2] - middle[2]) * blend);
				}
			}
			else if (p->type == 51)
			{
				VectorSet (from, 0, 0, 128);
				VectorSet (middle, 180, 180, 255);
				VectorSet (to, 255, 255, 255);
				if (alpha < 0.5f)
				{
					blend = alpha * 2.0f;
					color.r = (byte)(from[0] + (middle[0] - from[0]) * blend);
					color.g = (byte)(from[1] + (middle[1] - from[1]) * blend);
					color.b = (byte)(from[2] + (middle[2] - from[2]) * blend);
					scale *= 2.25f - alpha * 3.0f;
				}
				else
				{
					blend = (alpha - 0.5f) * 2.0f;
					color.r = (byte)(middle[0] + (to[0] - middle[0]) * blend);
					color.g = (byte)(middle[1] + (to[1] - middle[1]) * blend);
					color.b = (byte)(middle[2] + (to[2] - middle[2]) * blend);
					scale *= 0.75f;
				}
			}
			else
			{
				VectorSet (from, 80, 11, 0);
				VectorSet (middle, 215, 173, 63);
				VectorSet (to, 244, 244, 244);
				if (alpha < 0.5f)
				{
					blend = alpha * 2.0f;
					color.r = (byte)(from[0] + (middle[0] - from[0]) * blend);
					color.g = (byte)(from[1] + (middle[1] - from[1]) * blend);
					color.b = (byte)(from[2] + (middle[2] - from[2]) * blend);
				}
				else
				{
					blend = (alpha - 0.5f) * 2.0f;
					color.r = (byte)(middle[0] + (to[0] - middle[0]) * blend);
					color.g = (byte)(middle[1] + (to[1] - middle[1]) * blend);
					color.b = (byte)(middle[2] + (to[2] - middle[2]) * blend);
				}
				scale *= 2.0f;
			}
		}

		VectorScale (vup, scale, up);
		VectorScale (vright, scale, right);

		out_ptr->u = 0.0f;
		out_ptr->v = 0.0f;
		out_ptr->c = color.c;
		out_ptr->a = p->alpha * 255; // replace alpha
		out_ptr->x = p->origin[0] + right[0] + up[0];
		out_ptr->y = p->origin[1] + right[1] + up[1];
		out_ptr->z = p->origin[2] + right[2] + up[2];
		out_ptr++;

		out_ptr->u = 1.0f;
		out_ptr->v = 1.0f;
		out_ptr->c = color.c;
		out_ptr->x = p->origin[0] - right[0] - up[0];
		out_ptr->y = p->origin[1] - right[1] - up[1];
		out_ptr->z = p->origin[2] - right[2] - up[2];
		out_ptr++;
	}
	extGuEndMemory (out_ptr);
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF, (int)(out_ptr - out), 0, out);

	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuDepthMask (GU_FALSE);		// back to normal Z buffering
	GL_TexEnv (GU_TFX_REPLACE);
}

/*
===============
GL_DrawSpriteParticles
===============
*/
static void GL_DrawSpriteParticles ( int num_particles, const particle_t *particles, qboolean secondary )
{
	const particle_t	*p;
	image_t				*image;
	int					i;
	vec3_t				right, up, vertex;
	gu_color_32_t		color;
	float				size, alpha;

	for ( p = particles, i = 0; i < num_particles; i++, p++)
	{
		//if (p->type < 0 || p->type >= MAX_SFX_SPRITES)
		//	return;
	
		if (p->type == 0 || (!secondary && p->type >= 50))
			continue;	

		image = r_sfx_images[p->type];
		if (!image)
			image = r_notexture;

		if (p->type == SFX_SPRITE_SURF_BLOOD_POOL)
		{
			float elapsed = R_SFXTime () - p->frame * 0.001f;
			size = elapsed < 4.0f ? elapsed * 6.0f : 24.0f;
			if (size < 0.0f)
				size = 0.0f;
			VectorSet (right, size, 0, 0);
			VectorSet (up, 0, size, 0);

			color.c = d_8to24table[p->color];
		}
		else if (secondary && p->type == SFX_SPRITE_RAIN)
		{
			if (currententity)
				AngleVectors (currententity->angles, NULL, right, up);
			else
			{
				VectorCopy (vright, right);
				VectorCopy (vup, up);
			}

			color.c = GU_HCOLOR_DEFAULT;
		}
		else if (secondary && p->type >= SFX_SPRITE_SPLASH1 && p->type <= SFX_SPRITE_SPLASH4)
		{
			if (currententity)
				AngleVectors (currententity->angles, NULL, right, up);
			else
			{
				VectorCopy (vright, right);
				VectorCopy (vup, up);
			}
			VectorScale (right, 6.0f, right);
			VectorScale (up, 6.0f, up);

			color.c = GU_HCOLOR_DEFAULT;
		}
		else
		{
			size = (p->type == SFX_SPRITE_BLOOD_DRIP) ? 0.5f : 8.0f;
			VectorScale (vright, size, right);
			VectorScale (vup, size, up);

			color.c = d_8to24table[p->color];
		}

		alpha = Q_bound(0.0f, p->alpha, 1.0f);
		color.a = (byte)(alpha * 255.0f);

		GL_Bind (image);

		sceGuColor (color.c);

		gu_vert_ftv_t* const out = (gu_vert_ftv_t*)sceGuGetMemory (sizeof(gu_vert_ftv_t) * 4);
		out[0].uv[0] = 0.0f;
		out[0].uv[1] = 1.0f;
		VectorSubtract (p->origin, right, out[0].xyz);
		VectorAdd (out[0].xyz, up, out[0].xyz);
		out[1].uv[0] = 0.0f;
		out[1].uv[1] = 0.0f;
		VectorAdd (p->origin, right, out[1].xyz);
		VectorAdd (out[1].xyz, up, out[1].xyz);
		out[2].uv[0] = 1.0f;
		out[2].uv[1] = 0.0f;
		VectorAdd (p->origin, right, out[2].xyz);
		VectorSubtract (out[2].xyz, up, out[2].xyz);
		out[3].uv[0] = 1.0f;
		out[3].uv[1] = 1.0f;
		VectorSubtract (p->origin, right, out[3].xyz);
		VectorSubtract (out[3].xyz, up, out[3].xyz);

		sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 4, 0, out);
	}

	sceGuColor (GU_HCOLOR_DEFAULT);
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	sceGuDisable (GU_ALPHA_TEST);

	GL_DrawParticles (r_newrefdef.num_particles, r_newrefdef.particles, false);
	GL_DrawParticles (r_newrefdef.num_particles2, r_newrefdef.particles2, true);

	sceGuDepthMask (GU_TRUE);
	sceGuEnable (GU_BLEND);
	GL_TexEnv (GU_TFX_MODULATE);

	GL_DrawSpriteParticles(r_newrefdef.num_particles, r_newrefdef.particles, false);
	GL_DrawSpriteParticles(r_newrefdef.num_particles2, r_newrefdef.particles2, true);
	
	GL_TexEnv (GU_TFX_REPLACE);
	sceGuDisable (GU_BLEND);
	sceGuDepthMask (GU_FALSE);

	sceGuEnable (GU_ALPHA_TEST);
}
#else

/*
** GL_DrawParticles
**
*/
void GL_DrawParticles( int num_particles, const particle_t particles[] )
{
	const particle_t *p;
	int				i;
	vec3_t			up, right;
	float			scale;
	//uint			color;

    GL_Bind (r_particletexture);
	sceGuDepthMask (GU_TRUE);		// no z buffering
	sceGuEnable (GU_BLEND);
	GL_TexEnv (GU_TFX_MODULATE);

	//VectorScale (vup, 1.5, up);
	//VectorScale (vright, 1.5, right);

	gu_vert_ftcv_t* const out = (gu_vert_ftcv_t*)sceGuGetMemory (sizeof(gu_vert_ftcv_t) * num_particles * 2);

	for ( p = particles, i = 0; i < num_particles; i++, p++)
	{
#if 1
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = gl_particle_scale->value;
		else
			scale = gl_particle_scale->value + scale * 0.004f;

		VectorScale (vup, scale, up);
		VectorScale (vright, scale, right);

		//color = d_8to24table[p->color];
		//(byte *)color[3] = p->alpha*255;

		out[i * 2 + 0].u = 0.0f;
		out[i * 2 + 0].v = 0.0f;
		out[i * 2 + 0].c = d_8to24table[p->color];
		out[i * 2 + 0].a = p->alpha * 255; // replace alpha
		out[i * 2 + 0].x = p->origin[0] + right[0] + up[0];
		out[i * 2 + 0].y = p->origin[1] + right[1] + up[1];
		out[i * 2 + 0].z = p->origin[2] + right[2] + up[2];

		out[i * 2 + 1].u = 1.0f;
		out[i * 2 + 1].v = 1.0f;
		out[i * 2 + 1].c = out[i * 2 + 0].c;
		out[i * 2 + 1].x = p->origin[0] - right[0] - up[0];
		out[i * 2 + 1].y = p->origin[1] - right[1] - up[1];
		out[i * 2 + 1].z = p->origin[2] - right[2] - up[2];
#else
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		*(int *)color = colortable[p->color];
		color[3] = p->alpha*255;

		qglColor4ubv( color );

		qglTexCoord2f( 0.0625, 0.0625 );
		qglVertex3fv( p->origin );

		qglTexCoord2f( 1.0625, 0.0625 );
		qglVertex3f( p->origin[0] + up[0]*scale,
			         p->origin[1] + up[1]*scale,
					 p->origin[2] + up[2]*scale);

		qglTexCoord2f( 0.0625, 1.0625 );
		qglVertex3f( p->origin[0] + right[0]*scale,
			         p->origin[1] + right[1]*scale,
					 p->origin[2] + right[2]*scale);
#endif
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
	{
		int		i;
		//uint	color;
		const particle_t *p;

		sceGuDepthMask (GU_TRUE);
		sceGuEnable (GU_BLEND);
		sceGuDisable (GU_TEXTURE_2D);

		//qglPointSize( gl_particle_size->value );
		gu_vert_fcv_t* const out = (gu_vert_fcv_t*)sceGuGetMemory (sizeof(gu_vert_fcv_t) * r_newrefdef.num_particles);

		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			//color = d_8to24table[p->color];
			//(byte *)color[3] = p->alpha * 255;

			out[i].c = d_8to24table[p->color];
			out[i].a = p->alpha * 255;

			out[i].x = p->origin[0];
			out[i].y = p->origin[1];
			out[i].z = p->origin[2];
		}
		sceGuDrawArray (GU_POINTS, GU_COLOR_8888 | GU_VERTEX_32BITF, r_newrefdef.num_particles, 0, out);

		sceGuDisable (GU_BLEND);
		sceGuColor (GU_HCOLOR_DEFAULT);
		sceGuDepthMask (GU_FALSE);
		sceGuEnable (GU_TEXTURE_2D);

	}
	else
	{
		GL_DrawParticles (r_newrefdef.num_particles, r_newrefdef.particles);
	}
}
#endif

#ifdef KINGPIN_ENGINE
/*
============
R_DrawFirescreen
============
*/
static void R_DrawFirescreen (void)
{
	int	alpha;
	short	s, t;

	alpha = (int)(v_blend[3] * 1275.0f);
	if(alpha > 255)
		alpha = 255;

	sceGuEnable (GU_BLEND);
	sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, GU_HBLEND_ONE);

	sceGuDisable (GU_ALPHA_TEST);
	sceGuDisable (GU_DEPTH_TEST);

	GL_TexEnv (GU_TFX_MODULATE);
	sceGuColor (GU_HCOLOR_4UB(255, 255, 255, alpha));

	GL_Bind (r_firescreentexture);

	s = (rand () & 0x7fff) * (r_firescreentexture->sh * 0.2f / 32767.0f);
	t = (rand () & 0x7fff) * (r_firescreentexture->th * 0.2f / 32767.0f);

	gu_vert_htv_t* const out = (gu_vert_htv_t*)sceGuGetMemory(sizeof(gu_vert_htv_t) * 2);
	out[0].u = s;
	out[0].v = t;
	out[0].x = 0;
	out[0].y = 0;
	out[0].z = 0;
	out[1].u = s + r_firescreentexture->sh * 0.8f;
	out[1].v = t + r_firescreentexture->th * 0.8f;
	out[1].x = vid.width;
	out[1].y = vid.height;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);

	GL_TexEnv (GU_TFX_REPLACE);
	sceGuDisable (GU_BLEND);
	sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuColor (GU_HCOLOR_DEFAULT);

	sceGuEnable (GU_ALPHA_TEST);
}
#endif

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend->value)
		return;
	if (!v_blend[3])
		return;

#ifdef KINGPIN_ENGINE
	if(r_firescreentexture && v_blend[0] > 0.8f &&
		v_blend[1] > 0.5f && v_blend[1] < v_blend[0] && v_blend[2] < 0.2f)
	{
		R_DrawFirescreen();
		return;
	}
#endif

	sceGuEnable (GU_BLEND);
	sceGuDisable (GU_DEPTH_TEST);
	sceGuDisable (GU_TEXTURE_2D);

	sceGuColor (GU_HCOLOR_4FV(v_blend));

	gu_vert_hv_t* const out = (gu_vert_hv_t*)sceGuGetMemory(sizeof(gu_vert_hv_t) * 2);
	out[0].x = 0;
	out[0].y = 0;
	out[0].z = 0;
	out[1].x = vid.width;
	out[1].y = vid.height;
	out[1].z = 0;
	sceGuDrawArray (GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out); // through mode

	sceGuDisable (GU_BLEND);
	sceGuEnable (GU_TEXTURE_2D);

	sceGuColor (GU_HCOLOR_DEFAULT);
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int i;
	mleaf_t	*leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);

	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

// current viewcluster
#ifdef KINGPIN_ENGINE
	if ( !( r_newrefdef.rdflags & ( RDF_NOWORLDMODEL | RDF_NOWORLDMODELALPHA ) ) )
#else
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
#endif
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (i=0 ; i<4 ; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;
#ifdef KINGPIN_ENGINE
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODELALPHA )
	{
		/* Inventory/menu model views retain the existing colour buffer and
		 * clear only depth inside their viewport (FUN_1000ff80). */
		sceGuEnable( GU_SCISSOR_TEST );
		sceGuScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y,
			r_newrefdef.width, r_newrefdef.height );
		sceGuClear( GU_DEPTH_BUFFER_BIT | GU_FAST_CLEAR_BIT );
		sceGuDisable( GU_SCISSOR_TEST );
	}
#endif
#if 0 // not used, sceGuClear has a bug
	// clear out the portion of the screen that the NOWORLDMODEL defines
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
	{
		//sceGuEnable (GU_SCISSOR_TEST);
		sceGuClearColor (GU_HCOLOR_4F(0.3, 0.3, 0.3, 1.0));
		sceGuScissor (r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		sceGuClear (GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
		sceGuClearColor (GU_HCOLOR_4F(1.0, 0.0, 0.5, 0.5));
		//sceGuDisable (GU_SCISSOR_TEST);
	}
#endif
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
//	float	yfov;
	int		x, x2, y2, y, w, h;
	ScePspFVector3	translate;


	//
	// set up viewport
	//
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	sceGuViewport ( 2048 /*- ( vid.width >> 1 ) + x + ( w >> 1 )*/,
					2048 + (vid.height >> 1) - y2 - (h >> 1),
					w, h);
	sceGuScissor (x, vid.height - y2 - h, x + w, vid.height - y2);

	//
	// set up projection matrix
	//
    screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
//	yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI;
	sceGumMatrixMode (GU_PROJECTION);
	sceGumLoadIdentity ();

    sceGumPerspective (r_newrefdef.fov_y,  screenaspect,  4,  4096);

	sceGuFrontFace (GU_CW); // GL_FRONT

	sceGumMatrixMode (GU_VIEW);
    sceGumLoadIdentity ();

	sceGumRotateX (-90 * (GU_PI / 180.0f));
	sceGumRotateZ ( 90 * (GU_PI / 180.0f));
	sceGumRotateX (-r_newrefdef.viewangles[2] * (GU_PI / 180.0f));
	sceGumRotateY (-r_newrefdef.viewangles[0] * (GU_PI / 180.0f));
	sceGumRotateZ (-r_newrefdef.viewangles[1] * (GU_PI / 180.0f));

	translate.x = -r_newrefdef.vieworg[0];
	translate.y = -r_newrefdef.vieworg[1];
	translate.z = -r_newrefdef.vieworg[2];

	sceGumTranslate (&translate);

//	if ( gl_state.camera_separation != 0 && gl_state.stereo_enabled )
//		qglTranslatef ( gl_state.camera_separation, 0, 0 );

	sceGumMatrixMode (GU_MODEL);
	sceGumLoadIdentity ();

	sceGumUpdateMatrix ();	// apply a matrix transformation

	//sceGumStoreMatrix(&r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull->value)
		sceGuEnable (GU_CULL_FACE);
	else
		sceGuDisable (GU_CULL_FACE);

	sceGuDisable (GU_BLEND);
	sceGuDisable (GU_ALPHA_TEST);
	sceGuEnable (GU_DEPTH_TEST);
}

#ifdef KINGPIN_ENGINE
/*
====================
R_SetFog

PSP GE has linear fog only. Choose the endpoint such that the
linear fog amount at the renderer's 4096-unit far clip matches GL_EXP.
====================
*/
void R_SetFog (float density, vec3_t color, float density2, vec3_t color2)
{
	if (density <= 0.0f)
	{
		gl_state.fog.enabled = false;
		gl_state.fog.modified = true;
		return;
	}

	gl_state.fog.near = 0.0f;
	gl_state.fog.far =
		4096.0f / (1.0f - expf(-density * 4096.0f));
	gl_state.fog.color = GU_HCOLOR_4FV(color);
	gl_state.fog.enabled = true;
	gl_state.fog.modified = true;

	Com_DPrintf ("R_SetFog: %f %f %#010X\n", 
		gl_state.fog.near, 
		gl_state.fog.far, 
		gl_state.fog.color);
}

/*
====================
R_SetupFog
====================
*/
/* Retail FUN_1000cce0, called once per 3D frame before R_DrawWorld. */
void R_SetupFog (void)
{
	if (!gl_fog->value || !gl_state.fog.enabled)
	{
		if (gl_fog->modified || gl_state.fog.modified)
			sceGuDisable (GU_FOG);

		gl_fog->modified = false;
		gl_state.fog.modified = false;
        return;
    }

	if (!gl_fog->modified && !gl_state.fog.modified)
		return;

	sceGuEnable (GU_FOG);
	sceGuFog (gl_state.fog.near, gl_state.fog.far, gl_state.fog.color);
	sceGuClearColor (gl_state.fog.color);

	gl_fog->modified = false;
	gl_state.fog.modified = false;
}
#endif

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	int	cmask;

	cmask = GU_DEPTH_BUFFER_BIT | GU_FAST_CLEAR_BIT;

	if (gl_clear->value)
		cmask |= GU_COLOR_BUFFER_BIT;

	sceGuClear (cmask);

	gldepthmin = 0;
	gldepthmax = 65535;

	sceGuDepthFunc (GU_LEQUAL);
	sceGuDepthRange (gldepthmin, gldepthmax);
}

#ifdef KINGPIN_ENGINE
/*
=============
R_ClearDepth
=============
*/
void R_ClearDepth (void)
{
	sceGuClear (GU_DEPTH_BUFFER_BIT);
}
#endif

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Sys_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

#ifdef KINGPIN_ENGINE
	R_UpdateSFXTime ();
#endif

	R_PushDlights ();

#ifdef KINGPIN_ENGINE
	R_PushSurfaceLights ();
#endif

#if 0
	if (gl_finish->value)
		qglFinish ();
#endif

	R_SetupFrame ();

	R_SetupGL ();

	GU_ClipSetWorldFrustum ();

	R_MarkLeaves ();	// done here so we know if we're in water

#ifdef KINGPIN_ENGINE
	R_SetupFog ();
#endif

	R_DrawWorld ();

#ifdef KINGPIN_ENGINE
	R_DrawReflectionsSurfaces ();
#endif

	R_DrawEntitiesOnList ();

	R_RenderDlights ();

#ifdef KINGPIN_ENGINE
	/* Retail draws queued alpha geometry first, then SFX and both particle
	 * lists, followed by the deferred weapon-model effect. */
	R_DrawAlphaSurfaces ();
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODELALPHA))
	{
		R_DrawSFX ();
		R_DrawParticles ();
		R_DrawDeferredSFX ();
	}
#else
	R_DrawParticles ();
	R_DrawAlphaSurfaces ();
#endif

	R_PolyBlend(); // flash

	if (r_speeds->value)
	{
#ifdef KINGPIN_ENGINE
		static int fps_samples;
		static float fps_total;
		static float fps_average;

		if (r_detail_alpha->value > 0.0f)
		{
			if (fps_samples < 15)
			{
				fps_total += R_SFXFramerate ();
				fps_samples++;
			}
			else
			{
				fps_average = fps_total / fps_samples;
				fps_samples = 0;
				fps_total = 0.0f;
			}
			ri.Con_Printf (PRINT_ALL,
				"%4i wpoly %4i epoly %i tex %i lmaps %i fps\n",
				c_brush_polys, c_alias_polys, c_visible_textures,
				c_visible_lightmaps, Q_ftol (fps_average));
		}
#else
		ri.Con_Printf (PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys,
			c_alias_polys,
			c_visible_textures,
			c_visible_lightmaps);
#endif
	}
}


void	R_SetGL2D (void)
{
	// set 2D virtual screen size
#if 1
	sceGuViewport (2048, 2048, vid.width, vid.height);
	sceGuScissor (0, 0, vid.width, vid.height);
	sceGuDisable (GU_DEPTH_TEST);
	sceGuDisable (GU_CULL_FACE);
	sceGuDisable (GU_BLEND);
	sceGuEnable (GU_ALPHA_TEST);
	sceGuColor (GU_HCOLOR_DEFAULT);
#else
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);
	qglColor4f (1,1,1,1);
#endif
}

#if 0
static void GL_DrawColoredStereoLinePair( float r, float g, float b, float y )
{
	qglColor3f( r, g, b );
	qglVertex2f( 0, y );
	qglVertex2f( vid.width, y );
	qglColor3f( 0, 0, 0 );
	qglVertex2f( 0, y + 1 );
	qglVertex2f( vid.width, y + 1 );
}

static void GL_DrawStereoPattern( void )
{
	int i;

	if ( !( gl_config.renderer & GL_RENDERER_INTERGRAPH ) )
		return;

	if ( !gl_state.stereo_enabled )
		return;

	R_SetGL2D();

	qglDrawBuffer( GL_BACK_LEFT );

	for ( i = 0; i < 20; i++ )
	{
		qglBegin( GL_LINES );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 0 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 2 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 4 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 6 );
			GL_DrawColoredStereoLinePair( 0, 1, 0, 8 );
			GL_DrawColoredStereoLinePair( 1, 1, 0, 10);
			GL_DrawColoredStereoLinePair( 1, 1, 0, 12);
			GL_DrawColoredStereoLinePair( 0, 1, 0, 14);
		qglEnd();

		GU_EndFrame();
	}
}
#endif

/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

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

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetLightLevel ();
	R_SetGL2D ();
}


void R_Register( void )
{
#ifdef KINGPIN_ENGINE
	r_showbbox = ri.Cvar_Get ("r_showbbox", "0", 0);
	r_detail_alpha = ri.Cvar_Get ("r_detail_alpha", "0.2", CVAR_ARCHIVE);
	r_detail_scale = ri.Cvar_Get ("r_detail_scale", "8.0", CVAR_ARCHIVE);
	r_detail_dist = ri.Cvar_Get ("r_detail_dist", "256", CVAR_ARCHIVE);
	r_directional_lighting = ri.Cvar_Get ("r_directional_lighting", "1", CVAR_ARCHIVE);
#endif
	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_novis = ri.Cvar_Get ("r_novis", "0", 0);
	r_nocull = ri.Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = ri.Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0);

	gl_particle_scale = ri.Cvar_Get( "gl_particle_scale", "0.5", CVAR_ARCHIVE );
	gl_particle_point = ri.Cvar_Get( "gl_particle_point", "1", CVAR_ARCHIVE );

	gl_modulate = ri.Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE );
	gl_log = ri.Cvar_Get( "gl_log", "0", 0 );
	gl_mode = ri.Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE );
	gl_lightmap = ri.Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE );
	gl_dynamic = ri.Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get ("gl_round_down", "1", 0);
	gl_picmip = ri.Cvar_Get ("gl_picmip", "0", 0);
	gl_skymip = ri.Cvar_Get ("gl_skymip", "0", 0);
	gl_skytga = ri.Cvar_Get ("gl_skytga", "0", 0);
	gl_showtris = ri.Cvar_Get ("gl_showtris", "0", 0);
	gl_clear = ri.Cvar_Get ("gl_clear", "0", 0);
	gl_cull = ri.Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get ("gl_flashblend", "1", 0);
	gl_monolightmap = ri.Cvar_Get( "gl_monolightmap", "0", 0 );
	gl_lockpvs = ri.Cvar_Get( "gl_lockpvs", "0", 0 );
	gl_saturatelighting = ri.Cvar_Get( "gl_saturatelighting", "0", 0 );

#ifdef KINGPIN_ENGINE
	gl_fog = ri.Cvar_Get ("gl_fog", "1", CVAR_ARCHIVE);
	gl_reflections = ri.Cvar_Get ("gl_reflections", "1", 0);
	gl_specular = ri.Cvar_Get ("gl_specular", "1", CVAR_ARCHIVE);
	gl_test_specular = ri.Cvar_Get ("gl_test_specular", "0", 0);
	r_sfx_pause = ri.Cvar_Get ("r_sfx_pause", "0", 0);
	r_debug_lighting = ri.Cvar_Get ("r_debug_lighting", "0", 0);
	r_painskins = ri.Cvar_Get ("r_painskins", "1", CVAR_ARCHIVE);
	gl_maxtexsize = ri.Cvar_Get ("gl_maxtexsize", "999", CVAR_ARCHIVE);
	burn_enabled = ri.Cvar_Get ("burn_enabled", "0", 0);
	burn_r = ri.Cvar_Get ("burn_r", "1.0", 0);
	burn_g = ri.Cvar_Get ("burn_g", "1.0", 0);
	burn_b = ri.Cvar_Get ("burn_b", "1.0", 0);
#endif

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
	vid_ref = ri.Cvar_Get( "vid_ref", "gu", CVAR_ARCHIVE );

	ri.Cmd_AddCommand( "imagelist", GL_ImageList_f );
	ri.Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
	ri.Cmd_AddCommand( "modellist", Mod_Modellist_f );
#ifdef KINGPIN_ENGINE
	ri.Cmd_AddCommand ("burn_pickcolor", R_BurnPickColor_f);
#endif
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

	if (vid_fullscreen->modified)
	{
		ri.Con_Printf( PRINT_ALL, "R_SetMode() - CDS not allowed with this driver\n" );
		ri.Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->value );
		vid_fullscreen->modified = false;
	}

	fullscreen = vid_fullscreen->value;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	if ( ( err = GU_SetMode( &vid.width, &vid.height, gl_mode->value, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = gl_mode->value;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			ri.Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GU_SetMode( &vid.width, &vid.height, gl_mode->value, false ) ) == rserr_ok )
				return true;
		}
		else if ( err == rserr_invalid_mode )
		{
			ri.Cvar_SetValue( "gl_mode", gl_state.prev_mode );
			gl_mode->modified = false;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GU_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
}

/*
===============
R_Init
===============
*/
qboolean R_Init( void *hinstance, void *hWnd )
{
	int		err;
	int		j;
	extern float r_turbsin[256];

	for ( j = 0; j < 256; j++ )
	{
		r_turbsin[j] *= 0.5;
	}

	memset(&gl_state, 0, sizeof(gl_state));

	gl_state.currenttexture = NULL;
	gl_state.currenttexenv = -1;

	ri.Con_Printf (PRINT_ALL, "ref_gu version: "REF_VERSION"\n");

	Draw_GetPalette ();

	R_Register();

	// set our "safe" modes
	gl_state.prev_mode = 3;

	// initialize OS-specific parts
	if (!GU_Init( hinstance, hWnd ))
		return false;

	// create the window and set up the context
	if ( !R_SetMode () )
	{
        ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n" );
		return false;
	}

	//ri.Vid_MenuInit();

	/*
	** draw our stereo patterns
	*/
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern();
#endif

	GL_InitImages ();
	Mod_Init ();
	R_InitParticleTexture ();
#ifdef KINGPIN_ENGINE
	R_InitSFX ();
#endif
	Draw_InitLocal ();

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{
	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("screenshot");
	ri.Cmd_RemoveCommand ("imagelist");
#ifdef KINGPIN_ENGINE
	ri.Cmd_RemoveCommand ("burn_pickcolor");
#endif

#ifdef KINGPIN_ENGINE
	R_ClearSFX ();
#endif

	Mod_FreeAll ();

	GL_ShutdownImages ();

	/*
	** shut down OS specific GU stuff like contexts, etc.
	*/
	GU_Shutdown();
}



/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginFrame( float camera_separation )
{

	gl_state.camera_separation = camera_separation;

	/*
	** change modes if necessary
	*/
	if ( gl_mode->modified || vid_fullscreen->modified )
	{	// FIXME: only restart if CDS is required
		cvar_t	*ref;

		ref = ri.Cvar_Get ("vid_ref", "gu", 0);
		ref->modified = true;
	}

	if ( gl_log->modified )
	{
		GU_EnableLogging( gl_log->value );
		gl_log->modified = false;
	}

	if ( gl_log->value )
	{
		GU_LogNewFrame();
	}

	if ( vid_gamma->modified || intensity->modified )
	{
		GL_BuildGammaTable (vid_gamma->value, intensity->value);
		if ( vid_gamma->modified )
			R_SetPalette (NULL);

		vid_gamma->modified = false;
		intensity->modified = false;
	}

	GU_BeginFrame( camera_separation );

	/*
	** go into 2D mode
	*/
	R_SetGL2D ();

	//
	// clear screen if desired
	//
	R_Clear ();
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
#ifdef KINGPIN_ENGINE
	gl_state.screenpalette = (byte*)&r_rawpalette[0];
#endif

	sceGuClearColor (0x00000000);
	sceGuClear (GU_COLOR_BUFFER_BIT | GU_FAST_CLEAR_BIT);
	sceGuClearColor (GU_HCOLOR_4F(1.0, 0.0, 0.5 , 0.5));
}

/*
** R_DrawBeam
*/
void R_DrawBeam( entity_t *e )
{
#define BEAM_SEGS_LENGTH 64.0f

	vec3_t	mins, maxs;
	vec3_t	start, end, beam_vec, side_vec, cam_vec, beam_norm;
	vec3_t	step_vec, core_pos;
	float	length, radius;
	int		i, num_segs, num_verts;

	radius = e->frame * 0.5f;

	for (i = 0 ; i < 3 ; i++)
	{
		if (e->origin[i] < e->oldorigin[i])
		{
			mins[i] = e->origin[i] - radius;
			maxs[i] = e->oldorigin[i] + radius;
		}
		else
		{
			mins[i] = e->oldorigin[i] - radius;
			maxs[i] = e->origin[i] + radius;
		}
	}

	if (GU_ClipCullBox (mins, maxs) == GU_CULL_OUTSIDE)
		return;

	VectorCopy (e->origin, start);
	VectorCopy (e->oldorigin, end);

	VectorSubtract (end, start, beam_vec);

	// beam length
	length = VectorLength (beam_vec);
	if (length < 1.0f)
		return;

	// beam segments count
	num_segs = (int)(length / BEAM_SEGS_LENGTH);
	num_segs = Q_bound (1, num_segs, 32);

	num_verts = (num_segs + 1) * 2;

	// create sprite
	VectorScale (beam_vec, 1.0f / length, beam_norm);
	VectorSubtract (start, r_newrefdef.vieworg, cam_vec);
	CrossProduct (beam_norm, cam_vec, side_vec);

	VectorNormalize (side_vec);
	VectorScale (side_vec, radius, side_vec);

	gu_color_32_t color;
	color.c = d_8to24table[e->skinnum & 0xFF];
	color.a = (byte)(e->alpha * 255.0f);

	sceGuDisable (GU_TEXTURE_2D);
	sceGuEnable (GU_BLEND);
	sceGuColor (color.c);

	VectorScale (beam_vec, 1.0f / (float)num_segs, step_vec);
	VectorCopy (start, core_pos);

	gu_vert_fv_t* out = (gu_vert_fv_t*)sceGuGetMemory (sizeof(gu_vert_fv_t) * num_verts);
	gu_vert_fv_t* out_ptr = out;
	for (i = 0; i <= num_segs; i++, out_ptr += 2)
	{
		VectorSubtract(core_pos, side_vec, out_ptr[0].xyz);
		VectorAdd(core_pos, side_vec, out_ptr[1].xyz);
		VectorAdd(core_pos, step_vec, core_pos);
	}
	sceGuDrawArray (GU_TRIANGLE_STRIP,  GU_VERTEX_32BITF, num_verts, 0, out);

	sceGuEnable (GU_TEXTURE_2D);
	sceGuDisable (GU_BLEND);
	sceGuColor (GU_HCOLOR_DEFAULT);

	DEBUG_DrawBeamAABB (mins, maxs);
}

//===================================================================


void	R_BeginRegistration (char *map);
struct model_s	*R_RegisterModel (char *name);
struct image_s	*R_RegisterSkin (char *name);
void	R_EndRegistration (void);
void	R_ClearRegistered (void);

void	R_RenderFrame (refdef_t *fd);

struct image_s	*Draw_FindPic (char *name);
/*
void	Draw_Pic (int x, int y, char *name);
void	Draw_Char (int x, int y, int c);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill (int x, int y, int w, int h, int c);
void	Draw_FadeScreen (void);
*/

#if 1

void R_SetSkyKp(char *name)
{
	char	reflection_name[MAX_QPATH];
	vec3_t	zero_vec;

	VectorClear(zero_vec);

	R_SetSky(name, 0.0f, zero_vec);


	if (r_worldreflectiontexture)
		GL_FreeImage (r_worldreflectiontexture);

	Com_sprintf (reflection_name, sizeof(reflection_name), "env/%swinrefl.tga", name);
	r_worldreflectiontexture = GL_FindImage (reflection_name, IMG_TYPE_SKY);
	//if (!r_worldreflectiontexture)
	//	r_worldreflectiontexture = r_notexture;
}
#endif

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t GetRefAPI (refimport_t rimp )
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;
#ifdef KINGPIN_ENGINE
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.RegisterModelSkin = R_RegisterModelSkin;

	re.SetSky = R_SetSkyKp;
	re.SetFog = R_SetFog;

	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.ClearDepth = R_ClearDepth;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawPicTransparent = Draw_PicTransparent;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawStretchPicTransparent = Draw_StretchPicTransparent;
	re.DrawChar = Draw_Char;
	re.DrawCharUpper = Draw_CharUpper;
	re.DrawChar2 = Draw_Char2;
	re.DrawBigChar = Draw_BigChar;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFillRGBA = Draw_FillRGBA;
	re.DrawLine = Draw_Line;
	re.DrawFadeScreen= Draw_FadeScreen;
	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GU_EndFrame;

	re.AppActivate = GU_AppActivate;

	re.AddSFX = R_AddSFX;

	R_ResetSFXLists ();

#else
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;
	re.ClearRegistered = R_ClearRegistered;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawLine = Draw_Line;
	re.DrawFadeScreen= Draw_FadeScreen;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GU_EndFrame;

	re.AppActivate = GU_AppActivate;
#endif
	return re;
}


#ifndef REF_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, fmt);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}

#endif
