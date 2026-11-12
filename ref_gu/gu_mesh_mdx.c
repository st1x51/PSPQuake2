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

  MDX MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

extern vec3_t	lightspot;
extern float	r_avertexnormals[NUMVERTEXNORMALS][3];

static vec4_t	s_lerped[MAX_VERTS] __attribute__((aligned(16)));
static vec4_t	shadelight;

static float mdx_map_texcoords[MAX_VERTS][2];

static void Mdx_DrawBoundingBoxes (const dmdx_t *header, int frame)
{
    static const byte faces[6][4] = {
        {0, 1, 5, 4}, {2, 0, 4, 6}, {3, 2, 6, 7},
        {1, 3, 7, 5}, {4, 5, 7, 6}, {0, 2, 3, 1}
    };
    int object, face, i, axis, blend, lighting;
    if (!r_showbbox->value || header->num_subobjects <= 0 ||
        frame < 0 || frame >= header->num_frames)
        return;

    blend = sceGuGetStatus (GU_BLEND);
    lighting = sceGuGetStatus (GU_LIGHTING);
    GL_Bind (r_notexture);
    sceGuDisable (GU_LIGHTING);
    sceGuEnable (GU_BLEND);
    sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuColor (GU_HCOLOR_4F (0.5f, 0.5f, 0.5f, 0.3f));
    for (object = 0; object < header->num_subobjects; object++)
    {
        const float *bounds = (const float *)((const byte *)header + header->ofs_bbox_frames) +
            (object * header->num_frames + frame) * 6;
        for (face = 0; face < 6; face++)
        {
            gu_vert_ftv_t *out = (gu_vert_ftv_t *)sceGuGetMemory (sizeof(*out) * 4);
            for (i = 0; i < 4; i++)
            {
                out[i].u = out[i].v = 0.4f;
                for (axis = 0; axis < 3; axis++)
                    out[i].xyz[axis] = bounds[axis + ((faces[face][i] & (1 << axis)) ? 3 : 0)];
            }
            sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 4, 0, out);
        }
    }
    sceGuSetStatus (GU_LIGHTING, lighting);
    sceGuSetStatus (GU_BLEND, blend);
    sceGuColor (GU_HCOLOR_DEFAULT);
}

/* Retail reflects the camera ray about the interpolated normal and projects
 * it in world space. GU environment mapping cannot express that operation. */
void R_AliasMapTexCoords (int count, const dtrivertx_t *verts,
        const dtrivertx_t *oldverts, float backlerp, const vec4_t *positions,
        float (*texcoords)[2])
{
    vec3_t forward, right, up, delta, camera;
    int v, i;

    AngleVectors (currententity->angles, forward, right, up);
    VectorSubtract (r_origin, currententity->origin, delta);
    camera[0] = DotProduct (delta, forward);
    camera[1] = -DotProduct (delta, right);
    camera[2] = DotProduct (delta, up);
    for (v = 0; v < count; v++)
    {
        vec3_t normal, direction, reflected, world;
        float dot;
        for (i = 0; i < 3; i++)
            normal[i] = r_avertexnormals[verts[v].lightnormalindex][i] * (1.0f - backlerp) +
                r_avertexnormals[oldverts[v].lightnormalindex][i] * backlerp;
        VectorNormalize (normal);
        VectorSubtract (positions[v], camera, direction);
        VectorNormalize (direction);
        dot = DotProduct (direction, normal);
        VectorMA (direction, -2.0f * dot, normal, reflected);
        VectorNormalize (reflected);
        for (i = 0; i < 3; i++)
            world[i] = forward[i] * reflected[0] - right[i] * reflected[1] + up[i] * reflected[2];
        VectorNormalize (world);
        texcoords[v][0] = 0.5f + world[1] * 0.5f;
        texcoords[v][1] = 0.5f - world[2] * 0.5f;
    }
}

