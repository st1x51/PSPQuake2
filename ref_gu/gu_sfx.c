// gu_sfx.c
#include "gu_local.h"
#include <stdarg.h>
#include <stdio.h>

static void R_SFXLog (const char *fmt, ...);

typedef struct r_sfx_s
{
	image_t	*image;

	int		sprite;
	int		flags;

	vec3_t	origin;
	vec3_t	velocity;

	float	gravity;
	float	spawn_time;
	float	lifetime;
	float	start_alpha;
	float	end_alpha;
	float	fade_time;
	float	start_width;
	float	end_width;
	float	start_height;
	float	end_height;

	int		flip_s;
	int		flip_t;

	float	age;
	float	last_time;

	vec3_t	surface_right;
	vec3_t	surface_up;

	struct r_sfx_s	*next;
	struct r_sfx_s	*prev;
} r_sfx_t;

typedef struct
{
	qboolean	valid;
	image_t		*image;
	float		alpha;
	vec3_t		center;
	vec3_t		axis0;
	vec3_t		axis1;
	float		half_width;
	float		half_height;
	ScePspFMatrix4 model __attribute__((aligned(16)));
	ScePspFMatrix4 view __attribute__((aligned(16)));
	ScePspFMatrix4 projection __attribute__((aligned(16)));
} r_deferred_sfx_t;

image_t *r_sfx_images[MAX_SFX_SPRITES];

static r_sfx_t *r_sfx_lists[MAX_SFX_SPRITES];
static int r_num_sfx;

static r_deferred_sfx_t r_deferred_sfx;

static float r_sfx_time;
static float r_sfx_framerate;

#define R_SFXUsesFlameQuad(sprite) ((sprite) == SFX_SPRITE_FLAMETHROWER1 || \
	(sprite) == SFX_SPRITE_FLAMETHROWER2 || \
	(sprite) == SFX_SPRITE_FLAMETHROWER_BLUE3)

/*
===============
R_SFXTime
===============
*/
float R_SFXTime (void)
{
	return r_sfx_time;
}

/*
===============
R_SFXFramerate
===============
*/
float R_SFXFramerate (void)
{
	return r_sfx_framerate;
}

/*
===============
R_UpdateSFXTime
===============
*/
void R_UpdateSFXTime (void)
{
	static float previous_time;
	float elapsed;

	/* ref_gl_original.dll updates DAT_1008e0c8 once at the start of
	 * R_RenderView.  AddSFX calls made by the client before RenderFrame thus
	 * retain the preceding frame time, which is essential to the 0.01
	 * one-draw sentinel. */
	r_sfx_time = Sys_Milliseconds () * 0.001f;
	elapsed = r_sfx_time - previous_time;
	r_sfx_framerate = (float)fabs (elapsed) >= 2.0f ? 99.0f : 1.0f / elapsed;
	previous_time = r_sfx_time;
}

/*
===============
R_UnlinkSFX
===============
*/
static void R_UnlinkSFX (r_sfx_t *sfx)
{
	if (sfx->prev)
		sfx->prev->next = sfx->next;
	else
		r_sfx_lists[sfx->sprite] = sfx->next;

	if (sfx->next)
		sfx->next->prev = sfx->prev;

	free (sfx);

	if (r_num_sfx > 0)
		r_num_sfx--;
}

/*
===============
R_ClearSFX
===============
*/
void R_ClearSFX (void)
{
	int		i;
	r_sfx_t	*sfx, *next;
	image_t	*image;

	for (i = 0; i < MAX_SFX_SPRITES; i++)
	{
		sfx = r_sfx_lists[i];
		while (sfx)
		{
			next = sfx->next;
			free (sfx);
			sfx = next;
		}
		r_sfx_lists[i] = NULL;

		image = r_sfx_images[i];
		if(image && image != r_notexture)
			GL_FreeImage(image);
		r_sfx_images[i] = r_notexture;
	}
	r_num_sfx = 0;
	memset (&r_deferred_sfx, 0, sizeof(r_deferred_sfx));
}

/*
===============
R_ResetSFXLists
===============
*/
void R_ResetSFXLists (void)
{
	/* GetRefAPI in the retail DLL only zeros the 256 list heads. */
	memset (r_sfx_lists, 0, sizeof(r_sfx_lists));
}

