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
// gu_light.c

#include "gu_local.h"

int	r_dlightframecount;

#define	DLIGHT_CUTOFF	64.0

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

// sin(((16 - i) / 16.0) * (M_PI * 2))
// cos(((16 - i) / 16.0) * (M_PI * 2))
static float dlighttab[17][2] = {
{ 0.000000,  1.000000},
{-0.382683,  0.923879},
{-0.707107,  0.707107},
{-0.923879,  0.382683},
{-1.000000,  0.000000},
{-0.923879, -0.382683},
{-0.707107, -0.707107},
{-0.382683, -0.923879},
{-0.000000, -1.000000},
{ 0.382683, -0.923879},
{ 0.707107, -0.707107},
{ 0.923879, -0.382683},
{ 1.000000, -0.000000},
{ 0.923879,  0.382683},
{ 0.707107,  0.707107},
{ 0.382683,  0.923879},
{ 0.000000,  1.000000},
};

void R_RenderDlight (dlight_t *light)
{
	int		i;
	uint	vertcount;

	gu_vert_fcv_t* const out = (gu_vert_fcv_t*)extGuGetAlignedMemory(16, sizeof(gu_vert_fcv_t) * 18);

	vertcount = 0;

	__asm__ volatile (
		".set		push\n"					// save assembler option
		".set		noreorder\n"			// suppress reordering
		"lv.s		S013,  0(%1)\n"			// S013 = light->intensity
		"lv.s		S010,  0(%2)\n"			// S010 = light->color[0]
		"lv.s		S011,  4(%2)\n"			// S011 = light->color[1]
		"lv.s		S012,  8(%2)\n"			// S012 = light->color[2]
		"lv.s		S100,  0(%3)\n"			// S100 = light->origin[0]
		"lv.s		S110,  4(%3)\n"			// S110 = light->origin[1]
		"lv.s		S120,  8(%3)\n"			// S120 = light->origin[2]
		"lv.s		S101,  0(%4)\n"			// S101 = vright[0]
		"lv.s		S111,  4(%4)\n"			// S111 = vright[1]
		"lv.s		S121,  8(%4)\n"			// S121 = vright[2]
		"lv.s		S102,  0(%5)\n"			// S102 = vup[0]
		"lv.s		S112,  4(%5)\n"			// S112 = vup[1]
		"lv.s		S122,  8(%5)\n"			// S122 = vup[2]
		"lv.s		S103,  0(%6)\n"			// S103 = vpn[0]
		"lv.s		S113,  4(%6)\n"			// S113 = vpn[1]
		"lv.s		S123,  8(%6)\n"			// S123 = vpn[2]
		"vfim.s		S000, 0.2\n"			// S000 = 0.2
		"vfim.s		S001, 0.35\n"			// S001 = 0.35
		"vmul.q		C000, C000[X,X,X,Y], C010\n" // C000 = [light->color[*] * 0.2, light->intensity * 0.35]
		"vmscl.t	E101, E101, S003\n"		// E101 = [vright, vup, vpn] *= light->intensity * 0.35
		"viim.s		S003, 255\n"			// S003 = 255
		"vsat0.t	C000, C000\n"			// C000 = saturation to [0:1](c000)
		"vscl.t		C000, C000, S003\n"		// C000 = C000 * 255
		"vf2iz.q	C000, C000, 23\n"		// C000 = (int)C000 * 2^23
		"vi2uc.q	S000, C000\n"			// S000 = ((S003>>23)<<24) | ((S002>>23)<<16) | ((S001>>23)<<8) | (S000>>23)
		"vsub.t		C001, R100, R103\n"		// C001 = light->origin[i] - vpn[i]*rad;
		"sv.q		C000, %0\n"				// out[vertcount] = C000
		"li			$t0, 0xff000000\n"		// t0 = 0xff000000 (set alpha color)
		"mtv		$t0, S000\n"			// S010 = $t0
		".set		pop\n"					// restore assembler option
		:	"=m"(out[vertcount++])
		:	"r"(&light->intensity), "r"(light->color), "r"(light->origin), "r"(vright), "r"(vup), "r"(vpn)
		:	"$t0", "memory"
	);

	// The compiler will apply loop unrolling optimization here
	for (i = 0; i < 17; i++)
	{
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"lv.s		S010,  0(%1)\n"			// S010 = dlighttab[i][0]
			"lv.s		S011,  4(%1)\n"			// S011 = dlighttab[i][1]
			"vhdp.t		S001, C010, C100[Z, Y, X]\n"	//S001 = light->origin[0] + vright[0] * dlighttab[i][1] * rad + vup[0] * dlighttab[i][0] * rad;
			"vhdp.t		S002, C010, C110[Z, Y, X]\n"	//S002 = light->origin[1] + vright[1] * dlighttab[i][1] * rad + vup[1] * dlighttab[i][0] * rad;
			"vhdp.t		S003, C010, C120[Z, Y, X]\n"	//S003 = light->origin[2] + vright[2] * dlighttab[i][1] * rad + vup[2] * dlighttab[i][0] * rad;
			"sv.q		C000, %0\n"
			".set		pop\n"					// restore assembler option
			:	"=m"(out[vertcount++])
			:	"r"(dlighttab[i])
			:	"memory"
		);
	}
	sceGuDrawArray (GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF, vertcount, 0, out);
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	if (!gl_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	sceGuDepthMask (GU_TRUE);
	sceGuDisable (GU_TEXTURE_2D);
	sceGuShadeModel (GU_SMOOTH);
	sceGuEnable (GU_BLEND);
	sceGuBlendFunc (GU_ADD, GU_FIX, GU_FIX, GU_HBLEND_ONE, GU_HBLEND_ONE);

	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_RenderDlight (l);

	sceGuColor (GU_HCOLOR_DEFAULT);
	sceGuDisable (GU_BLEND);
	sceGuEnable (GU_TEXTURE_2D);
	sceGuShadeModel (GU_FLAT);
	sceGuBlendFunc (GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDepthMask (GU_FALSE);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	while(1)
	{
		if (node->contents != -1)
			return;

		splitplane = node->plane;
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;

		if (dist > light->intensity-DLIGHT_CUTOFF)
		{
			node = node->children[0];
			continue;
		}
		if (dist < -light->intensity+DLIGHT_CUTOFF)
		{
			node = node->children[1];
			continue;
		}

		// mark the polygons
		surf = r_worldmodel->surfaces + node->firstsurface;
		for (i=0 ; i<node->numsurfaces ; i++, surf++)
		{
			if (surf->dlightframe != r_dlightframecount)
			{
				surf->dlightbits = 0;
				surf->dlightframe = r_dlightframecount;
			}
			surf->dlightbits |= bit;
		}

		R_MarkLights (light, bit, node->children[0]);
		node = node->children[1];
		continue;
	}
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	if (gl_flashblend->value > 1)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame
	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_MarkLights ( l, 1<<i, r_worldmodel->nodes );
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t			pointcolor;
cplane_t		*lightplane;		// used as shadow plane
vec3_t			lightspot;

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	int			side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	int			maps;
	int			r;
	float		modulate;

	if (node->contents != -1)
		return -1;		// didn't hit anything

// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;

// go down front side
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something

	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing

// check for impact on this node
	VectorCopy (mid, lightspot);
	lightplane = plane;
	modulate = gl_modulate->value;

	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags&(SURF_DRAWTURB|SURF_DRAWSKY))
			continue;	// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorCopy (vec3_origin, pointcolor);
		if (lightmap)
		{
			vec3_t scale;

			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				for (i=0 ; i<3 ; i++)
					scale[i] = modulate * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

				pointcolor[0] += lightmap[0] * scale[0] * (1.0 / 255);
				pointcolor[1] += lightmap[1] * scale[1] * (1.0 / 255);
				pointcolor[2] += lightmap[2] * scale[2] * (1.0 / 255);
				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}
		}

		return 1;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t		end;
	float		r;
	int			lnum;
	dlight_t	*dl;
	float		light;
	vec3_t		dist;
	float		add;

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (r_worldmodel->nodes, p, end);

	if (r == -1)
	{
		VectorCopy (vec3_origin, color);
	}
	else
	{
		VectorCopy (pointcolor, color);
	}

	//
	// add dynamic lights
	//
	light = 0;
	dl = r_newrefdef.dlights;
	for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++, dl++)
	{
		VectorSubtract (currententity->origin,
						dl->origin,
						dist);
		add = dl->intensity - VectorLength(dist);
		add *= (1.0/256);
		if (add > 0)
		{
			VectorMA (color, add, dl->color, color);
		}
	}

	VectorScale (color, gl_modulate->value, color);
}