static void Mdx_MapTexCoords (int count, const dtrivertx_t *verts,
        const dtrivertx_t *oldverts, float backlerp)
{
    R_AliasMapTexCoords (count, verts, oldverts, backlerp, s_lerped, mdx_map_texcoords);
}

/* Authored model lights plus dynamic lights, as in the Kingpin renderer.
 * Software vertex colours retain all twelve lights without the GU four-light
 * limit; ordinary Q2-style lighting continues to use the existing GU path. */
#define MDX_MAX_LIGHTS 12
typedef struct
{
    vec3_t origin, direction, color;
} mdx_light_t;
static mdx_light_t mdx_lights[MDX_MAX_LIGHTS];
static int mdx_num_lights;
static qboolean mdx_directional;
static unsigned int mdx_vertex_colors[MAX_VERTS];

static void Mdx_AddLight (const vec3_t origin, const vec3_t color, float strength)
{
    vec3_t delta, forward, right, up;
    mdx_light_t *light;
    if (mdx_num_lights >= MDX_MAX_LIGHTS || strength <= 0.0f)
        return;
    light = &mdx_lights[mdx_num_lights++];
    VectorCopy (origin, light->origin);
    VectorSubtract (origin, currententity->origin, delta);
    if (VectorNormalize (delta) == 0.0f)
        VectorSet (delta, 0, 0, 1);
    AngleVectors (currententity->angles, forward, right, up);
    light->direction[0] = DotProduct (delta, forward);
    light->direction[1] = -DotProduct (delta, right);
    light->direction[2] = DotProduct (delta, up);
    VectorNormalize (light->direction);
    VectorScale (color, strength, light->color);
}

static void Mdx_SetupLights (void)
{
    model_lighting_t *lighting = &currententity->model_lighting;
    vec3_t delta, color;
    float distance, strength, dynamic_scale = 0.0f;
    int i, count;

    mdx_num_lights = 0;
    mdx_directional = r_directional_lighting->value != 0.0f &&
        (currententity->flags2 & RF2_DIR_LIGHTS) &&
        !(currententity->flags & (RF_FULLBRIGHT | RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE));
    if (!mdx_directional)
        return;

    count = Q_bound (0, lighting->num_dir_lights, MAX_MODEL_DIR_LIGHTS);
    for (i = 0; i < count; i++)
    {
        VectorSubtract (lighting->light_orgs[i], currententity->origin, delta);
        distance = VectorLength (delta);
        strength = (lighting->light_intensities[i] - distance) / 255.0f;
        VectorCopy (lighting->light_colors[i], color);
        if (r_newrefdef.lightstyles)
        {
            const float *rgb = r_newrefdef.lightstyles[lighting->light_styles[i]].rgb;
            color[0] *= rgb[0]; color[1] *= rgb[1]; color[2] *= rgb[2];
        }
        Mdx_AddLight (lighting->light_orgs[i], color, strength);
    }
    for (i = 0; i < r_newrefdef.num_dlights && mdx_num_lights < MDX_MAX_LIGHTS; i++)
    {
        dlight_t *dl = &r_newrefdef.dlights[i];
        VectorSubtract (dl->origin, currententity->origin, delta);
        strength = (dl->intensity * 0.6f - VectorLength (delta)) / 256.0f;
        if (strength > 0.0f)
        {
            dynamic_scale += Q_max3 (dl->color[0], dl->color[1], dl->color[2]);
            strength *= dynamic_scale;
        }
        Mdx_AddLight (dl->origin, dl->color, strength);
    }
}

static void Mdx_VertexColors (int count, const dtrivertx_t *verts,
        const dtrivertx_t *oldverts, float backlerp, float alpha)
{
    int v, i, j;
    for (v = 0; v < count; v++)
    {
        vec3_t normal, color;
        for (i = 0; i < 3; i++)
            normal[i] = r_avertexnormals[verts[v].lightnormalindex][i] * (1.0f - backlerp) +
                r_avertexnormals[oldverts[v].lightnormalindex][i] * backlerp;
        VectorNormalize (normal);
        VectorSet (color, 0.05f, 0.05f, 0.05f);
        for (i = 0; i < mdx_num_lights; i++)
        {
            float dot = DotProduct (normal, mdx_lights[i].direction);
            if (dot <= 0.0f)
                continue;
            for (j = 0; j < 3; j++)
                color[j] = Q_min (1.0f, color[j] + dot * mdx_lights[i].color[j]);
        }
        mdx_vertex_colors[v] = GU_HCOLOR_C4F (color[0], color[1], color[2], alpha);
    }
}