/*
===============
R_RegisterSFXImage
===============
*/
static void R_RegisterSFXImage (int sprite, char *name)
{
	char	pathname[MAX_QPATH];

	Com_sprintf(pathname, sizeof(pathname), "sprites/%s.tga", name);
	printf("R_RegisterSFXImage: %i:%s", sprite, pathname);

	image_t *image = GL_FindImage (pathname, IMG_TYPE_SFX);

	if (image)
		r_sfx_images[sprite] = image;
}

/*
===============
R_InitSFX
===============
*/
void R_InitSFX (void)
{
	int			i;

	R_ClearSFX ();

	for (i = 0; i < MAX_SFX_SPRITES; i++)
		r_sfx_images[i] = r_notexture;

	R_RegisterSFXImage (SFX_SPRITE_SURF_BLOOD1, "blood1");
	R_RegisterSFXImage (SFX_SPRITE_SURF_BULLET1, "bullet1");
	R_RegisterSFXImage (SFX_SPRITE_SURF_RIPPLE, "ripple");
	R_RegisterSFXImage (SFX_SPRITE_SURF_BLOOD_POOL, "bld_pl");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD_SPLAT, "bld_spt");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD_DRIP, "bld_dr");
	R_RegisterSFXImage (SFX_SPRITE_SMOKE_SMALL, "smoke_sm");
	R_RegisterSFXImage (SFX_SPRITE_FIRE, "fire");
	R_RegisterSFXImage (SFX_SPRITE_CORONA, "corona");
	R_RegisterSFXImage (SFX_SPRITE_FIRE_ALT, "fire");
	R_RegisterSFXImage (SFX_SPRITE_CORONA_ALPHA, "corona_a");
	R_RegisterSFXImage (SFX_SPRITE_CORONA_RED, "corona_r");
	R_RegisterSFXImage (SFX_SPRITE_CORONA_GREEN, "corona_g");
	R_RegisterSFXImage (SFX_SPRITE_CORONA_BLUE, "corona_b");
	R_RegisterSFXImage (SFX_SPRITE_RAIN, "rain");
	R_RegisterSFXImage (SFX_SPRITE_SMOKE_SMALL_RED, "smoke_sm_r");
	R_RegisterSFXImage (SFX_SPRITE_SNIPER1C, "sniper1c");
	R_RegisterSFXImage (SFX_SPRITE_SNIPER2, "sniper2");
	R_RegisterSFXImage (SFX_SPRITE_RIPPLE_ALT, "ripple");

	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 0, "blood/blood1");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 1, "blood/blood1b");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 2, "blood/blood1c");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 3, "blood/blood1d");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 4, "blood/blood1e");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 5, "blood/blood1f");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 6, "blood/blood1g");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 7, "blood/blood1h");
	R_RegisterSFXImage (SFX_SPRITE_BLOOD1 + 8, "blood/blood1i");

	R_RegisterSFXImage (SFX_SPRITE_MUZZLE3_ALT, "muzz3");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE_HMG, "muzzhmg");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE1, "muzz1");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE2, "muzz2");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE3, "muzz3");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE4, "muzz4");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE1_ALT, "muzz1a");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE2_ALT, "muzz2a");
	R_RegisterSFXImage (SFX_SPRITE_MUZZLE2B, "muzz2b");
	R_RegisterSFXImage (SFX_SPRITE_EXPLOSION1, "explode/explode1"); // it_sky?
	R_RegisterSFXImage (SFX_SPRITE_EXPLOSION2, "explode/explode2"); // it_sky?
	R_RegisterSFXImage (SFX_SPRITE_SMOKE_ANIM1, "smoke/smokeanim1"); // it_sky?
	R_RegisterSFXImage (SFX_SPRITE_FLAMETHROWER1, "fthrow1");
	R_RegisterSFXImage (SFX_SPRITE_FLAMETHROWER_BLUE4, "fthrow_blue4");
	R_RegisterSFXImage (SFX_SPRITE_SPLASH1, "splash1");
	R_RegisterSFXImage (SFX_SPRITE_SPLASH2, "splash2");
	R_RegisterSFXImage (SFX_SPRITE_SPLASH3, "splash3");
	R_RegisterSFXImage (SFX_SPRITE_SPLASH4, "splash4");
	R_RegisterSFXImage (SFX_SPRITE_FIREWALL, "firewall");
}

