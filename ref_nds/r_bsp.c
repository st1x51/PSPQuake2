/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// r_bsp.c

#include "r_local.h"
#include "../null/ds.h"

//#define BOXTESTED_BSP
//#define USE_HDR
//
// current entity info
//
qboolean		insubmodel;
entity_t		*currententity;
vec3_t			modelorg;		// modelorg is the viewpoint reletive to
								// the currently rendering entity
vec3_t			r_entorigin;	// the currently rendering entity in world
								// coordinates

float			entity_rotation[3][3];

int				r_currentbkey;

typedef enum {touchessolid, drawnode, nodrawnode} solidstate_t;

#define MAX_BMODEL_VERTS	500			// 6K
#define MAX_BMODEL_EDGES	1000		// 12K

static mvertex_t	*pbverts;
static bedge_t		*pbedges;
static int			numbverts, numbedges;

static mvertex_t	*pfrontenter, *pfrontexit;

static qboolean		makeclippededge;

int fp_modelorg[3];

void R_RenderPoly(msurface_t *fa);


//===========================================================================

/*
================
R_EntityRotate
================
*/
void R_EntityRotate (vec3_t vec)
{
	vec3_t	tvec;

	VectorCopy (vec, tvec);
	vec[0] = DotProduct (entity_rotation[0], tvec);
	vec[1] = DotProduct (entity_rotation[1], tvec);
	vec[2] = DotProduct (entity_rotation[2], tvec);
}/*4236505542*/


/*
================
R_RotateBmodel
================
*/
void R_RotateBmodel (void)
{
	float	angle, s, c, temp1[3][3], temp2[3][3], temp3[3][3];

// TODO: should use a look-up table
// TODO: should really be stored with the entity instead of being reconstructed
// TODO: could cache lazily, stored in the entity
// TODO: share work with R_SetUpAliasTransform

// yaw
	angle = currententity->angles[YAW];		
	angle = angle * M_PI*2 / 360;
	s = sin(angle);
	c = cos(angle);

	temp1[0][0] = c;
	temp1[0][1] = s;
	temp1[0][2] = 0;
	temp1[1][0] = -s;
	temp1[1][1] = c;
	temp1[1][2] = 0;
	temp1[2][0] = 0;
	temp1[2][1] = 0;
	temp1[2][2] = 1;


// pitch
	angle = currententity->angles[PITCH];		
	angle = angle * M_PI*2 / 360;
	s = sin(angle);
	c = cos(angle);

	temp2[0][0] = c;
	temp2[0][1] = 0;
	temp2[0][2] = -s;
	temp2[1][0] = 0;
	temp2[1][1] = 1;
	temp2[1][2] = 0;
	temp2[2][0] = s;
	temp2[2][1] = 0;
	temp2[2][2] = c;

	R_ConcatRotations (temp2, temp1, temp3);

// roll
	angle = currententity->angles[ROLL];		
	angle = angle * M_PI*2 / 360;
	s = sin(angle);
	c = cos(angle);

	temp1[0][0] = 1;
	temp1[0][1] = 0;
	temp1[0][2] = 0;
	temp1[1][0] = 0;
	temp1[1][1] = c;
	temp1[1][2] = s;
	temp1[2][0] = 0;
	temp1[2][1] = -s;
	temp1[2][2] = c;

	R_ConcatRotations (temp1, temp3, entity_rotation);

//
// rotate modelorg and the transformation matrix
//
	R_EntityRotate (modelorg);
	R_EntityRotate (vpn);
	R_EntityRotate (vright);
	R_EntityRotate (vup);

	R_TransformFrustum ();
}


