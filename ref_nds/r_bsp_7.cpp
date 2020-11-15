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

#include "../quake_ipc.h"

int fp_modelorg[3];

//replacment stuff
int *new_pfrustum_indexes[4];
byte areabits[MAX_MAP_AREAS/8];
int new_framecount;
int new_visframecount;
clipplane_t new_clipplanes[4];
model_t *new_worldmodel;

msurface_t *head_surf;

int drawn_surfs = 0;

bool R_RecursiveWorldNode (mnode_t *start_node, int start_clipflags);

void setup_bsp_render(void *model, void *clipplanes, int frc, int vfrc, byte *areab, int **pfri, int *modelo, unsigned int *head)
{
	new_worldmodel = (model_t *)model;
	memcpy(new_clipplanes, clipplanes, sizeof(new_clipplanes));
	new_framecount = frc;
	new_visframecount = vfrc;
	memcpy(areabits, areab, MAX_MAP_AREAS / 8);
	
	head_surf = NULL;
	
	for (int count = 0; count < 4; count++)
		new_pfrustum_indexes[count] = pfri[count];
	
	for (int count = 0; count < 3; count++)
		fp_modelorg[count] = modelo[count];
	
	if ((new_worldmodel->nodes->contents != CONTENTS_SOLID) && (new_worldmodel->nodes->visframe == new_visframecount))
		R_RecursiveWorldNode (new_worldmodel->nodes, 15);
	
	*head = (unsigned int)head_surf;
	
//	ARM7_PRINT_NUMBER(drawn_surfs);
	drawn_surfs = 0;
}

void R_RenderPoly(msurface_t *fa)
{
	msurface_t *temp_surface = head_surf;
	head_surf = fa;
	fa->nextalphasurface = temp_surface;
	drawn_surfs++;
}


//===========================================================================

/*
================
R_RecursiveWorldNode
================
*/

int perp_sides = 0;
int angled_sides = 0;
int clipped_faces = 0;

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

		pindex = new_pfrustum_indexes[i];

		bignormal[0] = new_clipplanes[i].normal_int[0];
		bignormal[1] = new_clipplanes[i].normal_int[1];
		bignormal[2] = new_clipplanes[i].normal_int[2];

		bigdist = new_clipplanes[i].dist_int;

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
		{
			ARM7_PRINT("we\'ve run out of fake bsp stack!\n");
			ARM7_HALT();
		}
		
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

//				if (r_newrefdef.areabits)
				{
					if (! (areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
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
						(*mark)->visframe = new_framecount;
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
				
				if (((node)->children[*side]->contents != CONTENTS_SOLID) && ((node)->children[*side]->visframe == new_visframecount))
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
				msurface_t *surf = new_worldmodel->surfaces + (node)->firstsurface;

				do
				{
					if (__builtin_expect(surf->visframe == new_framecount, 1))
					{
						if (surf->texinfo->flags & ( SURF_TRANS66 | SURF_TRANS33 ))
						{
							msurface_t *temp_surf = head_surf;
							head_surf = surf;
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

			if (((node)->children[!*side]->contents != CONTENTS_SOLID) && ((node)->children[!*side]->visframe == new_visframecount))

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
		{
			ARM7_PRINT("bsp recursion: %d is not a valid lr\n");
			ARM7_PRINT_NUMBER(sp->lr);
			ARM7_HALT();
		}
		}
	}
	return (sp + 1)->return_val;
	return true;
}