/*
===============
R_SFXAlpha
===============
*/
static float R_SFXAlpha (r_sfx_t *sfx, float age)
{
	float fade = (float)fabs (sfx->fade_time);
	float fade_end = fade;

	if (fade > sfx->lifetime)
		fade_end = fade - sfx->lifetime;

	if (fade != 0 && age <= fade_end)
	{
		if (sfx->fade_time < 0 || (sfx->flags & SFX_FLAG_SURFACE_EFFECT))
		{
			if (sfx->lifetime < fade)
				return 0;
			return sfx->start_alpha;
		}
		return fade_end != 0 ? sfx->start_alpha * age / fade_end : sfx->start_alpha;
	}

	if (sfx->lifetime <= fade_end)
		return sfx->end_alpha;
	return sfx->start_alpha + (sfx->end_alpha - sfx->start_alpha) *
		((age - fade_end) / (sfx->lifetime - fade_end));
}

/*
===============
R_SFXAdditive
===============
*/
static qboolean R_SFXAdditive (r_sfx_t *sfx)
{
	if (sfx->flags & (SFX_FLAG_LIGHTFLARE | SFX_FLAG_ADDITIVE_BLEND))
		return true;
	if (sfx->sprite == SFX_SPRITE_FIRE || sfx->sprite == SFX_SPRITE_FIRE_ALT ||
		sfx->sprite == SFX_SPRITE_EXPLOSION1 || sfx->sprite == SFX_SPRITE_EXPLOSION2 ||
		sfx->sprite == SFX_SPRITE_FIREWALL)
		return true;
	if (sfx->sprite == SFX_SPRITE_SMOKE_SMALL /*&& (gl_config.renderer & GL_RENDERER_3DFX)*/)
		return true;
	return false;
}

/*
===============
R_SFXTexCoords
===============
*/
static void R_SFXTexCoords (r_sfx_t *sfx, float fraction,
		float *s0, float *t0, float *s1, float *t1)
{
	if (sfx->sprite == SFX_SPRITE_EXPLOSION1 ||
		sfx->sprite == SFX_SPRITE_EXPLOSION2 ||
		sfx->sprite == SFX_SPRITE_SMOKE_ANIM1)
	{
		int frame;
		frame = (int)(fraction * 16);
		*s0 = (frame & 3) * 0.25f;
		*t0 = (frame >> 2) * 0.25f;
		*s1 = *s0 + 0.25f;
		*t1 = *t0 + 0.25f;
	}
	else
	{
#if 1
		*s0 = 0.0f;
		*t0 = 0.0f;
		*s1 = 1.0f;
		*t1 = 1.0f;
#else
		*s0 = sfx->image->sl;
		*t0 = sfx->image->tl;
		*s1 = sfx->image->sh;
		*t1 = sfx->image->th;
#endif
	}

	if (sfx->flip_s)
	{
		float swap = *s0;
		*s0 = *s1;
		*s1 = swap;
	}
	if (sfx->flip_t)
	{
		float swap = *t0;
		*t0 = *t1;
		*t1 = swap;
	}
}

/*
===============
R_DrawFlameQuad
===============
*/
static void R_DrawFlameQuad (r_sfx_t *sfx, vec3_t center, vec3_t right, vec3_t up,
	vec3_t color, float alpha, float width, float height)
{
	vec3_t	verts[4];
	float	crop_height;
	float	edge_alpha;
	float	edge_t;
	float	s0, s1;
	gu_color_32_t	gu_color;


	/* FUN_1000d800 @ 1000dcae: the flame/explode2 path is a skewed,
	 * per-vertex-faded quad rather than the ordinary centred billboard. */
	crop_height = alpha <= 0.5f ? (1.0f - alpha * 2.0f) * height : 0.0f;
	edge_alpha = alpha * 2.0f - 1.0f;
	edge_t = alpha <= 0.5f ? alpha * 2.0f : 1.0f;
	s0 = (float)sfx->flip_s;
	s1 = (float)!sfx->flip_s;

	VectorMA (center, crop_height - height * 0.5f, up, verts[0]);
	VectorMA (verts[0], width * -0.5f, right, verts[0]);
	VectorMA (center, height * 0.5f, up, verts[1]);
	VectorMA (verts[1], edge_t, right, verts[1]);
	VectorMA (verts[1], width, right, verts[2]);
	VectorMA (verts[2], crop_height - height, up, verts[3]);

	gu_color.c = GU_HCOLOR_3F(color[0], color[1], color[2]);

	GL_Bind (sfx->image);
#if 1
	gu_vert_ftcv_t* const out = (gu_vert_ftcv_t*)sceGuGetMemory (sizeof(gu_vert_ftcv_t) * 4);

	out[0].u = s0;
	out[0].v = edge_t;
	out[0].c = gu_color.c;
	out[0].a = (byte)(edge_alpha * 255.0f); // replace alpha
	VectorCopy(verts[0], out[0].xyz);

	out[1].u = s0;
	out[1].v = 0.0f;
	out[1].c = gu_color.c;
	out[1].a = (byte)(alpha * 255.0f); // replace alpha
	VectorCopy(verts[1], out[1].xyz);

	out[2].u = s1;
	out[2].v = 0.0f;
	out[2].c = out[1].c;
	VectorCopy(verts[2], out[2].xyz);

	out[3].u = s1;
	out[3].v = edge_t;
	out[3].c = out[0].c;
	VectorCopy(verts[3], out[3].xyz);

	sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF,  4, 0, out);