/*
===============
R_GetWorldLight
===============
*/
void R_GetWorldLight (vec3_t p, vec3_t color)
{
	vec3_t		end;
	float		r;

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	r = RecursiveLightPoint (r_worldmodel->nodes, p, end);

	if (r == -1)
	{
		VectorCopy (vec3_origin, color);
	}
	else
	{
		VectorCopy (pointcolor, color);
	}

	VectorScale (color, gl_modulate->value, color);
}

/*
===============
R_GetBestDynamicLights
===============
*/
void R_GetBestDynamicLights (float *origin, dlight_t **lights, int *count)
{
	int			lnum, i, k;
	float		minstrength, weakest, dist, r, f;
	float		lstrength[MAX_VLIGHTS];
	dlight_t	*el;
	vec3_t		mid;

	*count = 0;
	for (i = 0; i < MAX_VLIGHTS; i++)
	{
		lstrength[i] = 0.0f;
		lights[i] = NULL;
	}

	el = r_newrefdef.dlights;
	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, el++)
	{
		if (el->intensity <= 0)
			continue;

		VectorSubtract (origin, el->origin, mid);
		dist = DotProduct (mid, mid);
		r = el->intensity * el->intensity;

		f = (dist > r) ? (r / dist) : 1.0f;

		if (f > 0.05f)
		{
			k = -1;
			if (*count < MAX_VLIGHTS)
			{
				k = *count;
				(*count)++;
			}
			else
			{
				weakest = f;
				for (i = 0; i < MAX_VLIGHTS; i++)
				{
					if (lstrength[i] < weakest)
					{
						weakest = lstrength[i];
						k = i;
					}
				}
			}

			if (k != -1)
			{
				lights[k] = el;
				lstrength[k] = f;
			}
		}
	}
}