static void Mdx_ShadowVector (vec3_t direction)
{
    vec3_t delta, forward, right, up;
    float nearest = 9999.0f;
    int i, best = -1;
    if (!mdx_directional)
        return;
    for (i = 0; i < mdx_num_lights; i++)
    {
        float distance;
        VectorSubtract (currententity->origin, mdx_lights[i].origin, delta);
        distance = VectorLength (delta);
        if (distance < nearest)
        {
            nearest = distance;
            best = i;
        }
    }
    if (best < 0)
        return;
    VectorSubtract (currententity->origin, mdx_lights[best].origin, delta);
    VectorNormalize (delta);
    if (delta[2] > -0.5f)
    {
        delta[2] = -0.5f;
        VectorNormalize (delta);
    }
    AngleVectors (currententity->angles, forward, right, up);
    direction[0] = -DotProduct (delta, forward);
    direction[1] = DotProduct (delta, right);
    direction[2] = -DotProduct (delta, up);
}

/* MD2 and MDX use the same Kingpin lighting rules. The scratch arrays are
 * copied into GU list memory by each caller before the next entity is drawn. */
qboolean R_AliasSetupDirectionalLights (void)
{
    Mdx_SetupLights ();
    if (mdx_directional)
        sceGuDisable (GU_LIGHTING);
    return mdx_directional;
}

const unsigned int *R_AliasDirectionalColors (int count, const dtrivertx_t *verts,
        const dtrivertx_t *oldverts, float backlerp, float alpha)
{
    Mdx_VertexColors (count, verts, oldverts, backlerp, alpha);
    return mdx_vertex_colors;
}

void R_AliasDirectionalShadow (vec3_t direction)
{
    Mdx_ShadowVector (direction);
}

static float	mdx_sfx_previous[5];
static float	mdx_sfx_frame_time = -1.0f;
static int		mdx_sfx_frame_mask;


#if 1
/*
=============
Mdx_SFXSizeRandomScale
=============
*/
static float Mdx_SFXSizeRandomScale (float variance)
{
	/* FUN_100084b0 damps only the authored size variance by 0.3. */
	return 1.0f + variance * 0.3f * (2.0f * rand () / RAND_MAX - 1.0f);
}

/*
=============
Mdx_SFXTimeRandomScale
=============
*/
static float Mdx_SFXTimeRandomScale (float variance)
{
	/* Lifetime, fade time and the special 99..109 weapon flashes use the
	 * complete authored time-variance range. */
	return 1.0f + variance * (2.0f * rand () / RAND_MAX - 1.0f);
}

/*
=============
R_UpdateMdxSFXIntervalMask
=============
*/
void R_UpdateMdxSFXIntervalMask (void)
{
	static const float intervals[5] = { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f };
	float now = R_SFXTime ();
	int i;

	if (mdx_sfx_frame_time == now)
		return;
	mdx_sfx_frame_time = now;
	mdx_sfx_frame_mask = 0;
	for (i = 0; i < 5; i++)
	{
		if ((float)fabs (now - mdx_sfx_previous[i]) >= intervals[i])
		{
			mdx_sfx_frame_mask |= 1 << i;
			mdx_sfx_previous[i] = now;
		}
	}
}

/*
=============
Mdx_SFXIntervalMask
=============
*/
static int Mdx_SFXIntervalMask (void)
{
	R_UpdateMdxSFXIntervalMask ();
	return mdx_sfx_frame_mask;
}