/*
================
R_RecursiveClipBPoly

Clip a bmodel poly down the world bsp tree
================
*/
//void R_RecursiveClipBPoly (bedge_t *pedges, mnode_t *pnode, msurface_t *psurf)
//{
//	bedge_t		*psideedges[2], *pnextedge, *ptedge;
//	int			i, side, lastside;
//	float		dist, frac, lastdist;
//	mplane_t	*splitplane, tplane;
//	mvertex_t	*pvert, *plastvert, *ptvert;
//	mnode_t		*pn;
//	int			area;
//
//	psideedges[0] = psideedges[1] = NULL;
//
////	makeclippededge = false;
//	byte_write(&makeclippededge, false);
//
//// transform the BSP plane into model space
//// FIXME: cache these?
//	splitplane = pnode->plane;
//	tplane.dist = splitplane->dist -
//			DotProduct(r_entorigin, splitplane->normal);
//	tplane.normal[0] = DotProduct (entity_rotation[0], splitplane->normal);
//	tplane.normal[1] = DotProduct (entity_rotation[1], splitplane->normal);
//	tplane.normal[2] = DotProduct (entity_rotation[2], splitplane->normal);
//
//// clip edges to BSP plane
//	for ( ; pedges ; pedges = pnextedge)
//	{
//		pnextedge = pedges->pnext;
//
//	// set the status for the last point as the previous point
//	// FIXME: cache this stuff somehow?
//		plastvert = pedges->v[0];
//		lastdist = DotProduct (plastvert->position, tplane.normal) -
//				   tplane.dist;
//
//		if (lastdist > 0)
//			lastside = 0;
//		else
//			lastside = 1;
//
//		pvert = pedges->v[1];
//
//		dist = DotProduct (pvert->position, tplane.normal) - tplane.dist;
//
//		if (dist > 0)
//			side = 0;
//		else
//			side = 1;
//
//		if (side != lastside)
//		{
//		// clipped
//			if (numbverts >= MAX_BMODEL_VERTS)
//				return;
//
//		// generate the clipped vertex
//			frac = lastdist / (lastdist - dist);
//			ptvert = &pbverts[numbverts++];
//			ptvert->position[0] = plastvert->position[0] +
//					frac * (pvert->position[0] -
//					plastvert->position[0]);
//			ptvert->position[1] = plastvert->position[1] +
//					frac * (pvert->position[1] -
//					plastvert->position[1]);
//			ptvert->position[2] = plastvert->position[2] +
//					frac * (pvert->position[2] -
//					plastvert->position[2]);
//
//		// split into two edges, one on each side, and remember entering
//		// and exiting points
//		// FIXME: share the clip edge by having a winding direction flag?
//			if (numbedges >= (MAX_BMODEL_EDGES - 1))
//			{
//				ri.Con_Printf (PRINT_ALL,"Out of edges for bmodel\n");
//				return;
//			}
//
//			ptedge = &pbedges[numbedges];
//			ptedge->pnext = psideedges[lastside];
//			psideedges[lastside] = ptedge;
//			ptedge->v[0] = plastvert;
//			ptedge->v[1] = ptvert;
//
//			ptedge = &pbedges[numbedges + 1];
//			ptedge->pnext = psideedges[side];
//			psideedges[side] = ptedge;
//			ptedge->v[0] = ptvert;
//			ptedge->v[1] = pvert;
//
//			numbedges += 2;
//
//			if (side == 0)
//			{
//			// entering for front, exiting for back
//				pfrontenter = ptvert;
////				makeclippededge = true;
//				byte_write(&makeclippededge, true);
//			}
//			else
//			{
//				pfrontexit = ptvert;
////				makeclippededge = true;
//				byte_write(&makeclippededge, true);
//			}
//		}
//		else
//		{
//		// add the edge to the appropriate side
//			pedges->pnext = psideedges[side];
//			psideedges[side] = pedges;
//		}
//	}
//
//// if anything was clipped, reconstitute and add the edges along the clip
//// plane to both sides (but in opposite directions)
//	if (makeclippededge)
//	{
//		if (numbedges >= (MAX_BMODEL_EDGES - 2))
//		{
//			ri.Con_Printf (PRINT_ALL,"Out of edges for bmodel\n");
//			return;
//		}
//
//		ptedge = &pbedges[numbedges];
//		ptedge->pnext = psideedges[0];
//		psideedges[0] = ptedge;
//		ptedge->v[0] = pfrontexit;
//		ptedge->v[1] = pfrontenter;
//
//		ptedge = &pbedges[numbedges + 1];
//		ptedge->pnext = psideedges[1];
//		psideedges[1] = ptedge;
//		ptedge->v[0] = pfrontenter;
//		ptedge->v[1] = pfrontexit;
//
//		numbedges += 2;
//	}
//
//// draw or recurse further
//	for (i=0 ; i<2 ; i++)
//	{
//		if (psideedges[i])
//		{
//		// draw if we've reached a non-solid leaf, done if all that's left is a
//		// solid leaf, and continue down the tree if it's not a leaf
//			pn = pnode->children[i];
//
//		// we're done with this branch if the node or leaf isn't in the PVS
//			if (pn->visframe == r_visframecount)
//			{
//				if (pn->contents != CONTENTS_NODE)
//				{
//					if (pn->contents != CONTENTS_SOLID)
//					{
//						if (r_newrefdef.areabits)
//						{
//							area = ((mleaf_t *)pn)->area;
//							if (! (r_newrefdef.areabits[area>>3] & (1<<(area&7)) ) )
//								continue;		// not visible
//						}
//
//						r_currentbkey = ((mleaf_t *)pn)->key;
//						R_RenderBmodelFace (psideedges[i], psurf);
//					}
//				}
//				else
//				{
//					R_RecursiveClipBPoly (psideedges[i], pnode->children[i],
//									  psurf);
//				}
//			}
//		}
//	}
//}