//===================================================================

static float s_blocklights[34*34*4] __attribute__((aligned(16)));
/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			s, t;
	int			smax, tmax;
	mtexinfo_t	*tex;
	dlight_t	*dl;
	float		*fBL, *pfBL;
	int			dlightbits;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;
	dlightbits = surf->dlightbits;

	fBL = s_blocklights;

	__asm__ volatile (
		".set		push\n"							// save assembler option
		".set		noreorder\n"					// suppress reordering
		"ulv.q		C000,  0(%[vecs])\n"			// C000 = tex->vecs[0]
		"ulv.q		C010, 16(%[vecs])\n"			// C010 = tex->vecs[1]
		"lh			$8,    0(%[texturemins])\n"		// $8 = surf->texturemins[0]
		"lh			$9,    2(%[texturemins])\n"		// $9 = surf->texturemins[1]
		"vfim.s		S030, "M_ATOS(DLIGHT_CUTOFF)"\n"// S030 = fminlight
		"mtv		$8,   S100\n"					// S100 = raw(surf->texturemins[0])
		"mtv		$9,   S101\n"					// S101 = raw(surf->texturemins[1])
		"vfim.s		S031, 16.0\n"					// S031 = 16.0
		"vi2f.p		C100, C100, 0\n"				// C100 = halfword(C100)
		"ulv.q		C020,  0(%[normal])\n"			// C020 = normal & dist
		"vsub.p		R003, R003, C100\n"				// S003 = tex->vecs[0][3] - surf->texturemins[0], S013 = tex->vecs[1][3] - surf->texturemins[1]
		"vneg.s		S023, S023\n"					// S023 = -surf->plane->dist
		".set		pop\n"							// restore assembler option
		::	[vecs] "r" (tex->vecs),
			[texturemins] "r"(surf->texturemins),
			[normal] "r" (surf->plane->normal)
		:	"memory", "$8", "$9"
	);

	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if ( !(dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		dl = &r_newrefdef.dlights[lnum];

		__asm__ volatile goto (
			".set		push\n"						// save assembler option
			".set		noreorder\n"				// suppress reordering
			"ulv.q		C100, 0(%[origin])\n"		// C100 = dl->origin
			"ulv.q		C110,  0(%[color])\n"		// C110 = dl->color & intensity
			"vhdp.q		S103, C100, C020\n"			// S103(fdist) = DotProduct (dl->origin, surf->plane->normal) - surf->plane->dist;
			"vsub.s		S120, S113, S103[|X|]\n"	// S120(frad) = S113(dl->intensity) - fabs(S103(fdist));
			"vcmp.s		LT, S120, S030\n"			// if (S120(frad) < S030(fminlight))
			"bvt		0, %l[skip_light]\n"		// goto skip_light
			"vsub.s		S121, S120, S030\n"			// S121(fminlight) = S120(frad) - S030(fminlight) (delay slot)
			"vscl.t		C200, C020, S103\n"			// C200(impact) = C020(surf->plane->normal[i]) * S103(fdist)
			"vsub.t		C200, C100, C200\n"			// C200(impact) = dl->origin[i] - surf->plane->normal[i] * fdist
			"vhdp.q		S122, C200, C000\n"			// S122(flocs) = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0]
			"vhdp.q		S123, C200, C010\n"			// S123(floct) = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1]
			"vzero.s	S033\n"						// S033(ftacc) = 0
			".set		pop\n"						// restore assembler option
			::	[origin] "r" (dl->origin),
				[color] "r" (dl->color)
			:	"cc", "memory"
			: skip_light
		);

		pfBL = fBL;
		for (t = 0; t < tmax; t++)
		{
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				"vsub.s		S130, S123, S033\n"		// S130(td) = S123(floct) - S033(ftacc)
				"vadd.s		S033, S033, S031\n"		// S033(ftacc) += 16
				"vabs.s		S130, S130\n"			// S130(td) = abs(td)
				"vzero.s	S032\n"					// S032(fsacc) = 0
				".set		pop\n"					// restore assembler option
			);

			for (s = 0; s < smax; s++, pfBL += 4)
			{
				__asm__ volatile (
					".set		push\n"					// save assembler option
					".set		noreorder\n"			// suppress reordering
					"vsub.s		S131, S122, S032\n"		// S131(sd) = S122(flocs) - S032(fsacc)
					"vabs.s		S131, S131\n"			// S131(sd) = abs(sd)
					"vmin.s		S100, S130, S131\n"		// S100 = S130/S131(min)
					"vmax.s		S101, S130, S131\n" 	// S101 = S130/S131(max)
					"vmul.s		S100, S100, S100[1/2]\n"// S100 = (min >> 1);
					"lv.q		C200,  0(%0)\n"			// C200 = pfBL (early load to avoid stalling the pipeline)
					"vadd.s		S101, S100, S101\n"		// S101(fdist_pix) = S100(min >> 1) + S101(max)
					"vcmp.s		LT, S101, S121\n"		// if (S101(fdist_pix) < S121(fminlight))
					"vadd.s		S032, S032, S031\n"		// S032(fsacc) += 16
					"bvf		0, 0f\n"				// if (S101(fdist_pix) < S121(fminlight)) goto skip_pix
					"vsub.s		S102, S120, S101\n"		// S102(intensity_pix) = S120(frad) - S101(fdist_pix) (delay slot)
					"vscl.t		C210, C110, S102\n"		// C210 = C110(dl->color) * S102(intensity_pix)
					"vadd.t		C200, C200, C210\n"     // pfBL + color
					"sv.q		C200,  0(%0)\n"			// pfBL = C130
				"0:"									// skip_pix label
					".set		pop\n"					// restore assembler option
					:: "r" (pfBL)
					: "memory"
				);
			}
		}
skip_light:
		continue;
	}
}