/*
=============
Mdx_TransformPoint
=============
*/
static void Mdx_TransformPoint (const vec3_t point, vec3_t result)
{
	float pitch = -currententity->angles[PITCH] * (M_PI / 180.0f);
	float yaw = currententity->angles[YAW] * (M_PI / 180.0f);
	float roll = -currententity->angles[ROLL] * (M_PI / 180.0f);
	float sp = (float)sin (pitch), cp = (float)cos (pitch);
	float sy = (float)sin (yaw), cy = (float)cos (yaw);
	float sr = (float)sin (roll), cr = (float)cos (roll);

	/* Exact 3x3 matrix built by retail FUN_100080a0. */
	result[0] = (cr * cy + sr * sp * sy) * point[0] +
		(sr * sp * cy - cr * sy) * point[1] + sr * cp * point[2] +
		currententity->origin[0];
	result[1] = cp * sy * point[0] + cp * cy * point[1] - sp * point[2] +
		currententity->origin[1];
	result[2] = (cr * sp * sy - sr * cy) * point[0] +
		(cr * sp * cy - sr * sy) * point[1] + cr * cp * point[2] +
		currententity->origin[2];
}

/*
=============
Mdx_TransformVector
=============
*/
static void Mdx_TransformVector (const vec3_t vector, vec3_t result)
{
	vec3_t point;

	Mdx_TransformPoint (vector, point);
	VectorSubtract (point, currententity->origin, result);
}