/*
================
R_DrawSolidClippedSubmodelPolygons

Bmodel crosses multiple leafs
================
*/
//void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel, mnode_t *topnode)
//{
//	int			i, j, lindex;
//	vec_t		dot;
//	msurface_t	*psurf;
//	int			numsurfaces;
//	mplane_t	*pplane;
//	mvertex_t	bverts[MAX_BMODEL_VERTS];
//	bedge_t		bedges[MAX_BMODEL_EDGES], *pbedge;
//	medge_t		*pedge, *pedges;
//
//// FIXME: use bounding-box-based frustum clipping info?
//
//	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
//	numsurfaces = pmodel->nummodelsurfaces;
//	pedges = pmodel->edges;
//
//	for (i=0 ; i<numsurfaces ; i++, psurf++)
//	{
//	// find which side of the node we are on
//		pplane = psurf->plane;
//
//		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
//
//	// draw the polygon
//		if (( !(psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
//			((psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
//			continue;
//
//	// FIXME: use bounding-box-based frustum clipping info?
//
//	// copy the edges to bedges, flipping if necessary so always
//	// clockwise winding
//	// FIXME: if edges and vertices get caches, these assignments must move
//	// outside the loop, and overflow checking must be done here
//		pbverts = bverts;
//		pbedges = bedges;
//		numbverts = numbedges = 0;
//		pbedge = &bedges[numbedges];
//		numbedges += psurf->numedges;
//
//		for (j=0 ; j<psurf->numedges ; j++)
//		{
//		   lindex = pmodel->surfedges[psurf->firstedge+j];
//
//			if (lindex > 0)
//			{
//				pedge = &pedges[lindex];
//				pbedge[j].v[0] = &r_pcurrentvertbase[pedge->v[0]];
//				pbedge[j].v[1] = &r_pcurrentvertbase[pedge->v[1]];
//			}
//			else
//			{
//				lindex = -lindex;
//				pedge = &pedges[lindex];
//				pbedge[j].v[0] = &r_pcurrentvertbase[pedge->v[1]];
//				pbedge[j].v[1] = &r_pcurrentvertbase[pedge->v[0]];
//			}
//
//			pbedge[j].pnext = &pbedge[j+1];
//		}
//
//		pbedge[j-1].pnext = NULL;	// mark end of edges
//
//		if ( !( psurf->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 ) ) )
//			R_RecursiveClipBPoly (pbedge, topnode, psurf);
//		else
//			R_RenderBmodelFace( pbedge, psurf );
//	}
//}


/*
================
R_DrawSubmodelPolygons

All in one leaf
================
*/
//void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags, mnode_t *topnode)
//{
//	int			i;
//	vec_t		dot;
//	msurface_t	*psurf;
//	int			numsurfaces;
//	mplane_t	*pplane;
//
//// FIXME: use bounding-box-based frustum clipping info?
//
//	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
//	numsurfaces = pmodel->nummodelsurfaces;
//
//	for (i=0 ; i<numsurfaces ; i++, psurf++)
//	{
//	// find which side of the node we are on
//		pplane = psurf->plane;
//
//		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
//
//	// draw the polygon
//		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
//			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
//		{
//			r_currentkey = ((mleaf_t *)topnode)->key;
//
//		// FIXME: use bounding-box-based frustum clipping info?
//			R_RenderFace (psurf, clipflags);
//		}
//	}
//}