#else
	qglBegin (GL_QUADS);

	qglColor4f (color[0], color[1], color[2], edge_alpha);
	qglTexCoord2f (s0, edge_t);
	qglVertex3fv (verts[0]);

	qglColor4f (color[0], color[1], color[2], alpha);
	qglTexCoord2f (s0, 0.0f);
	qglVertex3fv (verts[1]);

	qglTexCoord2f (s1, 0.0f);
	qglVertex3fv (verts[2]);

	qglColor4f (color[0], color[1], color[2], edge_alpha);
	qglTexCoord2f (s1, edge_t);
	qglVertex3fv (verts[3]);

	qglEnd ();
#endif
}

/*
===============
R_DrawOneSFX
===============
*/
static void R_DrawOneSFX (r_sfx_t *sfx, float alpha, float fraction,
	float dt, float now)
{
	vec3_t center, right, up, delta;
	vec3_t verts[4];
	float width, height;
	float s0, t0, s1, t1;
	float color[3] = {1, 1, 1};

	if (sfx->flags & (SFX_FLAG_SURFACE_EFFECT | SFX_FLAG_LIGHTFLARE))
		VectorCopy (sfx->origin, center);
	else
	{
		VectorMA (sfx->origin, sfx->age, sfx->velocity, center);
		if (sfx->flags & SFX_FLAG_GRAVITY)
		{
			sfx->gravity += dt * SFX_GRAVITY;
			center[2] -= sfx->gravity * sfx->age;
		}
	}

	width = sfx->start_width + (sfx->end_width - sfx->start_width) * fraction;
	height = sfx->start_height + (sfx->end_height - sfx->start_height) * fraction;
	if (sfx->sprite == SFX_SPRITE_SURF_BLOOD_POOL &&
		now - sfx->spawn_time < 4.0f)
	{
		/* Retail updates the stored dimensions for the next traversal after
		 * calculating this frame's interpolated size. */
		float size = (now - sfx->spawn_time) * 12.0f;
		sfx->start_width = sfx->end_width = size;
		sfx->start_height = sfx->end_height = size;
	}

	VectorSubtract (center, r_origin, delta);
	if ((sfx->flags & SFX_FLAG_CLOSE_CULL) &&
		VectorLength (delta) < width + 32.0f)
		return;

	if (sfx->flags & SFX_FLAG_SURFACE_EFFECT)
	{
		VectorCopy (sfx->surface_right, right);
		VectorCopy (sfx->surface_up, up);
	}
	else if (sfx->flags & SFX_FLAG_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		VectorSet (right, -vpn[1], vpn[0], 0);
		if (VectorNormalize (right) == 0)
			VectorSet (right, 1, 0, 0);
	}
	else
	{
		VectorCopy (vright, right);
		VectorCopy (vup, up);
	}
	if (sfx->sprite >= SFX_SPRITE_FLAMETHROWER1 &&
		sfx->sprite <= SFX_SPRITE_FLAMETHROWER_BLUE3)
		center[2] += height * 0.25f;

	if ((sfx->flags & SFX_FLAG_VEL_IS_COLOR_MODULATION) ||
		((sfx->flags & SFX_FLAG_LIGHTFLARE) &&
		 (sfx->velocity[0] + sfx->velocity[1] + sfx->velocity[2] > 0.01f)))
		VectorCopy (sfx->velocity, color);

	if (R_SFXUsesFlameQuad (sfx->sprite))
	{
		R_DrawFlameQuad (sfx, center, right, up, color,
			alpha, width, height);
		return;
	}

	VectorScale (right, width * 0.5f, right);
	VectorScale (up, height * 0.5f, up);
	VectorSubtract (center, right, verts[0]);
	VectorAdd (verts[0], up, verts[0]);
	VectorAdd (center, right, verts[1]);
	VectorAdd (verts[1], up, verts[1]);
	VectorAdd (center, right, verts[2]);
	VectorSubtract (verts[2], up, verts[2]);
	VectorSubtract (center, right, verts[3]);
	VectorSubtract (verts[3], up, verts[3]);

	R_SFXTexCoords (sfx, fraction, &s0, &t0, &s1, &t1);

	GL_Bind (sfx->image);
	sceGuColor(GU_HCOLOR_4F(color[0], color[1], color[2], alpha));
#if 1
	gu_vert_ftv_t* const out = (gu_vert_ftv_t*)sceGuGetMemory (sizeof(gu_vert_ftv_t) * 4);

	out[0].u = s0;
	out[0].v = t0;
	VectorCopy(verts[0], out[0].xyz);

	out[1].u = s1;
	out[1].v = t0;
	VectorCopy(verts[1], out[1].xyz);

	out[2].u = s1;
	out[2].v = t1;
	VectorCopy(verts[2], out[2].xyz);

	out[3].u = s0;
	out[3].v = t1;
	VectorCopy(verts[3], out[3].xyz);

	sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF,  4, 0, out);