/*
=============
Mdx_ProcessSFX

 Retail FUN_100084b0 stores types 99..109 in a single deferred slot.  These
 are weapon muzzle flashes, drawn after alpha surfaces with the modelview
 matrix which was active while the weapon itself was rendered.
=============
*/
static void Mdx_ProcessSFX (dmdx_t *header, const dtrivertx_t *frameverts)
{
	dmdx_sfxdef_t *defs;
	const dmdx_sfxentry_t *entries;
	const dtriangle_t *triangles;
	const int *vertexinfo;
	vec3_t angles = { 0, 0, 0 };
	vec3_t axis0, axis1;
	int interval_mask;
	int i;

	if ((r_newrefdef.rdflags & RDF_PAUSED) || r_sfx_pause->value ||
		header->num_sfxdefs <= 0 || header->num_sfxentries <= 0)
		return;

	defs = (dmdx_sfxdef_t *)((byte *)header + header->ofs_sfxdefs);
	entries = (const dmdx_sfxentry_t *)((const byte *)header + header->ofs_sfxentries);
	triangles = (const dtriangle_t *)((const byte *)header + header->ofs_tris);
	vertexinfo = (const int *)((const byte *)header + header->ofs_vertexinfo);
	AngleVectors (angles, NULL, axis1, axis0);
	interval_mask = Mdx_SFXIntervalMask ();

	for (i = 0; i < header->num_sfxentries; i++)
	{
		const dmdx_sfxentry_t *entry = &entries[i];
		dmdx_sfxdef_t *def;
		vec3_t anchor, direction_axis;
		int object_vertex;
		float half_width, half_height;
		int frame = currententity->frame;

		if (frame < 0 || frame >= MDX_SFX_FRAME_BYTES * 8 ||
			!(entry->frames[frame >> 3] & (0x80 >> (frame & 7))) ||
			entry->definition < 0 || entry->definition >= header->num_sfxdefs)
			continue;

		def = &defs[entry->definition];
		if (entry->anchor_mode == 0)
		{
			if (entry->anchor < 0 || entry->anchor >= header->num_xyz)
				continue;
			object_vertex = entry->anchor;
			VectorCopy (s_lerped[entry->anchor], anchor);
			if (def->direction_mode == 1)
				VectorCopy (r_avertexnormals[frameverts[entry->anchor].lightnormalindex],
					direction_axis);
			else
				VectorSet (direction_axis, 0, 0, 1);
		}
		else if (entry->anchor_mode == 1)
		{
			const dtriangle_t *triangle;
			vec3_t edge1, edge2;
			int a, b, c;

			if (entry->anchor < 0 || entry->anchor >= header->num_tris)
				continue;
			triangle = &triangles[entry->anchor];
			a = triangle->index_xyz[0];
			b = triangle->index_xyz[1];
			c = triangle->index_xyz[2];
			if (a < 0 || a >= header->num_xyz || b < 0 || b >= header->num_xyz ||
				c < 0 || c >= header->num_xyz)
				continue;
			object_vertex = a;
			VectorAdd (s_lerped[a], s_lerped[b], anchor);
			VectorAdd (anchor, s_lerped[c], anchor);
			VectorScale (anchor, 1.0f / 3.0f, anchor);
			if (def->direction_mode == 1)
			{
				VectorSubtract (s_lerped[b], s_lerped[a], edge1);
				VectorSubtract (s_lerped[c], s_lerped[a], edge2);
				CrossProduct (edge1, edge2, direction_axis);
				VectorNormalize (direction_axis);
				VectorScale (direction_axis, -1, direction_axis);
			}
			else
				VectorSet (direction_axis, 0, 0, 1);
		}
		else
			continue;

		if (vertexinfo[object_vertex] & currententity->invisible_objects)
			continue;

		if (entry->anchor_mode == 0 && def->sprite >= 99 && def->sprite <= 109)
		{
			if (def->sprite == SFX_SPRITE_MUZZLE2)
			{
				static const int tomgun_flashes[3] = { SFX_SPRITE_MUZZLE2, SFX_SPRITE_MUZZLE2_ALT, SFX_SPRITE_MUZZLE2B };
				def->sprite = tomgun_flashes[rand () % 3];
			}
			/* Retail feeds file height into its first quad axis and vice versa. */
			half_width = def->start_height * Mdx_SFXTimeRandomScale (def->time_variance);
			half_height = def->start_width * Mdx_SFXTimeRandomScale (def->time_variance);
			R_SetDeferredSFX (def->sprite, def->start_alpha,
				s_lerped[entry->anchor], axis0, axis1, half_width, half_height);
		}
		else
		{
			vec3_t origin, velocity, local_velocity;
			float lifetime, fade_time;
			float start_width, end_width, start_height, end_height;

			if (def->interval_mask && !(def->interval_mask & interval_mask))
				continue;
			if ((float)rand () / (RAND_MAX + 1.0f) >= def->chance)
				continue;

			if (!(def->flags & 1) && def->offset > 0.0f)
				VectorMA (anchor, def->offset, direction_axis, anchor);
			Mdx_TransformPoint (anchor, origin);
			VectorScale (direction_axis, def->direction, local_velocity);
			Mdx_TransformVector (local_velocity, velocity);

			lifetime = def->lifetime * Mdx_SFXTimeRandomScale (def->time_variance);
			fade_time = def->fade_time * Mdx_SFXTimeRandomScale (def->time_variance);
			start_width = def->start_width * Mdx_SFXSizeRandomScale (def->size_variance);
			end_width = def->end_width * Mdx_SFXSizeRandomScale (def->size_variance);
			start_height = def->start_height * Mdx_SFXSizeRandomScale (def->size_variance);
			end_height = def->end_height * Mdx_SFXSizeRandomScale (def->size_variance);
			R_AddSFX (def->sprite, def->flags, origin, velocity, (int)def->offset,
				lifetime, def->start_alpha, def->end_alpha, fade_time,
				start_width, end_width, start_height, end_height);
		}
	}
}
#else
#define Mdx_ProcessSFX(a, b)
#endif
/*
=============
Mdx_ObjectVisible
=============
*/
static qboolean Mdx_ObjectVisible (int object, int mapmode)
{
	image_t *skin;
	int skinnum;

	if (object >= 0 && object < MAX_MODELPART_OBJECTS)
	{
		if (currententity->invisible_objects & (1 << object))
			return false;
		/* FUN_10007980 changes per-object skins only in the ordinary skin
		 * pass.  Binding them during reflection/specular passes would replace
		 * the environment-map texture selected by R_DrawAliasModelMapped. */
		if (!mapmode && currententity->use_skinarray)
		{
			skinnum = currententity->skinarray[object];
			if (skinnum < MAX_MDXSKINS)
			{
				skin = currentmodel->skins[skinnum];
				if (skin)
					GL_Bind (skin);
			}
		}
	}
	return true;
}