/*
================
R_RecursiveWorldNode
================
*/

int perp_sides = 0;
int angled_sides = 0;
int clipped_faces = 0;
bool R_RecursiveWorldNode (mnode_t *node, int clipflags) __attribute__((section(".itcm"), long_call));

#if 0
bool R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			i, c, side, myside, *pindex;
	//vec3_t		acceptpt, rejectpt;
	
	
	int bigdist;
	int bignormal[3];
	int d1, d2;
	
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	float		d, dot;
	int idot;
	mleaf_t		*pleaf;

//	if (node->contents == CONTENTS_SOLID)
//		return;		// solid
//
//	if (node->visframe != r_visframecount)
//		return;

// cull the clipping planes if not trivial accept
// FIXME: the compiler is doing a lousy job of optimizing here; it could be
//  twice as fast in ASM
#ifndef BOXTESTED_BSP
	if (clipflags)
	{
		for (i=0 ; i<4 ; i++)
		{
			if (! (clipflags & (1<<i)) )
				continue;	// don't need to clip against it
			
//			clipped_faces++;

		// generate accept and reject points
		// FIXME: do with fast look-ups or integer tests based on the sign bit
		// of the floating point values
			
			pindex = pfrustum_indexes[i];

			bignormal[0] = view_clipplanes[i].normal_int[0];
			bignormal[1] = view_clipplanes[i].normal_int[1];
			bignormal[2] = view_clipplanes[i].normal_int[2];
			
			bigdist = view_clipplanes[i].dist_int;
			
			d1 = node->minmaxs[pindex[0]] * bignormal[0]
				+ node->minmaxs[pindex[1]] * bignormal[1]
				+ node->minmaxs[pindex[2]] * bignormal[2];
			
			d1 -= bigdist;

			if (d1 <= 0)
				return false;
			
			d2 = node->minmaxs[pindex[3+0]] * bignormal[0]
				+ node->minmaxs[pindex[3+1]] * bignormal[1]
				+ node->minmaxs[pindex[3+2]] * bignormal[2];
			
			d2 -= bigdist;

			if (d2 >= 0)
				clipflags &= ~(1<<i);	// node is entirely on screen
		}
	}
#endif
	
#ifdef BOXTESTED_BSP
	if (psp_boxtest(node->bt_minmaxs[0], node->bt_minmaxs[1], node->bt_minmaxs[2],
			node->bt_minmaxs[3], node->bt_minmaxs[4], node->bt_minmaxs[5]) == 0)
		return;
#endif

// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
				return false;		// not visible
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}
	}
	else
	{
	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
		plane = node->plane;
		
		switch (plane->type)
		{
		case PLANE_X:
		case PLANE_Y:
		case PLANE_Z:
			side = (fp_modelorg[plane->type] - plane->int_dist) < 0;
//			perp_sides++;
			break;
		default:
		{
#define BIG_TYPE long long
			BIG_TYPE big_temp = (BIG_TYPE)fp_modelorg[0] * (BIG_TYPE)plane->int_normal[0]
					+ (BIG_TYPE)fp_modelorg[1] * (BIG_TYPE)plane->int_normal[1]
					+ (BIG_TYPE)fp_modelorg[2] * (BIG_TYPE)plane->int_normal[2];
			idot = big_temp >> 13;
			idot -= plane->int_dist;
#undef BIG_TYPE
			
			if (idot >= 0)
				side = 0;
			else
				side = 1;
			
//			angled_sides++;
			break;
		}
		}

	// recurse down the children, front side first
		
		bool retr = false;
		if ((node->children[side]->contents != CONTENTS_SOLID) && (node->children[side]->visframe == r_visframecount))
			retr = R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
		c = node->numsurfaces;

		if (retr && c)
		{
			surf = r_worldmodel->surfaces + node->firstsurface;
			
			do
			{
				if (surf->visframe == r_framecount)
				{
					R_RenderPoly(surf, clipflags);
				}

				surf++;
			} while (--c);
		}

	// recurse down the back side
		if ((node->children[!side]->contents != CONTENTS_SOLID) && (node->children[!side]->visframe == r_visframecount))
			retr |= R_RecursiveWorldNode (node->children[!side], clipflags);
		
		return retr;
	}
	
	return true;
}