#else
	qglColor4f (color[0], color[1], color[2], alpha);
	qglBegin (GL_QUADS);
	qglTexCoord2f (s0, t0); qglVertex3fv (verts[0]);
	qglTexCoord2f (s1, t0); qglVertex3fv (verts[1]);
	qglTexCoord2f (s1, t1); qglVertex3fv (verts[2]);
	qglTexCoord2f (s0, t1); qglVertex3fv (verts[3]);
	qglEnd ();
#endif
}

/*
===============
R_DrawSFX
===============
*/
void R_DrawSFX (void)
{
	int i;
	float now = R_SFXTime ();
	qboolean additive = false;

	sceGuDepthMask (GU_TRUE);
	sceGuDisable (GU_ALPHA_TEST);
	sceGuDisable (GU_CULL_FACE);
	sceGuEnable (GU_BLEND);
	sceGuShadeModel (GU_SMOOTH);

	GL_TexEnv (GU_TFX_MODULATE);

	for (i = 0; i < MAX_SFX_SPRITES; i++)
	{
		r_sfx_t *sfx = r_sfx_lists[i];
		while (sfx)
		{
			r_sfx_t *next = sfx->next;
			float dt = now - sfx->last_time;
			float alpha, fraction;
			qboolean base_additive, want_additive;

			/* FUN_1000e2f0 promotes every light flare to the depth-hacked
			 * path immediately before drawing it.  AddSFX receives 0x0c from
			 * the client, but retail therefore renders the corona as 0x1c. */
			if (sfx->flags & SFX_FLAG_LIGHTFLARE)
				sfx->flags |= SFX_FLAG_DEPTHHACK;

			if (sfx->lifetime == 0 &&
				(dt == 0 || r_sfx_pause->value != 0) &&
				(dt == 0 || ((sfx->sprite < SFX_SPRITE_FLAMETHROWER1 ||
				 sfx->sprite > SFX_SPRITE_FLAMETHROWER2) &&
				 sfx->sprite != SFX_SPRITE_EXPLOSION1 &&
				 sfx->sprite != SFX_SPRITE_EXPLOSION2)))
				sfx->lifetime = 0.01f;
			sfx->last_time = now;
			if (!r_sfx_pause->value ||
				((((sfx->sprite >= SFX_SPRITE_FLAMETHROWER1 &&
				    sfx->sprite <= SFX_SPRITE_FLAMETHROWER2) ||
				   sfx->sprite == SFX_SPRITE_EXPLOSION1 ||
				   sfx->sprite == SFX_SPRITE_EXPLOSION2) && dt != 0) ||
				 (sfx->sprite == SFX_SPRITE_FLAMETHROWER_BLUE3 && dt != 0)))
				sfx->age += dt;

			/* Retail treats 0.01 as a one-draw sentinel, not a ten-millisecond
			 * lifetime.  It survives this timeout check and is retired only after
			 * the first traversal in which time has advanced. */
			if (now < sfx->spawn_time ||
				(sfx->age >= sfx->lifetime && sfx->lifetime != 0.01f))
			{
				R_UnlinkSFX (sfx);
				sfx = next;
				continue;
			}

			fraction = sfx->lifetime > 0 ? sfx->age / sfx->lifetime : 1;
			alpha = R_SFXAlpha (sfx, sfx->age);
			/* A 0.01 effect is drawn once after time advances, then marked for
			 * removal on the following traversal (FUN_1000d800 @ 1000da06). */
			if (dt != 0 && sfx->lifetime == 0.01f)
				sfx->lifetime = 0;
			if (sfx->flags & SFX_FLAG_ALPHA_IS_FRAME)
			{
				/* Retail uses the already evaluated alpha curve as the atlas
				 * frame coordinate, then renders that frame fully opaque. */
				fraction = alpha;
				alpha = 1;
			}
			if (alpha <= 0)
			{
				sfx = next;
				continue;
			}
			if (alpha > 1)
				alpha = 1;

			base_additive = R_SFXAdditive (sfx);
			/* Retail temporarily forces velocity-coloured effects to ordinary
			 * alpha blending, then restores the sprite's normal blend mode. */
			want_additive = (sfx->flags & SFX_FLAG_VEL_IS_COLOR_MODULATION) ?
				false : base_additive;
			if (want_additive != additive)
			{
				sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, 
					want_additive ? GU_FIX : GU_ONE_MINUS_SRC_ALPHA, 0, GU_HBLEND_ONE);
				additive = want_additive;
			}

			if (sfx->flags & SFX_FLAG_DEPTHHACK)
				sceGuDepthRange (gldepthmin, gldepthmin + 0.3f * (gldepthmax - gldepthmin));

			{
				static int draw_logged[MAX_SFX_SPRITES];

				if (draw_logged[sfx->sprite] < 5)
				{
					draw_logged[sfx->sprite]++;
					R_SFXLog ("draw sprite %d alpha %.2f frac %.2f age %.2f additive %d img %s\n",
						sfx->sprite, alpha, fraction, sfx->age, want_additive, sfx->image->name);
				}
			}

			R_DrawOneSFX (sfx, alpha, fraction, dt, now);

			if (sfx->flags & SFX_FLAG_DEPTHHACK)
				sceGuDepthRange (gldepthmin, gldepthmax);

			if (sfx->flags & SFX_FLAG_VEL_IS_COLOR_MODULATION)
			{
				//qglColor4f (1, 1, 1, 1);
				sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA,
					base_additive ? GU_FIX : GU_ONE_MINUS_SRC_ALPHA, 0, GU_HBLEND_ONE);
				additive = base_additive;
			}
			sfx = next;
		}
	}

	sceGuShadeModel (GU_FLAT);
	sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDisable (GU_BLEND);
	sceGuEnable (GU_ALPHA_TEST);
	sceGuEnable (GU_CULL_FACE);
	sceGuDepthMask (GU_FALSE);

	sceGuColor (GU_HCOLOR_DEFAULT);

	GL_TexEnv (GU_TFX_REPLACE);
}

