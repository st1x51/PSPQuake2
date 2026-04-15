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
// gu_warp.c -- sky and water polygons

#include <malloc.h>
#include "gu_local.h"

extern	model_t	*loadmodel;
extern	char	loadname[32];	// for hunk tags

#define	SKY_DIV		4 // 4x4
#define	SKY_S_STEP	(2.0f / (float)SKY_DIV)
#define	SKY_T_STEP	(2.0f / (float)SKY_DIV)

typedef struct
{
	char			name[MAX_QPATH];
	float			rotate;
	vec3_t			axis;
	image_t			*images[6];
	float			mins[2][6], maxs[2][6];
	float			min, max;
#ifdef USE_CACHED_SKY
	gu_vert_ftv_t	*verts;
#endif
} skybox_t;
static skybox_t skybox;

msurface_t	*warpface;

#define	SUBDIVIDE_SIZE	64
//#define	SUBDIVIDE_SIZE	1024

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;
	vec3_t	total;
	float	total_s, total_t;

	if (numverts > 60)
		ri.Sys_Error (ERR_DROP, "numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = SUBDIVIDE_SIZE * floor (m/SUBDIVIDE_SIZE + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}
#if 0
	// add a point in the center to help keep warp valid
	poly = ri.Hunk_AllocName (sizeof(glpoly_t) + ((numverts + 1) * sizeof(gu_vert_ftv_t)), loadname);
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts + 2;
	VectorClear (total);
	total_s = 0;
	total_t = 0;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i+1].xyz);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);

		total_s += s;
		total_t += t;
		VectorAdd (total, verts, total);

		poly->verts[i + 1].u = s;
		poly->verts[i + 1].v = t;
	}

	VectorScale (total, (1.0/numverts), poly->verts[0].xyz);
	poly->verts[0].u = total_s/numverts;
	poly->verts[0].v = total_t/numverts;

	// copy first vertex to last
	memcpy (&poly->verts[i + 1], &poly->verts[1], sizeof(gu_vert_ftv_t));
#else
	poly = ri.Hunk_AllocName (sizeof(glpoly_t) + ((numverts - 1) * sizeof(gu_vert_ftv_t)), loadname);
	poly->c_verts = NULL;
	poly->c_numverts = 0;
	poly->c_frameverts = -1;
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i].xyz);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);

		poly->verts[i].u = s;
		poly->verts[i].v = t;
	}