#else

bool world_clip(mnode_t *node, unsigned char &clip) __attribute__ ((no_instrument_function));
bool world_clip(mnode_t *node, unsigned char &clip)
{
	int *pindex;
	int bigdist;
	int bignormal[3];
	int d1, d2;
	
//	int clipflags = *clip;
	
	for (int i=0 ; i<4 ; i++)
	{
		if (! (clip & (1<<i)) )
			continue;	// don't need to clip against it

		pindex = pfrustum_indexes[i];

		bignormal[0] = view_clipplanes[i].normal_int[0];
		bignormal[1] = view_clipplanes[i].normal_int[1];
		bignormal[2] = view_clipplanes[i].normal_int[2];

		bigdist = view_clipplanes[i].dist_int;

		d1 = node->minmaxs[pindex[0]] * bignormal[0]
		     + node->minmaxs[pindex[1]] * bignormal[1]
		     + node->minmaxs[pindex[2]] * bignormal[2];

		d1 -= bigdist;

		if (d1 <= 0)
			return false;

		d2 = node->minmaxs[pindex[3+0]] * bignormal[0]
		     + node->minmaxs[pindex[3+1]] * bignormal[1]
		     + node->minmaxs[pindex[3+2]] * bignormal[2];

		d2 -= bigdist;

		if (d2 >= 0)
			clip &= ~(1<<i);	// node is entirely on screen
	}
	
	return true;
}

#define MAX_DEPTH 100
struct bsp_stack_s
{
	mnode_t *node;
	unsigned char clipflags;
	unsigned char return_val;
	
	unsigned char side;
	unsigned char accum_retr;
	
	unsigned char lr;
};

msurface_t *head_alpha_surf = NULL;
//msurface_t *render_surfs[1000];
//unsigned int surfaces_to_render = 0;