/*
===============
R_SFXLog

Debug output to <gamedir>/sfx_debug.log (limited number of lines)
===============
*/
static void R_SFXLog (const char *fmt, ...)
{
	static int	lines;
	char		path[MAX_OSPATH];
	va_list		args;
	FILE		*f;

	if (lines >= 300)
		return;
	lines++;

	Com_sprintf (path, sizeof(path), "%s/sfx_debug.log", ri.FS_GetWriteDir (FS_PATH_GAMEDIR));
	f = fopen (path, "a");
	if (!f)
		return;
	va_start (args, fmt);
	vfprintf (f, fmt, args);
	va_end (args);
	fclose (f);
}

/*
===============
R_AddSFX
===============
*/
void R_AddSFX (int sprite, int flags, vec3_t origin, vec3_t vel, int unused,
			float lifetime, float start_alpha, float end_alpha, float fadein_time,
			float start_width, float end_width, float start_height, float end_height)
{
	r_sfx_t *sfx;
	float now;

	(void)unused;
	if (sprite < 0 || sprite >= MAX_SFX_SPRITES || !origin || !vel)
		return;

	if ((flags & SFX_FLAG_SURFACE_EFFECT) && start_width > 2.0f &&
		sprite != SFX_SPRITE_FLAMETHROWER_BLUE4 &&
		sprite != SFX_SPRITE_SNIPER1C && sprite != SFX_SPRITE_SNIPER2 &&
		sprite != SFX_SPRITE_RIPPLE_ALT && sprite != SFX_SPRITE_FLAMETHROWER1)
	{
		r_sfx_t *check;
		r_sfx_t *oldest_live = NULL;
		int decal_count = 0;

		for (check = r_sfx_lists[sprite]; check; check = check->next)
		{
			vec3_t delta;

			if (!(check->flags & SFX_FLAG_SURFACE_EFFECT) ||
				check->sprite != sprite)
				continue;
			if (sprite == SFX_SPRITE_SURF_BLOOD1 ||
				sprite == SFX_SPRITE_SURF_BULLET1)
			{
				decal_count++;
				if (check->lifetime != 0)
					oldest_live = check;
			}
			VectorSubtract (check->origin, origin, delta);
			if (check->lifetime > 0.01f &&
				VectorLength (delta) < (start_width + check->start_width) * 0.125f)
				return;
		}

		/* Retail keeps separate pools for bullet and blood surface decals.
		 * The list is newest-first, so the last live record is the oldest. */
		if (((sprite == SFX_SPRITE_SURF_BULLET1 && decal_count >= 200) ||
			 (sprite == SFX_SPRITE_SURF_BLOOD1 && decal_count >= 400)) && oldest_live)
			oldest_live->lifetime = 0;
	}

	sfx = (r_sfx_t *)calloc (1, sizeof(*sfx));
	if (!sfx)
		return;

	now = R_SFXTime ();
	sfx->image = r_sfx_images[sprite] ? r_sfx_images[sprite] : r_notexture;
	{
		static int add_logged[MAX_SFX_SPRITES];

		if (add_logged[sprite] < 5)
		{
			add_logged[sprite]++;
			R_SFXLog ("add sprite %d flags 0x%x img %s alpha %.2f..%.2f life %.2f size %.1f lists_now %d\n",
				sprite, flags, sfx->image->name, start_alpha, end_alpha, lifetime, start_width, r_num_sfx);
		}
	}
	sfx->sprite = sprite;
	sfx->flags = flags;
	VectorCopy (origin, sfx->origin);
	VectorCopy (vel, sfx->velocity);
	if (flags & SFX_FLAG_SURFACE_EFFECT)
	{
		vec3_t angles;
		if (flags & SFX_FLAG_VEL_IS_ANGLES)
		{
			AngleVectors (vel, sfx->surface_right, sfx->surface_up, NULL);
			VectorClear (sfx->velocity);
		}
		else
		{
			vectoangles (vel, angles);
			AngleVectors (angles, NULL, sfx->surface_right, sfx->surface_up);
		}
	}

	if (lifetime == 0.01f)
	{
		float alpha = start_alpha > end_alpha ? start_alpha : end_alpha;
		start_alpha = end_alpha = alpha;
	}
	sfx->spawn_time = now;
	sfx->lifetime = lifetime;
	sfx->start_alpha = start_alpha;
	sfx->end_alpha = end_alpha;
	sfx->fade_time = fadein_time;
	sfx->start_width = start_width;
	sfx->end_width = end_width;
	sfx->start_height = start_height;
	sfx->end_height = end_height;
	sfx->flip_s = (flags & SFX_FLAG_NO_FLIP) ? 0 : rand () & 1;
	sfx->flip_t = 0;
	sfx->last_time = now;

	sfx->next = r_sfx_lists[sprite];
	if (sfx->next)
		sfx->next->prev = sfx;
	r_sfx_lists[sprite] = sfx;
	r_num_sfx++;
}