#if 1
static inline void GL_MDXLerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] )
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

#ifdef KINGPIN_ENGINE
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
#else
	//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
#endif
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
GL_DrawMDXFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawMDXFrameLerp (dmdx_t *pmdxhdr, float backlerp, int mapmode)
{
	float 	l;
	daliasframe_t	*frame, *oldframe;
	dtrivertx_t		*v, *ov, *verts;
	int		*order;
	int		count, object;
	float	frontlerp;
	float	alpha;
	float	*norm;
	vec3_t	move, delta, vectors[3];
	vec3_t	frontv, backv;
	int		i;
	int		index_xyz;
	float	*lerp;
	int		prim;

	frame = (daliasframe_t *)((byte *)pmdxhdr + pmdxhdr->ofs_frames
		+ currententity->frame * pmdxhdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)pmdxhdr + pmdxhdr->ofs_frames
		+ currententity->oldframe * pmdxhdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)pmdxhdr + pmdxhdr->ofs_glcmds);

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

	GL_MDXLerpVerts( pmdxhdr->num_xyz, v, ov, verts, lerp, move, frontv, backv );

	if (mdx_directional)
		Mdx_VertexColors (pmdxhdr->num_xyz, verts, ov, backlerp, alpha);

	if (mapmode)
	{
		Mdx_MapTexCoords (pmdxhdr->num_xyz, verts, ov, backlerp);
	}
	else
	{
		Mdx_ProcessSFX ((dmdx_t *)pmdxhdr, verts);
	}

	if (currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ))
	{
		sceGuDisable (GU_TEXTURE_2D);
		sceGuColor (GU_HCOLOR_C4F (shadelight[0], shadelight[1], shadelight[2], alpha));

		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done

			object = *order++;
			if (!Mdx_ObjectVisible(object, mapmode))
			{
				order += (count < 0 ? -count : count) * 3;
				continue;
			}

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

			object = *order++;
			if (!Mdx_ObjectVisible(object, mapmode))
			{
				order += (count < 0 ? -count : count) * 3;
				continue;
			}

			if (count < 0)
			{
				count = -count;
				prim = GU_TRIANGLE_FAN;
			}
			else
			{
				prim = GU_TRIANGLE_STRIP;
			}

            if (mdx_directional)
            {
                gu_vert_ftcv_t *out = (gu_vert_ftcv_t *)sceGuGetMemory (sizeof(*out) * count);
                for (i = 0; i < count; i++, order += 3)
                {
                    index_xyz = order[2];
                    out[i].u = mapmode ? mdx_map_texcoords[index_xyz][0] : ((float *)order)[0];
                    out[i].v = mapmode ? mdx_map_texcoords[index_xyz][1] : ((float *)order)[1];
                    out[i].c = mdx_vertex_colors[index_xyz];
                    VectorCopy (s_lerped[index_xyz], out[i].xyz);
                }
                sceGuDrawArray (prim, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF,
                    count, 0, out);
                continue;
            }

			gu_vert_ftnv_t* const out = (gu_vert_ftnv_t*)sceGuGetMemory (sizeof(gu_vert_ftnv_t) * count);
			gu_vert_ftnv_t* out_ptr = out;
			for (i = 0; i < count; i++, order += 3, out_ptr++)
			{
				index_xyz = order[2];

				// texture coordinates come from the draw list
				out_ptr->u = mapmode ? mdx_map_texcoords[index_xyz][0] : ((float *)order)[0];
				out_ptr->v = mapmode ? mdx_map_texcoords[index_xyz][1] : ((float *)order)[1];

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
GL_DrawMDXShadow
=============
*/
void GL_DrawMDXShadow (dmdx_t *pmdxhdr, int posenum)
{
	dtrivertx_t	*verts;
	int		*order;
	vec3_t	shadevector;
	float	an, height, lheight;
	int		count, object;
	daliasframe_t	*frame;
	int		i;
	int		prim;
	int		index;

	an = currententity->angles[YAW] / 180 * M_PI;
	shadevector[0] = cos (-an);
	shadevector[1] = sin (-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);
	Mdx_ShadowVector (shadevector);

	lheight = currententity->origin[2] - lightspot[2];

	frame = (daliasframe_t *)((byte *)pmdxhdr + pmdxhdr->ofs_frames
		+ currententity->frame * pmdxhdr->framesize);
	verts = frame->verts;

	height = 0;

	order = (int *)((byte *)pmdxhdr + pmdxhdr->ofs_glcmds);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done

		object = *order++;
		if (!Mdx_ObjectVisible(object, 0))
		{
			order += (count < 0 ? -count : count) * 3;
			continue;
		}

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
R_CullMDXModel
=============
*/
static qboolean R_CullMDXModel (dmdx_t *pmdxhdr, vec3_t mins, vec3_t maxs, entity_t *e)
{
	int			i, clipstatus;
	vec3_t		vectors[3];
	float		thismm, oldmm;
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t		angles;

	if ((e->frame >= pmdxhdr->num_frames) || (e->frame < 0))
	{
		if ((e->flags2 & RF2_MODULUS_FRAME) &&
			e->frame >= 0 && pmdxhdr->num_frames > 0)
		{
			e->frame %= pmdxhdr->num_frames;
		}
		else
		{
			ri.Con_Printf (PRINT_ALL, "R_CullMDXModel %s: no such frame %d\n",
				currentmodel->name, e->frame);
			e->frame = 0;
		}
	}
	if ((e->oldframe >= pmdxhdr->num_frames) || (e->oldframe < 0))
	{
		if ((e->flags2 & RF2_MODULUS_FRAME) &&
			e->oldframe >= 0 && pmdxhdr->num_frames > 0)
		{
			e->oldframe %= pmdxhdr->num_frames;
		}
		else
		{
			ri.Con_Printf (PRINT_ALL, "R_CullMDXModel %s: no such oldframe %d\n",
				currentmodel->name, e->oldframe);
			e->oldframe = 0;
		}
	}

	pframe = (daliasframe_t *)((byte *)pmdxhdr + pmdxhdr->ofs_frames +
										e->frame * pmdxhdr->framesize);

	poldframe = (daliasframe_t *)((byte *)pmdxhdr + pmdxhdr->ofs_frames +
										e->oldframe * pmdxhdr->framesize);

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
R_SetupMDXLighting
=============
*/
static qboolean R_SetupMDXLighting (void)
{
	dlight_t	*best_dls[MAX_VLIGHTS];
	int			i, num_dls;
	float		r, g, b;

	Mdx_SetupLights ();

	ScePspFVector3	sun_dir = { -0.5f, 0.8f, 0.5f };
	qboolean		flat_light = currententity->flags & (  RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_FULLBRIGHT );


	if ( flat_light )
	{
		if ( currententity->flags & RF_FULLBRIGHT )
		{
			VectorSet (shadelight, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			VectorClear (shadelight);

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
		if (currententity->flags & RF_DIMLIGHT)
			VectorSet (shadelight, 0.8f, 0.8f, 0.8f);
		else if (currententity->flags2 & RF2_PASSLIGHT)
		{
			float passed = (currententity->flags2 & 0x1f) * (1.0f / 31.0f);
			VectorSet (shadelight, passed, passed, passed);
		}

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


	if (flat_light || mdx_directional)
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
R_DrawMDXModel

=================
*/
void R_DrawMDXModel (entity_t *e, int mapmode)
{
	int			i, flags;
	dmdx_t		*pmdxhdr;
	float		an;
	vec3_t		mins, maxs;
	image_t		*skin;
	qboolean	light_en;
	ScePspFVector3	scale;
	qboolean	mdx_blend = false;

	if (e->flags2 & RF2_PASSALPHA)
		e->flags |= RF_TRANSLUCENT;

	flags = e->flags;

	if (flags & RF_WEAPONMODEL)
	{
		if ( r_lefthand->value == 2 )
			return;
	}

	pmdxhdr = (dmdx_t *)Mod_Extradata (currentmodel);

	sceGumPushMatrix ();
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.
	R_RotateForEntity (e); // &proj update
	e->angles[PITCH] = -e->angles[PITCH];	// sigh.


	if (!(flags & RF_WEAPONMODEL))
	{
		if (R_CullMDXModel (pmdxhdr, mins, maxs, e))
		{
			sceGumPopMatrix ();
			sceGumUpdateMatrix ();
			return;
		}
	}

	//
	// get lighting information
	//
	light_en = R_SetupMDXLighting ();

	//
	// locate the proper data
	//

	c_alias_polys += pmdxhdr->num_tris;

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
	if (mapmode)
	{
		skin = (mapmode == 2) ? r_speculartexture : r_reflectiontexture;
	}
	else if (currententity->skin)
	{
		skin = currententity->skin;	// custom player skin
	}
	else
	{
		if (currententity->skinnum >= MAX_MDXSKINS)
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

	mdx_blend = (mapmode == 2 || (!mapmode && (flags & (RF_REFL_MAP | RF_TRANSLUCENT))));

	if (mdx_blend)
		sceGuEnable (GU_BLEND);

	if ((currententity->frame >= pmdxhdr->num_frames) || (currententity->frame < 0))
	{
		if ((currententity->flags2 & RF2_MODULUS_FRAME) &&
			currententity->frame >= 0 && pmdxhdr->num_frames > 0)
		{
			currententity->frame %= pmdxhdr->num_frames;
		}
		else
		{
			ri.Con_Printf (PRINT_ALL, "R_DrawMDXModel %s: no such frame %d\n",
				currentmodel->name, currententity->frame);
			currententity->frame = 0;
			currententity->oldframe = 0;
		}
	}

	if ((currententity->oldframe >= pmdxhdr->num_frames) || (currententity->oldframe < 0))
	{
		if ((currententity->flags2 & RF2_MODULUS_FRAME) &&
			currententity->oldframe >= 0 && pmdxhdr->num_frames > 0)
		{
			currententity->oldframe %= pmdxhdr->num_frames;
		}
		else
		{
			ri.Con_Printf (PRINT_ALL, "R_DrawMDXModel %s: no such oldframe %d\n",
				currentmodel->name, currententity->oldframe);
			currententity->frame = 0;
			currententity->oldframe = 0;
		}
	}

	if (!r_lerpmodels->value)
		currententity->backlerp = 0;
	GL_DrawMDXFrameLerp (pmdxhdr, currententity->backlerp, mapmode);
	Mdx_DrawBoundingBoxes (pmdxhdr, currententity->frame);

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

	if (mdx_blend)
		sceGuDisable (GU_BLEND);

	if (flags & RF_DEPTHHACK)
		sceGuDepthRange (gldepthmin, gldepthmax);

	if (light_en)
		sceGuDisable (GU_LIGHTING);
#if 1
	if (!mapmode && gl_shadows->value && !(flags & (RF_TRANSLUCENT | RF_WEAPONMODEL)) && !(currententity->flags2 & RF2_NOSHADOW))
	{
		sceGumPushMatrix ();
		R_RotateForEntity (e);
		sceGuDisable (GU_TEXTURE_2D);
		sceGuEnable (GU_BLEND);
		sceGuColor (GU_HCOLOR_4F(0.0, 0.0, 0.0, 0.5));
		GL_DrawMDXShadow (pmdxhdr, currententity->frame );
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


