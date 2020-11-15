#include "gu_local.h"
#include "gu_model.h"

#include "clipping.hpp"
using namespace quake;

extern	model_t	*loadmodel;

char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;
image_t	*sky_images[6];

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

float	skymins[2][6], skymaxs[2][6];
float	sky_min, sky_max;

void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;
#if 0
glBegin (GL_POLYGON);
for (i=0 ; i<nump ; i++, vecs+=3)
{
	VectorAdd(vecs, r_origin, v);
	qglVertex3fv (v);
}
glEnd();
return;
#endif
	// decide which face it maps to
	VectorCopy (vec3_origin, v);
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
	{
		VectorAdd (vp, v, v);
	}
	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001)
			continue;	// don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
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
		Sys_Error ("ClipSkyPolygon: MAX_CLIP_VERTS");
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


extern msurface_t* r_sky_surfaces;
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


//#define GU_TRANSF


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
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

static float s_axis;
static float t_axis;
static vec3_t v_axis;

void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		b;
	int			j, k;

	b[0] = s*2560;
	b[1] = t*2560;
	b[2] = 2560;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v_axis[j] = -b[-k - 1];
		else
			v_axis[j] = b[k - 1];
#ifndef GU_TRANSF
		v_axis[j] += r_origin[j]; //from q1
#endif
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	if (s < sky_min)
		s = sky_min;
	else if (s > sky_max)
		s = sky_max;
	if (t < sky_min)
		t = sky_min;
	else if (t > sky_max)
		t = sky_max;

		
	t = 1.0 - t;

	s_axis = s;
	t_axis = t;
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int		i;
#ifdef GU_TRANSF
	if (skyrotate)
	{	// check for no sky at all
		int j;
		for (j=0 ; j<6 ; j++)
			if (skymins[0][j] < skymaxs[0][j]
			&& skymins[1][j] < skymaxs[1][j])
				break;
		if (j == 6)
			return;		// nothing visible
	}

    sceGumPushMatrix ();

    ScePspFVector3 translate = {r_origin[0], r_origin[1], r_origin[2]};
	sceGumTranslate(&translate);

	if(skyaxis[0])
       sceGumRotateX(r_newrefdef.time * skyrotate * (GU_PI / 180.0f));

	if(skyaxis[1])
	   sceGumRotateY(r_newrefdef.time * skyrotate * (GU_PI / 180.0f));

	if(skyaxis[2])
	   sceGumRotateZ(r_newrefdef.time * skyrotate * (GU_PI / 180.0f));

    
#endif
	for (i=0 ; i<6 ; i++)
	{
#ifdef GU_TRANSF
        if (skyrotate)
		{	// hack, forces full sky to draw when rotating
			skymins[0][i] = -1;
			skymins[1][i] = -1;
			skymaxs[0][i] = 1;
			skymaxs[1][i] = 1;
		}
#endif
		if (skymins[0][i] >= skymaxs[0][i]
		|| skymins[1][i] >= skymaxs[1][i])
			continue;
	
		// Allocate memory for this polygon.
		const int		unclipped_vertex_count	= 4;
		glvert_t* const	unclipped_vertices		=
			static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));
       GL_Bind (sky_images[skytexorder[i]]->texnum);

		MakeSkyVec (skymins[0][i], skymins[1][i], i);

        unclipped_vertices[0].st[0]	    = s_axis;
        unclipped_vertices[0].st[1]	    = t_axis;
        unclipped_vertices[0].xyz[0]	= v_axis[0];
        unclipped_vertices[0].xyz[1]	= v_axis[1];
        unclipped_vertices[0].xyz[2]	= v_axis[2];

		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);

        unclipped_vertices[1].st[0]	    = s_axis;
        unclipped_vertices[1].st[1]	    = t_axis;
        unclipped_vertices[1].xyz[0]	= v_axis[0];
        unclipped_vertices[1].xyz[1]	= v_axis[1];
        unclipped_vertices[1].xyz[2]	= v_axis[2];

		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);

        unclipped_vertices[2].st[0]	    = s_axis;
        unclipped_vertices[2].st[1]	    = t_axis;
        unclipped_vertices[2].xyz[0]	= v_axis[0];
        unclipped_vertices[2].xyz[1]	= v_axis[1];
        unclipped_vertices[2].xyz[2]	= v_axis[2];

		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);

        unclipped_vertices[3].st[0]	    = s_axis;
        unclipped_vertices[3].st[1]	    = t_axis;
        unclipped_vertices[3].xyz[0]	= v_axis[0];
        unclipped_vertices[3].xyz[1]	= v_axis[1];
        unclipped_vertices[3].xyz[2]	= v_axis[2];

        if (clipping::is_clipping_required(
            unclipped_vertices,
            unclipped_vertex_count))
        {
	          // Clip the polygon.
            const glvert_t*	clipped_vertices;
            std::size_t		clipped_vertex_count;
            clipping::clip(
                unclipped_vertices,
                unclipped_vertex_count,
                &clipped_vertices,
                &clipped_vertex_count);

            // Did we have any vertices left?
            if (clipped_vertex_count)
            {
                // Copy the vertices to the display list.
                const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
                glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
                memcpy(display_list_vertices, clipped_vertices, buffer_size);

                // Draw the clipped vertices.
                sceGuDrawArray(
                    GU_TRIANGLE_FAN,
                    GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
                    clipped_vertex_count, 0, display_list_vertices);
            }
	    }
        else
        {
            // Draw the poly directly.
            sceGuDrawArray(
                GU_TRIANGLE_FAN,
                GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
                unclipped_vertex_count, 0, unclipped_vertices);
        }
	}
#ifdef GU_TRANSF
 	sceGumPopMatrix();
 	sceGumUpdateMatrix();
#endif
}

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void R_RegisterSky(const char* name, float rotate, vec3_t axis) 
{
	int		i;
	char	pathname[MAX_QPATH];
/*
    if (strncmp(skyname, name, sizeof(skyname)-1)) //Crow_bar. pure old sky (if use command "sky")
    {
       	for (i = 0; i < 6; i++)
	    {
			if(sky_images[i])
			{
			   if(sky_images[i]->texnum != r_notexture->texnum) //don't unload notexture
			      GL_UnloadTexture(sky_images[i]->texnum);
            }
		}
	}
*/
	strncpy (skyname, name, sizeof(skyname)-1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	for (i = 0; i < 6; i++)
	{
		if(gu_tgasky->value)
		{
		   Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", skyname, suf[i]);
		}
		else
		{ 
		   Com_sprintf (pathname, sizeof(pathname), "env/%s%s.pcx", skyname, suf[i]);
        }

		sky_images[i] = GL_FindImage (pathname, it_skin);
/*
		if (!sky_images[i])
			 sky_images[i] = r_notexture;
*/
#if 0
		if (skyrotate)
		{	// take less memory
			sky_min = 1.0/256;
			sky_max = 255.0/256;
		}
		else
#endif
		{
			sky_min = 1.0/512;
			sky_max = 511.0/512;
		}
	}
}