/*
===============
R_SetDeferredSFX
===============
*/
void R_SetDeferredSFX (int sprite, float alpha, vec3_t center,
        vec3_t axis0, vec3_t axis1, float half_width, float half_height)
{
    if (sprite < 0 || sprite >= MAX_SFX_SPRITES)
        return;

    r_deferred_sfx.valid = true;
    r_deferred_sfx.image = r_sfx_images[sprite] ? r_sfx_images[sprite] : r_notexture;
    r_deferred_sfx.alpha = Q_bound (0.0f, alpha, 1.0f);
    VectorCopy (center, r_deferred_sfx.center);
    VectorCopy (axis0, r_deferred_sfx.axis0);
    VectorCopy (axis1, r_deferred_sfx.axis1);
    r_deferred_sfx.half_width = half_width;
    r_deferred_sfx.half_height = half_height;

    /* Keep the weapon transform, including left-handed projection, until
     * the late pass. GUM matrices are copied here, never shared with GE. */
    sceGumMatrixMode (GU_PROJECTION);
    sceGumStoreMatrix (&r_deferred_sfx.projection);
    sceGumMatrixMode (GU_VIEW);
    sceGumStoreMatrix (&r_deferred_sfx.view);
    sceGumMatrixMode (GU_MODEL);
    sceGumStoreMatrix (&r_deferred_sfx.model);
}