#endif
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float	r_turbsin[] =
{
	#include "warpsin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
#ifdef USE_FAST_WATER
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t		*p;
	int				i;
	float			s, t, os, ot;
	float			scroll;
	float			rdt = r_newrefdef.time;
	float			scale = 1.0f / 64.0f;

	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ((rdt * 0.5f) - (int)(rdt * 0.5f));
	else
		scroll = 0;

	sceGuTexScale(scale, scale);

	for (p = fa->polys; p; p = p->next)
	{
		os = (r_turbsin[(int)(rdt * TURBSCALE) & 255] + scroll) * scale;
		ot = (r_turbsin[(int)(rdt * TURBSCALE * 0.5f) & 255]) * scale;

		sceGuTexOffset(os, ot);

		if (GU_ClipIsRequired(&p->verts[0], p->numverts))
		{
			gu_vert_ftv_t *cv;
			int cvc;
			GU_Clip(&p->verts[0], p->numverts, &cv, &cvc);
			if (cvc) sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, cvc, 0, cv);
		}
		else sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, p->numverts, 0, &p->verts[0]);
	}
	sceGuTexOffset (0.0f, 0.0f);
	sceGuTexScale (1.0f, 1.0f);
}
#else
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t		*p;
	int				i;
	float			os, ot;
	float			scroll;
	float			rdt = r_newrefdef.time;
	float			scale = 1.0f / 64.0f;
	qboolean		needsclip;

	if (fa->texinfo->flags & SURF_FLOWING)
		scroll = -64 * ((rdt * 0.5f) - (int)(rdt * 0.5f));
	else
		scroll = 0;

	needsclip = (fa->flags & SURF_NEEDS_CLIP);
	if (needsclip)
		fa->flags &= ~SURF_NEEDS_CLIP;

	sceGuTexScale (scale, scale);
	sceGuTexOffset (scroll * scale, 0.0f);

	for (p = fa->polys;p ; p = p->next)
	{
		gu_vert_ftv_t* const out = (gu_vert_ftv_t*)sceGuGetMemory (sizeof(gu_vert_ftv_t) * p->numverts);
		for (i = 0; i < p->numverts; i++)
		{
			os = p->verts[i].u;
			ot = p->verts[i].v;
			out[i].u = os + r_turbsin[(int)((ot * 0.125f + rdt) * TURBSCALE) & 255];
			out[i].v = ot + r_turbsin[(int)((os * 0.125f + rdt) * TURBSCALE) & 255];
			VectorCopy (p->verts[i].xyz, out[i].xyz);
		}

		if (needsclip && GU_ClipIsRequired (out, p->numverts))
		{
			// Clip the polygon.
			gu_vert_ftv_t	*cv;
			int				cvc;

			GU_Clip (out, p->numverts, &cv, &cvc);
#if CLIPPING_DEBUGGING
			sceGuDisable (GU_TEXTURE_2D);
			sceGuColor (0xff00ff00);
#endif
			if (cvc) sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, cvc, 0, cv);
#if CLIPPING_DEBUGGING
			sceGuEnable (GU_TEXTURE_2D);
			sceGuColor (GU_HCOLOR_DEFAULT);
#endif
		}
		else sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, p->numverts, 0, out);
	}

	sceGuTexOffset (0.0f, 0.0f);
	sceGuTexScale (1.0f, 1.0f);
}
#endif

//===================================================================


vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;

	// decide which face it maps to
	VectorCopy (vec3_origin, v);

	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
		VectorAdd (vp, v, v);

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);

	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		if (dv < 0.001) continue;	// don't divide by zero

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		skybox.mins[0][axis] = Q_min(skybox.mins[0][axis], s);
		skybox.mins[1][axis] = Q_min(skybox.mins[1][axis], t);
		skybox.maxs[0][axis] = Q_max(skybox.maxs[0][axis], s);
		skybox.maxs[1][axis] = Q_max(skybox.maxs[1][axis], t);
	}
}