bool R_RecursiveWorldNode (mnode_t *start_node, int start_clipflags)
{
	struct bsp_stack_s bsp_stack[MAX_DEPTH];
	struct bsp_stack_s *sp = bsp_stack;
	
	sp->node = start_node;
	sp->clipflags = start_clipflags;
	sp->lr = 0;		//start from the top
	
	
	while (sp >= &bsp_stack[0])
	{
		if (sp > &bsp_stack[MAX_DEPTH - 1])
			Sys_Error("we\'ve run out of fake bsp stack!\n");
		
		//arguments
		mnode_t *node = sp->node;
		unsigned char *clipflags = &sp->clipflags;
		unsigned char *side = &sp->side;
		
		switch (sp->lr)
		{
		case 0:
		{
			if (*clipflags)
				if (world_clip(node, *clipflags) == false)
				{
					sp->return_val = false;
					sp--;
					continue;
				}
		}
		case 1:
		{
			if ((node)->contents != -1)
			{
				mleaf_t *pleaf = (mleaf_t *)(node);

				if (r_newrefdef.areabits)
				{
					if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
					{
						sp->return_val = false;
						sp--;
						continue;
					}
				}

				msurface_t **mark = pleaf->firstmarksurface;
				int c = pleaf->nummarksurfaces;

				if (c)
				{
					do
					{
						(*mark)->visframe = r_framecount;
						mark++;
					} while (--c);
				}

				sp->return_val = true;
				sp--;
				continue;
			}
			else
			{
				mplane_t *plane = (node)->plane;

				switch (plane->type)
				{
				case PLANE_X:
				case PLANE_Y:
				case PLANE_Z:
					*side = (fp_modelorg[plane->type] - plane->int_dist) < 0;
					break;
				default:
				{
#define BIG_TYPE long long
					BIG_TYPE big_temp = (BIG_TYPE)fp_modelorg[0] * (BIG_TYPE)plane->int_normal[0]
					                                                                           + (BIG_TYPE)fp_modelorg[1] * (BIG_TYPE)plane->int_normal[1]
					                                                                                                                                    + (BIG_TYPE)fp_modelorg[2] * (BIG_TYPE)plane->int_normal[2];
					int idot = big_temp >> 13;
					idot -= plane->int_dist;
#undef BIG_TYPE

					if (idot >= 0)
						*side = 0;
					else
						*side = 1;

					break;
				}
				}
				
				sp->accum_retr = false;

//				bool retr = false;
//				if ((node->children[side]->contents != CONTENTS_SOLID) && (node->children[side]->visframe == r_visframecount))
//					retr = R_RecursiveWorldNode (node->children[side], clipflags);
				
				if (((node)->children[*side]->contents != CONTENTS_SOLID) && ((node)->children[*side]->visframe == r_visframecount))
				{
					sp->lr = 2;
					sp++;
					
					sp->node = (node)->children[*side];
					sp->clipflags = *clipflags;
					sp->lr = 0;
					
					continue;
				}
				else
					(sp + 1)->return_val = false;
			}
		}
		case 2:
		{
			sp->accum_retr = (sp + 1)->return_val;
			// draw stuff
			int c = (node)->numsurfaces;

			if ((sp + 1)->return_val && c)
			{
				msurface_t *surf = r_worldmodel->surfaces + (node)->firstsurface;

				do
				{
					if (__builtin_expect(surf->visframe == r_framecount, 1))
					{
						if (surf->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 ))
						{
							msurface_t *temp_surf = head_alpha_surf;
							head_alpha_surf = surf;
							surf->nextalphasurface = temp_surf;
						}
						else
						{
							R_RenderPoly(surf);
						}
					}

					surf++;
				} while (--c);
				
//				do
//				{
//					if (__builtin_expect(surf->visframe == r_framecount, 1))
//					{
//						render_surfs[surfaces_to_render] = surf;
//						surfaces_to_render++;
//					}
//
//					surf++;
//				} while (--c);
				
			}

			// recurse down the back side
			//				if ((node->children[!side]->contents != CONTENTS_SOLID) && (node->children[!side]->visframe == r_visframecount))
			//					retr |= R_RecursiveWorldNode (node->children[!side], clipflags);

			if (((node)->children[!*side]->contents != CONTENTS_SOLID) && ((node)->children[!*side]->visframe == r_visframecount))

			{
				sp->lr = 3;
				sp++;

				sp->node = (node)->children[!*side];
				sp->clipflags = *clipflags;
				sp->lr = 0;

				continue;
			}
			else
				(sp + 1)->return_val = false;
		}
		case 3:
		{
			sp->return_val = (sp + 1)->return_val | sp->accum_retr;
			sp--;
			continue;
		}
		default:
			Sys_Error("bsp recursion: %d is not a valid lr\n", sp->lr);
		}
	}
	return (sp + 1)->return_val;
	return true;
}

#endif



/*
================
R_RenderWorld
================
*/
int rendering_is_go = 0;
int render_flags = 0;

float x_org = 0;
float y_org = 0;
float z_org = 0;

extern float hdr_scale;
extern unsigned int this_frame_max, this_frame_average, this_frame_total;