/*
** R_SetCacheState
*/
void R_SetCacheState( msurface_t *surf )
{
	int maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			r, g, b, a, max;
	int			i, j, size;
	byte		*lightmap, *gammatable;
	float		scale[4];
	int			maps;
	int			nummaps, numblocks;
	float		*bl;
	int			monolightmap;
	float		modulate;
	lightstyle_t	*lightstyles;
	byte			*surfstyles;

	if ( surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) )
		ri.Sys_Error (ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;

	if (size > (sizeof(s_blocklights)>>4) )
		ri.Sys_Error (ERR_DROP, "Bad s_blocklights size");

	// set to full bright if no light data
	if (!surf->samples)
	{
		for (i = 0; i < size * 4; i++)
			s_blocklights[i] = 255;
		goto store;
	}

	lightmap = surf->samples;
	modulate = gl_modulate->value;
	lightstyles = r_newrefdef.lightstyles;
	surfstyles = surf->styles;

	bl = s_blocklights;

	if (surfstyles[0] == 255)
	{
		numblocks = (size + 3) >> 2;	// round up to 4 pixels

		// fast memset
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"vzero.q	C000\n"
			"vzero.q	C010\n"
			"vzero.q	C020\n"
			"vzero.q	C030\n"
		"1:\n"
			"addiu		%[numblocks], %[numblocks], -1\n"
			"sv.q		C000,  0(%[bl])\n"
			"sv.q		C010, 16(%[bl])\n"
			"sv.q		C020, 32(%[bl])\n"
			"sv.q		C030, 48(%[bl])\n"
			"bne		%[numblocks], $0, 1b\n"
			"addiu		%[bl], %[bl], %[blocksize]\n"		// bl += blocksize (delay slot)
			".set		pop\n"					// restore assembler option
			:	[bl] "+r" (bl),
				[numblocks] "+r" (numblocks)
			:	[blocksize] "i" (sizeof (s_blocklights[0]) * 4 * 4)
			:	"memory"
		);
	}
	else
	{
		__asm__ volatile (
			".set		push\n"					// save assembler option
			".set		noreorder\n"			// suppress reordering
			"mtv		%[modulate], S100\n"	// S100 = modulate
			"lv.s		S010,  0(%[rgb])\n"		// R
			"lv.s		S011,  4(%[rgb])\n"		// G
			"lv.s		S012,  8(%[rgb])\n"		// B
			"vscl.t		C000, C010, S100\n"
			"lwr		$8,    0(%[lightmap])\n"	// preloading
			"lwl		$8,    3(%[lightmap])\n"	//
			"mtv		$8,   S010\n"
			"vuc2ifs.s	C020, S010\n"			// C020 = S010
			"vi2f.q		C010, C020, 23\n"		// C020 = (float)C020 / (1<<31)
			".set		pop\n"					// restore assembler option
			::	[modulate] "r" (modulate),
				[rgb] "r" (lightstyles[surfstyles[0]].rgb),
				[lightmap] "r" (lightmap)
			:	"memory", "$8"
		);

		for (i = 0; i < size; i++, bl += 4, lightmap += 3)
		{
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				"vmul.t		C030, C010, C000\n"
				"lwr		$8,    3(%[lightmap])\n"
				"lwl		$8,    6(%[lightmap])\n"
				"mtv		$8,   S010\n"
				"sv.q		C030,  0(%[bl])\n"
				"vuc2ifs.s	C020, S010\n"			// C020 = S010
				"vi2f.q		C010, C020, 23\n"		// C010 = (float)C020 / (1<<31)
				".set		pop\n"					// restore assembler option
				::	[bl] "r" (bl),
					[lightmap] "r" (lightmap)
				:	"memory", "$8"
			);
		}

		for (maps = 1; maps < MAXLIGHTMAPS && surfstyles[maps] != 255; maps++)
		{
			bl = s_blocklights;

			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				//"lbu		$zero, 64(%[lightmap])\n"	// precache
				"lv.s		S010,  0(%[rgb])\n"		// R
				"lv.s		S011,  4(%[rgb])\n"		// G
				"lv.s		S012,  8(%[rgb])\n"		// B
				"vscl.t		C000, C010, S100\n"
				"lwr		$8,    0(%[lightmap])\n"	// preloading
				"lwl		$8,    3(%[lightmap])\n"	//
				"mtv		$8,   S101\n"
				"vuc2ifs.s	C010, S101\n"			// C010 = S101
				"vi2f.q		C020, C010, 23\n"		// C020 = (float)C010 / (1<<31)
				"lv.q		C030,  0(%[bl])\n"		// C030 = bl
				".set		pop\n"					// restore assembler option
				::	[modulate] "r" (modulate),
					[rgb] "r" (lightstyles[surfstyles[maps]].rgb),
					[lightmap] "r" (lightmap),
					[bl] "r" (bl)
				:	"memory", "$8"
			);

			for (i = 0; i < size; i++, bl += 4, lightmap += 3)
			{
				__asm__ volatile (
					".set		push\n"					// save assembler option
					".set		noreorder\n"			// suppress reordering
					"vmul.t		C020, C020, C000\n"
					//"lbu		$zero, 64(%[lightmap])\n"	// precache
					"lwr		$8,    3(%[lightmap])\n"
					"lwl		$8,    6(%[lightmap])\n"
					"vadd.t		C030, C030, C020\n"
					"mtv		$8,   S101\n"
					"sv.q		C030,  0(%[bl])\n"
					"vuc2ifs.s	C010, S101\n"			// C010 = S101
					"vi2f.q		C020, C010, 23\n"		// C020 = (float)C010 / (1<<31)
					"lv.q		C030,  16(%[bl])\n"
					".set		pop\n"					// restore assembler option
					::	[bl] "r" (bl),
						[lightmap] "r" (lightmap)
					:	"memory", "$8"
				);
			}
		}
	}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// put into texture format