void R_DrawDeferredSFX (void)
{
    vec3_t temp;
    gu_vert_ftv_t *out;
    int blend, alpha_test, cull, lighting;

    if (!r_deferred_sfx.valid)
        return;
    r_deferred_sfx.valid = false;

    blend = sceGuGetStatus (GU_BLEND);
    alpha_test = sceGuGetStatus (GU_ALPHA_TEST);
    cull = sceGuGetStatus (GU_CULL_FACE);
    lighting = sceGuGetStatus (GU_LIGHTING);

    sceGumMatrixMode (GU_PROJECTION);
    sceGumPushMatrix ();
    sceGumLoadMatrix (&r_deferred_sfx.projection);
    sceGumMatrixMode (GU_VIEW);
    sceGumPushMatrix ();
    sceGumLoadMatrix (&r_deferred_sfx.view);
    sceGumMatrixMode (GU_MODEL);
    sceGumPushMatrix ();
    sceGumLoadMatrix (&r_deferred_sfx.model);

    /* Per-draw list memory remains alive until GE consumes this frame. */
    out = (gu_vert_ftv_t *)sceGuGetMemory (sizeof(*out) * 4);
    VectorMA (r_deferred_sfx.center, -r_deferred_sfx.half_width,
        r_deferred_sfx.axis0, temp);
    VectorMA (temp, -r_deferred_sfx.half_height, r_deferred_sfx.axis1, out[0].xyz);
    VectorMA (temp, r_deferred_sfx.half_height, r_deferred_sfx.axis1, out[3].xyz);
    VectorMA (r_deferred_sfx.center, r_deferred_sfx.half_width,
        r_deferred_sfx.axis0, temp);
    VectorMA (temp, -r_deferred_sfx.half_height, r_deferred_sfx.axis1, out[1].xyz);
    VectorMA (temp, r_deferred_sfx.half_height, r_deferred_sfx.axis1, out[2].xyz);
    out[0].u = 0; out[0].v = 1;
    out[1].u = 1; out[1].v = 1;
    out[2].u = 1; out[2].v = 0;
    out[3].u = 0; out[3].v = 0;

    GL_Bind (r_deferred_sfx.image);
    sceGuDisable (GU_CULL_FACE);
    sceGuDisable (GU_ALPHA_TEST);
    sceGuDisable (GU_LIGHTING);
    sceGuDepthMask (GU_TRUE);
    sceGuEnable (GU_BLEND);
    GL_TexEnv (GU_TFX_MODULATE);
    sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuColor (GU_HCOLOR_4F (1, 1, 1, r_deferred_sfx.alpha));
    sceGumDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
        4, 0, out);

    sceGumPopMatrix ();
    sceGumMatrixMode (GU_VIEW);
    sceGumPopMatrix ();
    sceGumMatrixMode (GU_PROJECTION);
    sceGumPopMatrix ();
    sceGumMatrixMode (GU_MODEL);
    sceGumUpdateMatrix ();

    sceGuSetStatus (GU_CULL_FACE, cull);
    sceGuSetStatus (GU_ALPHA_TEST, alpha_test);
    sceGuSetStatus (GU_LIGHTING, lighting);
    sceGuSetStatus (GU_BLEND, blend);
    sceGuDepthMask (GU_FALSE);
    GL_TexEnv (GU_TFX_REPLACE);
    sceGuColor (GU_HCOLOR_DEFAULT);
}