void R_RenderWorld (void)
{
	if (!r_drawworld->value)
		return;
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;
	
	extern unsigned short *frame_buffer_base;
	extern int frame_buffer_width;
	extern int frame_buffer_height;
	
//	ds_memset(frame_buffer_base, 0, 2 * frame_buffer_width * frame_buffer_height);

	// auto cycle the world frame for texture animation
	r_worldentity.frame = (int)(r_newrefdef.time*2);
	currententity = &r_worldentity;

	VectorCopy (r_origin, modelorg);
	
//	scaled_modelorg[0] = (short)(modelorg[0] * DS_MINI_GEOM_SCALE);
//	scaled_modelorg[1] = (short)(modelorg[1] * DS_MINI_GEOM_SCALE);
//	scaled_modelorg[2] = (short)(modelorg[2] * DS_MINI_GEOM_SCALE);
	
	psp_pushmatrix();
	
	psp_translatef(modelorg[0] * x_org, modelorg[1] * y_org, modelorg[2] * z_org);
	//psp_translatef(modelorg[0] / -DS_GEOM_SCALE, modelorg[1] / -DS_GEOM_SCALE, modelorg[2] / -DS_GEOM_SCALE);
	
	fp_modelorg[0] = (int)(modelorg[0] * 8192);
	fp_modelorg[1] = (int)(modelorg[1] * 8192);
	fp_modelorg[2] = (int)(modelorg[2] * 8192);
	
	currentmodel = r_worldmodel;
	r_pcurrentvertbase = currentmodel->vertexes;
	r_pcurrentrealvertbase = currentmodel->real_vertexes;
	
//	if (vertbase_cache == NULL)
//		vertbase_cache = (short *)ds_malloc((currentmodel->numvertexes + 8) * sizeof(short) * 3);
	
//	if (vertbase_cache)
//	{
////		printf("dma from %08x to %08x, size %d\n", r_pcurrentrealvertbase, vertbase_cache, (currentmodel->numvertexes + 8) * sizeof(short) * 3);
//		DC_FlushRange(vertbase_cache, (currentmodel->numvertexes + 8) * sizeof(short) * 3);
//		dmaCopyHalfWords(1, r_pcurrentrealvertbase, vertbase_cache, (currentmodel->numvertexes + 8) * sizeof(short) * 3);
//		DC_InvalidateRange(vertbase_cache, (currentmodel->numvertexes + 8) * sizeof(short) * 3);
//		r_pcurrentrealvertbase = vertbase_cache;
//	}
//	else
//		printf("couldn\'t allocate vertex cache\n");
//	
	rendering_is_go = 1;
	render_flags = 0;

	extern bool use_arm7_bsp;
	
	psp_polyfmt(0, 31, 0, POLY_CULL_FRONT);		//front and back
	
	head_alpha_surf = NULL;
	
	if (use_arm7_bsp == false)
	{
		if ((currentmodel->nodes->contents != CONTENTS_SOLID) && (currentmodel->nodes->visframe == r_visframecount))
			R_RecursiveWorldNode (currentmodel->nodes, 15);
		
		psp_polyfmt(1, 15, 0, POLY_CULL_NONE);
		while (head_alpha_surf)
		{
			R_RenderPoly(head_alpha_surf);
			head_alpha_surf = head_alpha_surf->nextalphasurface;
		}
		psp_polyfmt(0, 31, 0, POLY_CULL_NONE);
	}
	else
	{
		psp_setup_bsp_render((void *)currentmodel, view_clipplanes, r_framecount, r_visframecount, r_newrefdef.areabits, pfrustum_indexes, fp_modelorg, (unsigned int *)&head_alpha_surf);
	
		while (head_alpha_surf)
		{
			R_RenderPoly(head_alpha_surf);
			head_alpha_surf = head_alpha_surf->nextalphasurface;
		}
	}
	
//	printf("%08x\n", render_flags & ~SURF_SKY);
	
	if (render_flags & SURF_SKY)
	{
//		ds_scalef(8, 8, 8);
		psp_pushmatrix();
		//psp_translatef(modelorg[0] / DS_GEOM_SCALE, modelorg[1] / DS_GEOM_SCALE, modelorg[2] / DS_GEOM_SCALE);
		R_EmitSkyBox();
		psp_popmatrix();
//		printf("sky\n");
	}
	render_flags = 0;
	
#ifdef USE_HDR
	float avg = (float)this_frame_average / (this_frame_total + 1);
	
	printf("%03.2f %03.2f %d\n", hdr_scale, avg, this_frame_max);
	
#define POWER(x) ((x))
	
	avg = (avg);
	
	if (avg > 165.0f)
		hdr_scale -= 0.01f;
	
	if (avg < 155.0f)
		hdr_scale += 0.01f;
//	else if (avg < 210)
//		hdr_scale += 0.01f;
	
	if (hdr_scale < 0.80f)
		hdr_scale = 0.80f;
	else if (hdr_scale > 1.20f)
		hdr_scale = 1.20f;
	
	this_frame_max = 0;
	this_frame_average = 0;
	this_frame_total = 0;
#endif
	
//	for (int count = 0; count < surfaces_to_render; count++)
//		R_RenderPoly(render_surfs[count], 0);
//	surfaces_to_render = 0;
	
//	if (vertbase_cache)
//	{
//		ds_free(vertbase_cache);
//		vertbase_cache = NULL;
//	}
	
	psp_popmatrix();
}