store:
	stride -= (smax<<2);
	bl = s_blocklights;

	monolightmap = gl_monolightmap->string[0];
	gammatable = gl_state.gammatable;

	__asm__ volatile (
		".set		push\n"					// save assembler option
		".set		noreorder\n"			// suppress reordering
		"viim.s		S100, 255\n"			// S100 = 255
		"vzero.q	C110\n"					// C110 = zero
		".set		pop\n"					// restore assembler option
	);

	if ( monolightmap == '0' )
	{
		for (i = 0; i < tmax; i++, dest += stride)
		{
			if (smax > 0)
			{
				__asm__ volatile (
					".set		push\n"					// save assembler option
					".set		noreorder\n"			// suppress reordering
					"lv.q		C200, 0(%[bl])\n"		// C200 = bl0(preloading pixel0)
					"lv.q		C210, 16(%[bl])\n"		// C210 = bl1(preloading pixel1)
					".set		pop\n"					// restore assembler option
					:: [bl] "r"(bl)
					:	"memory"
				);
			}

			for (j = 0; j < smax; j += 2)
			{
				unsigned int p1, p2;

				__asm__ volatile (
					".set		push\n"					// save assembler option
					".set		noreorder\n"			// suppress reordering
					"vmov.q		C000, C200\n"			// C000 = next pixel
					"vmov.q		C010, C210\n"			// C010 = next pixel
					"lv.q		C200, 32(%[bl])\n"		// C200 = bl0(preloading pixel0)
					"lv.q		C210, 48(%[bl])\n"		// C200 = bl1(preloading pixel1)
					"vmax.q		C000, C000, C110\n"		// C000 = max(C000(bl0), C110(0))
					"vmax.q		C010, C010, C110\n"		// C010 = max(C010(bl1), C110(0))
					"vmax.s		S020, S000, S001\n"		// S020 = max(S000(r0), S001(g0))
					"vmax.s		S030, S010, S011\n"		// S030 = max(S010(r1), S011(g1))
					"vmax.s		S020, S020, S002\n"		// S020(max0) = max(S020(max(r0, g0)), S002(b0))
					"vmax.s		S030, S030, S012\n"		// S030(max1) = max(S030(max(r1, g1)), S012(b1))
					"vrcp.s		S021, S020\n"			// S021 = 1 / S020(max0)
					"vrcp.s		S031, S030\n"			// S031 = 1 / S030(max1)
					"vmin.s		S003, S020, S100\n"		// S022(a0) = max(S020(max0), S100(255))
					"vmin.s		S013, S030, S100\n"		// S032(a1) = max(S030(max0), S100(255))
					"vmul.s		S021, S003, S021\n"		// S021(t0) = S003(a0) * S021(1 / max0)
					"vmul.s		S031, S013, S031\n"		// S031(t0) = S013(a0) * S031(1 / max1)
					"vscl.q		C000, C000, S021\n"		// C000(p0) = C000(bl0) * S021(t0)
					"vscl.q		C010, C010, S031\n"		// C010(p1) = C010(bl1) * S031(t1)
					"vf2iz.q	C000, C000, 23\n"		// C000(p0) = C000(p0) * 2^23
					"vf2iz.q	C010, C010, 23\n"		// C010(p1) = C010(p1) * 2^23
					"vi2uc.q	S000, C000\n"			// S000(p0) = ((S003>>23)<<24) | ((S002>>23)<<16) | ((S001>>23)<<8) | (S000>>23)
					"vi2uc.q	S010, C010\n"			// S010(p1) = ((S013>>23)<<24) | ((S012>>23)<<16) | ((S011>>23)<<8) | (S010>>23)
					"mfv		%[p1], S000\n"			// p1 = S000
					"mfv		%[p2], S010\n"			// p2 = S010
					".set		pop\n"					// restore assembler option
					:	[p1] "=r" (p1),
						[p2] "=r" (p2)
					:	[bl] "r" (bl)
					:	"memory"
				);

				dest[0] = gammatable[p1 & 0xFF];
				dest[1] = gammatable[(p1 >> 8) & 0xFF];
				dest[2] = gammatable[(p1 >> 16) & 0xFF];
				dest[3] = (p1 >> 24) & 0xFF;

				if (j + 1 < smax)
				{
					dest[4] = gammatable[p2 & 0xFF];
					dest[5] = gammatable[(p2 >> 8) & 0xFF];
					dest[6] = gammatable[(p2 >> 16) & 0xFF];
					dest[7] = (p2 >> 24) & 0xFF;

					dest += 8;
					bl += 8;
				}
				else
				{
					dest += 4;
					bl += 4;
				}
			}
		}
	}
	else
	{
		for (i = 0; i < tmax; i++, dest += stride)
		{
			for (j = 0; j < smax; j++)
			{

				r = Q_ftol( bl[0] );
				g = Q_ftol( bl[1] );
				b = Q_ftol( bl[2] );

				// catch negative lights
				r = (r < 0) ? 0 : r;
				g = (g < 0) ? 0 : g;
				b = (b < 0) ? 0 : b;

				/*
				** determine the brightest of the three color components
				*/
				max = Q_max3( r, g, b );

				/*
				** alpha is ONLY used for the mono lightmap case.  For this reason
				** we set it to the brightest of the color components so that
				** things don't get too dim.
				*/
				a = max;

				/*
				** rescale all the color components if the intensity of the greatest
				** channel exceeds 1.0
				*/
				if (max > 255)
				{
					float t = 255.0F / max;

					r = r*t;
					g = g*t;
					b = b*t;
					a = a*t;
				}

				/*
				** So if we are doing alpha lightmaps we need to set the R, G, and B
				** components to 0 and we need to set alpha to 1-alpha.
				*/
				switch ( monolightmap )
				{
				case 'L':
				case 'I':
					r = a;
					g = b = 0;
					break;
				case 'C':
					// try faking colored lighting
					a = 255 - ((r+g+b)/3);
					r *= a/255.0;
					g *= a/255.0;
					b *= a/255.0;
					break;
				case 'A':
				default:
					r = g = b = 0;
					a = 255 - a;
					break;
				}

				dest[0] = gammatable[r];
				dest[1] = gammatable[g];
				dest[2] = gammatable[b];
				dest[3] = a;

				bl += 4;
				dest += 4;
			}
		}
	}
}