#define	ON_EPSILON		0.1			// point on plane side epsilon
#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	float	*norm;
	float	*v;
	qboolean	front, back;
	float	d, e;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2];
	int		i, j;

	if (nump > MAX_CLIP_VERTS-2)
		ri.Sys_Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{
			e = v[j] + d*(v[j+3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_AddSkySurface
=================
*/
void R_AddSkySurface (msurface_t *fa)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	// calculate vertex values for sky box
	for (p=fa->polys ; p ; p=p->next)
	{
		for (i=0 ; i<p->numverts ; i++)
		{
			VectorSubtract (p->verts[i].xyz, r_origin, verts[i]);
		}
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i=0 ; i<6 ; i++)
	{
		skybox.mins[0][i] = skybox.mins[1][i] = 9999;
		skybox.maxs[0][i] = skybox.maxs[1][i] = -9999;
	}
}

static inline void MakeSkyVec (float s, float t, int axis, gu_vert_ftv_t *out)
{
	vec3_t		b;
	int			j, k;

	b[0] = s * 2300.0f;
	b[1] = t * 2300.0f;
	b[2] = 2300.0f;

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		out->xyz[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
	}

#ifndef USE_CACHED_SKY
	if (!skybox.rotate)
		VectorAdd(out->xyz, r_origin, out->xyz);
#endif

	// avoid bilerp seam
	s = (s + 1.0f) * 0.5f;
	t = (t + 1.0f) * 0.5f;

	s = Q_bound(skybox.min, s, skybox.max);
	t = Q_bound(skybox.min, t, skybox.max);

	out->u = s;
	out->v = 1.0 - t;
}

#ifdef USE_CACHED_SKY
/*
==============
R_BuildSkyBoxVertices
==============
*/
static void R_BuildSkyBoxVertices (void)
{
	int				i, sy, sx;
	float			s, t;
	gu_vert_ftv_t	*v;
	size_t			verts_size;

	verts_size = 6 * SKY_DIV * SKY_DIV * 4 * sizeof(gu_vert_ftv_t);

	skybox.verts = memalign (16, verts_size);
	if (!skybox.verts)
		ri.Sys_Error(ERR_FATAL, "No enough space for skybox vertices!");

	for (i = 0; i < 6; i++)
	{
		for (sy = 0; sy < SKY_DIV; sy++)
		{
			t = -1.0f + (sy * SKY_T_STEP);
			for (sx = 0; sx < SKY_DIV; sx++)
			{
				s = -1.0f + (sx * SKY_S_STEP);

				v = &skybox.verts[(i * SKY_DIV * SKY_DIV * 4) + ((sy * SKY_DIV + sx) * 4)];
				MakeSkyVec( s,              t,              i, &v[0]);
				MakeSkyVec( s,              t + SKY_T_STEP, i, &v[1]);
				MakeSkyVec( s + SKY_S_STEP, t + SKY_T_STEP, i, &v[2]);
				MakeSkyVec( s + SKY_S_STEP, t,              i, &v[3]);
			}
		}
	}
	sceKernelDcacheWritebackRange (skybox.verts, verts_size);
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int	i;

	if (!skybox.verts)
		return;

	sceGumPushMatrix ();
	sceGumTranslate ((ScePspFVector3*)&r_origin);

	if (skybox.rotate)
	{
		if (skybox.axis[0]) sceGumRotateX (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
		if (skybox.axis[1]) sceGumRotateY (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
		if (skybox.axis[2]) sceGumRotateZ (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
	}

	sceGumUpdateMatrix ();

	for (i = 0; i < 6; i++)
	{
		if (!skybox.rotate && (skybox.mins[0][i] >= skybox.maxs[0][i] || skybox.mins[1][i] >= skybox.maxs[1][i]))
			continue;

		GL_Bind (skybox.images[skytexorder[i]]);

		sceGuDrawArrayN (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 
			4, SKY_DIV * SKY_DIV, NULL, &skybox.verts[i * SKY_DIV * SKY_DIV * 4]);
	}

	sceGumPopMatrix ();
	sceGumUpdateMatrix ();
}
#else
/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int				i;
	ScePspFVector3	translate;

#if 0
	qglEnable (GL_BLEND);
	GL_TexEnv( GL_MODULATE );
	qglColor4f (1,1,1,0.5);
	qglDisable (GL_DEPTH_TEST);
#endif
	if (skybox.rotate)
	{	// check for no sky at all
		for (i=0 ; i<6 ; i++)
		{
			if (skybox.mins[0][i] < skybox.maxs[0][i] && skybox.mins[1][i] < skybox.maxs[1][i])
				break;
		}
		if (i == 6) return; // nothing visible
	}

	if (skybox.rotate)
	{
		sceGumPushMatrix ();

		translate.x = r_origin[0];
		translate.y = r_origin[1];
		translate.z = r_origin[2];

		sceGumTranslate (&translate);

		if (skybox.axis[0]) sceGumRotateX (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
		if (skybox.axis[1]) sceGumRotateY (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
		if (skybox.axis[2]) sceGumRotateZ (r_newrefdef.time * skybox.rotate * (GU_PI / 180.0f));
		sceGumUpdateMatrix ();

		GU_ClipSetModelFrustum ();
	}

	for (i = 0; i < 6; i++)
	{
		if (skybox.rotate)
		{	// hack, forces full sky to draw when rotating
			skybox.mins[0][i] = -1;
			skybox.mins[1][i] = -1;
			skybox.maxs[0][i] = 1;
			skybox.maxs[1][i] = 1;
		}

		if (skybox.mins[0][i] >= skybox.maxs[0][i] || skybox.mins[1][i] >= skybox.maxs[1][i])
			continue;

		GL_Bind (skybox.images[skytexorder[i]]);

		gu_vert_ftv_t* const uv = (gu_vert_ftv_t*)sceGuGetMemory (sizeof(gu_vert_ftv_t) * 4);
		MakeSkyVec (skybox.mins[0][i], skybox.mins[1][i], i, &uv[0]);
		MakeSkyVec (skybox.mins[0][i], skybox.maxs[1][i], i, &uv[1]);
		MakeSkyVec (skybox.maxs[0][i], skybox.maxs[1][i], i, &uv[2]);
		MakeSkyVec (skybox.maxs[0][i], skybox.mins[1][i], i, &uv[3]);

		if (GU_ClipIsRequired (uv, 4))
		{
			// Clip the polygon.
			gu_vert_ftv_t	*cv;
			int				cvc;
#if CLIPPING_DEBUGGING
			sceGuDisable (GU_TEXTURE_2D);
			sceGuColor (0xffff0000);
#endif
			GU_Clip (uv, 4, &cv, &cvc);
			if (cvc) sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, cvc, 0, cv);
#if CLIPPING_DEBUGGING
			sceGuEnable (GU_TEXTURE_2D);
			sceGuColor (GU_HCOLOR_DEFAULT);
#endif
		}
		else sceGuDrawArray (GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF, 4, 0, uv);

	}

	if (skybox.rotate)
	{
		GU_ClipRestoreWorldFrustum ();
		sceGumPopMatrix ();
		sceGumUpdateMatrix ();
	}
#if 0
	glDisable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f (1,1,1,0.5);
	glEnable (GL_DEPTH_TEST);
#endif
}
#endif

/*
============
R_FreeSkybox
============
*/
void R_PurgeSkybox (void)
{
	int	i;

	// release old skybox texture
	for (i = 0; i < 6; i++)
	{
		if (!skybox.images[i])
			continue;
		GL_FreeImage (skybox.images[i]);
	}

#ifdef USE_CACHED_SKY
	if (skybox.verts)
		free (skybox.verts);
#endif

	memset(&skybox, 0, sizeof(skybox));
}

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void R_SetSky (char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];
	float	skymip;

	if (!name || !name[0])
	{
		R_PurgeSkybox ();
		return;
	}

	if(strncmp(name, skybox.name, sizeof(skybox.name) - 1))
		R_PurgeSkybox ();

	strncpy (skybox.name, name, sizeof(skybox.name)-1);
	skybox.rotate = rotate;
	VectorCopy (axis, skybox.axis);

	// save value
	skymip = gl_skymip->value;

	// chop down rotating skies for less memory
	if (skybox.rotate && (gl_skymip->value == 0.0f))
		gl_skymip->value = 1.0f;

	for (i=0 ; i<6 ; i++)
	{
		if (gl_skytga->value)
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", skybox.name, suf[i]);
		else
			Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skybox.name, suf[i]);

		skybox.images[i] = GL_FindImage (pathname, IMG_TYPE_SKY);
		if (!skybox.images[i])
			skybox.images[i] = r_notexture;
	}

	// take less memory
	if (gl_skymip->value)
	{
		// restore value
		gl_skymip->value = skymip;

		skybox.min = 1.0/256;
		skybox.max = 255.0/256;
	}
	else
	{
		skybox.min = 1.0/512;
		skybox.max = 511.0/512;
	}
#ifdef USE_CACHED_SKY
	R_BuildSkyBoxVertices ();
#endif
}
